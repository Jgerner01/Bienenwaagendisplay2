/**
 * webserver.cpp - WiFiManager + Webinterface Bienenwaage
 */

#include "webserver.h"
#include <ArduinoJson.h>
#include <Updater.h>

// ============================================================
// HTML-KONSTANTEN
// ============================================================

const char WebServerManager::HTML_END[] = "</body></html>";

// JavaScript: holt /api/data alle 2 s und aktualisiert nur kg/°C-Spans
static const char JS_LIVE_UPDATE[] PROGMEM =
    "<script>"
    "setInterval(function(){"
    "fetch('/api/data').then(function(r){return r.json();}).then(function(d){"
    "var w=document.getElementById('wv');"
    "if(w){if(d.kgOk){w.className='value ok';w.textContent=d.kg.toFixed(3)+' kg';}"
    "else{w.className='err';w.textContent='kein g\xc3\xbcltiger Wert';}}"
    "var wt=document.getElementById('wtcv');"
    "if(wt){if(d.kgOk){"
    "wt.className=d.corrActive?'value ok':'value';"
    "wt.textContent=d.kgTCorrected.toFixed(3)+' kg'+(d.corrActive?' *':'');}"
    "else{wt.className='err';wt.textContent='kein g\xc3\xbcltiger Wert';}}"
    "var wc=document.getElementById('wcv');"
    "if(wc){if(d.kgOk){"
    "wc.className=d.corrActive?'value ok':'value';"
    "wc.textContent=d.kgCorrected.toFixed(3)+' kg'+(d.corrActive?' **':'');}"
    "else{wc.className='err';wc.textContent='kein g\xc3\xbcltiger Wert';}}"
    "var ev=document.getElementById('erv');"
    "if(ev){if(d.ertragsAktiv){"
    "ev.className='value ok';"
    "var s=d.ertragsgewicht>=0?'+':'';"
    "ev.textContent=s+d.ertragsgewicht.toFixed(3)+' kg';}"
    "else{ev.className='value';ev.textContent='nicht gesetzt';}}"
    "var t=document.getElementById('tv');"
    "if(t){if(d.tempOk){t.className='value ok';t.textContent=d.tempC.toFixed(2)+'\xc2\xb0\x43';}"
    "else{t.className='err';t.textContent='kein Sensor';}}"
    "var r=document.getElementById('rv');"
    "if(r){r.textContent=d.raw;}"
    "var s=document.getElementById('sv');"
    "if(s){s.textContent=d.kgOk?(d.spread*1000).toFixed(1)+' g':'-';}"
    "var m=document.getElementById('tmv');"
    "if(m){m.textContent=d.kgOk?d.trimmedMean.toFixed(3)+' kg':'-';}"
    "}).catch(function(){});},5000);"
    "</script>";

static const char HTML_CSS[] PROGMEM = R"css(
body{font-family:system-ui,sans-serif;margin:0;padding:12px;background:#f0f2f5;color:#333}
h1{text-align:center;color:#e6a817;margin:8px 0;font-size:1.4em}
h2{color:#555;font-size:1.05em;border-bottom:2px solid #e6a817;padding-bottom:3px;margin-top:0}
.card{background:#fff;border-radius:10px;padding:14px;margin:10px 0;box-shadow:0 2px 6px rgba(0,0,0,.1)}
.btn{display:block;width:100%;padding:11px;background:#e6a817;color:#fff;border:none;border-radius:8px;
  font-size:1em;cursor:pointer;text-align:center;text-decoration:none;margin:6px 0;box-sizing:border-box}
.btn:hover{background:#c98e10}.btn.red{background:#c0392b}.btn.red:hover{background:#a93226}
.btn.grey{background:#6c757d}.btn.grey:hover{background:#5a6268}
.net{padding:9px;margin:3px 0;background:#f8f9fa;border-radius:7px;cursor:pointer;border:1px solid #ddd}
.net:hover{background:#fff8e1;border-color:#e6a817}
input[type=text],input[type=password],input[type=number],select{
  width:100%;padding:9px;margin:3px 0 10px 0;border:1px solid #ddd;border-radius:7px;
  box-sizing:border-box;font-size:1em}
.row{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid #eee;font-size:.95em}
.row:last-child{border-bottom:none}
.label{color:#666}.value{font-weight:bold}
.ok{color:#0d904f;font-weight:bold}.err{color:#c5221f;font-weight:bold}
.nav{display:flex;flex-wrap:wrap;gap:5px;margin:8px 0}
.nav a{flex:1;min-width:80px;text-align:center;padding:8px;background:#fff;border:1px solid #e6a817;
  border-radius:7px;color:#e6a817;text-decoration:none;font-size:.9em}
.nav a:hover{background:#fff8e1}
.back{color:#e6a817;text-decoration:none;display:inline-block;margin-top:6px}
)css";

// ============================================================
// KONSTRUKTOR
// ============================================================

WebServerManager::WebServerManager()
    : server(nullptr), dnsServer(nullptr), apMode(false), staConnected(false),
      scaleReaderRef(nullptr), tempSensorRef(nullptr), tempCalRef(nullptr),
      pt2CalRef(nullptr), ertragsAktivRef(nullptr), ertragsOffsetRef(nullptr),
      wifiSaveCb(nullptr), mqttSaveCb(nullptr),
      scaleSaveCb(nullptr), tempCalSaveCb(nullptr), pt2CalSaveCb(nullptr),
      tareCb(nullptr), calibrateCb(nullptr),
      displayCb(nullptr), apModeCb(nullptr), connectStartTime(0), apShutdownTime(0) {
    memset(&cachedWifiConfig, 0, sizeof(WifiConfig));
}

WebServerManager::~WebServerManager() {
    if (server)    { delete server;    server    = nullptr; }
    if (dnsServer) { delete dnsServer; dnsServer = nullptr; }
}

// ============================================================
// BEGIN
// ============================================================

bool WebServerManager::begin(ScaleReader* scaleReader) {
    scaleReaderRef = scaleReader;
    connectStartTime = millis();

    // ESP8266: eigene NVS-Credentials deaktivieren – sonst versucht der Chip
    // beim Boot selbstständig mit alten (fremden) Credentials zu verbinden
    WiFi.persistent(false);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);
    delay(100);

    // cachedWifiConfig wurde vor begin() via setWifiConfig() gesetzt
    if (strlen(cachedWifiConfig.ssid) > 0) {
        DEBUG_PRINTF("[WiFi] Verbinde mit: %s\n", cachedWifiConfig.ssid);
        WiFi.mode(WIFI_STA);
        WiFi.hostname(WIFI_HOSTNAME);
        WiFi.begin(cachedWifiConfig.ssid, cachedWifiConfig.password);

        unsigned long waitStart = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - waitStart < WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
            DEBUG_PRINT(".");
        }
        DEBUG_PRINTLN();

        if (WiFi.status() == WL_CONNECTED) {
            staConnected = true;
            apMode       = false;
            DEBUG_PRINTF("[WiFi] Verbunden! IP: %s\n", WiFi.localIP().toString().c_str());
            server = new WiFiServer(80);
            server->begin();
            if (displayCb) displayCb(WiFi.localIP().toString());
            if (apModeCb)  apModeCb(false);
            return true;
        }
        DEBUG_PRINTLN("[WiFi] Verbindung fehlgeschlagen, starte AP-Mode");
        WiFi.disconnect(true);
        delay(300);
    }

    startApMode();
    return false;
}

// ============================================================
// AP MODE
// ============================================================

void WebServerManager::startApMode() {
    apMode       = true;
    staConnected = false;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

    DEBUG_PRINTLN("[WiFi] AP-Mode: " WIFI_AP_SSID " PW: " WIFI_AP_PASSWORD);

    dnsServer = new DNSServer();
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, "*", WiFi.softAPIP());

    server = new WiFiServer(80);
    server->begin();

    if (displayCb) displayCb("192.168.4.1");
    if (apModeCb)  apModeCb(true);
}

// ============================================================
// STA MODE
// ============================================================

bool WebServerManager::startStaMode() {
    if (strlen(cachedWifiConfig.ssid) == 0) return false;

    DEBUG_PRINTF("[WiFi] AP+STA Verbinde: %s\n", cachedWifiConfig.ssid);

    // AP+STA: AP bleibt aktiv → laufende HTTP-Verbindung überlebt
    WiFi.mode(WIFI_AP_STA);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(cachedWifiConfig.ssid, cachedWifiConfig.password);

    unsigned long waitStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - waitStart < 30000UL) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        staConnected  = true;
        // apMode bleibt true – AP läuft noch (AP+STA)
        // loop() schaltet AP nach apShutdownTime ab
        apShutdownTime = millis() + 120000UL;   // AP in 2 Minuten abschalten
        DEBUG_PRINTF("[WiFi] STA verbunden: %s\n", WiFi.localIP().toString().c_str());
        if (displayCb) displayCb(WiFi.localIP().toString());
        if (apModeCb)  apModeCb(false);
        return true;
    }

    // Verbindung fehlgeschlagen → zurück zu reinem AP-Mode
    DEBUG_PRINTLN("[WiFi] STA fehlgeschlagen, bleibe in AP-Mode");
    WiFi.mode(WIFI_AP);
    connectStartTime = millis();
    return false;
}

// ============================================================
// LOOP
// ============================================================

void WebServerManager::loop() {
    // AP nach erfolgreicher STA-Verbindung abschalten (AP+STA → STA)
    if (staConnected && apMode && apShutdownTime > 0 && millis() > apShutdownTime) {
        apMode         = false;
        apShutdownTime = 0;
        if (dnsServer) { dnsServer->stop(); delete dnsServer; dnsServer = nullptr; }
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        DEBUG_PRINTLN("[WiFi] AP abgeschaltet (STA aktiv)");
    }

    // Ohne STA: nach Timeout sicherstellen dass AP läuft
    if (!staConnected && millis() - connectStartTime > AP_FALLBACK_TIMEOUT_MS) {
        if (!apMode) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(300);
            startApMode();
        }
        connectStartTime = millis();
    }

    if (dnsServer) dnsServer->processNextRequest();

    if (server) {
        WiFiClient client = server->accept();
        if (client) {
            parseRequest(client);
            client.stop();
        }
    }
}

String WebServerManager::getIp() const {
    if (staConnected) return WiFi.localIP().toString();
    if (apMode)       return WiFi.softAPIP().toString();
    return "N/A";
}

// ============================================================
// REQUEST PARSER
// ============================================================

void WebServerManager::parseRequest(WiFiClient& client) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    int sp1 = line.indexOf(' ');
    int sp2 = line.lastIndexOf(' ');
    String method = line.substring(0, sp1);
    String path   = line.substring(sp1 + 1, sp2);

    int contentLength = 0;
    while (true) {
        String h = client.readStringUntil('\n');
        h.trim();
        if (h.length() == 0) break;
        if (h.startsWith("Content-Length:")) contentLength = h.substring(16).toInt();
    }

    DEBUG_PRINTF("[HTTP] %s %s\n", method.c_str(), path.c_str());

    // OTA-Upload: Firmware direkt in Flash streamen, NICHT in String puffern
    if (path == "/ota" && method == "POST") {
        handleOtaUpload(client, contentLength);
        return;
    }

    String body;
    if (contentLength > 0) {
        unsigned long t = millis();
        while (client.available() < contentLength && millis() - t < 3000) delay(5);
        for (int i = 0; i < contentLength && client.available(); i++)
            body += (char)client.read();
    }

    if (path == "/" || path == "/generate_204" || path == "/fwlink") {
        handleRoot(client);
    } else if (path == "/scale") {
        handleScale(client);
    } else if (path == "/tare" && method == "POST") {
        handleTare(client);
    } else if (path == "/ertragstara" && method == "POST") {
        handleErtragsTara(client);
    } else if (path == "/ertragstara-reset" && method == "POST") {
        handleErtragsTaraReset(client);
    } else if (path == "/calibrate" && method == "GET") {
        handleCalibratePage(client);
    } else if (path == "/calibrate" && method == "POST") {
        handleCalibratePost(client, body);
    } else if (path == "/reset-scale" && method == "POST") {
        handleResetScale(client);
    } else if (path == "/gain" && method == "GET") {
        handleGainPage(client);
    } else if (path == "/gain" && method == "POST") {
        handleGainPost(client, body);
    } else if (path == "/mqtt" && method == "GET") {
        handleMqttPage(client);
    } else if (path == "/mqtt" && method == "POST") {
        handleMqttPost(client, body);
    } else if (path == "/scan") {
        handleWifiScan(client);
    } else if (path == "/save" && method == "POST") {
        handleWifiSave(client, body);
    } else if (path == "/status") {
        handleStatus(client);
    } else if (path == "/reboot" && method == "POST") {
        handleReboot(client);
    } else if (path == "/api/data") {
        handleApiData(client);
    } else if (path == "/params") {
        handleParams(client);
    } else if (path == "/params/tempcal" && method == "POST") {
        handleParamsTempCalPost(client, body);
    } else if (path == "/params/pt2cal" && method == "POST") {
        handleParamsPT2CalPost(client, body);
    } else if (path == "/tempcal") {
        handleTempCalPage(client);
    } else if (path == "/pt2cal") {
        handlePT2CalPage(client);
    } else if (path == "/api/tempcal" && method == "GET") {
        handleTempCalApiGet(client);
    } else if (path == "/api/tempcal" && method == "POST") {
        handleTempCalApiPost(client, body);
    } else if (path == "/api/pt2cal" && method == "GET") {
        handlePT2CalApiGet(client);
    } else if (path == "/api/pt2cal" && method == "POST") {
        handlePT2CalApiPost(client, body);
    } else if (path == "/ota") {
        handleOtaPage(client);
    } else {
        // Captive Portal Fallback
        sendRedirect(client, "http://192.168.4.1/");
    }
}

// ============================================================
// HTML HELPER
// ============================================================

String WebServerManager::htmlHead(const String& title, uint16_t refreshSec) {
    String h;
    h.reserve(900);
    h += F("<!DOCTYPE html><html lang='de'><head>"
           "<meta charset='UTF-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>");
    if (refreshSec > 0) {
        h += F("<meta http-equiv='refresh' content='");
        h += refreshSec;
        h += F("'>");
    }
    h += F("<title>");
    h += title;
    h += F("</title><style>");
    h += FPSTR(HTML_CSS);
    h += F("</style></head><body>");
    return h;
}

void WebServerManager::sendHtml(WiFiClient& client, int code, const String& body) {
    client.printf("HTTP/1.1 %d OK\r\n", code);
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println("Cache-Control: no-cache");
    client.println();
    client.print(body);
}

void WebServerManager::sendRedirect(WiFiClient& client, const char* location) {
    client.println("HTTP/1.1 302 Found");
    client.print("Location: "); client.println(location);
    client.println("Connection: close");
    client.println();
}

// ============================================================
// SEITE: ROOT "/"
// ============================================================

void WebServerManager::handleRoot(WiFiClient& client) {
    const ScaleData& d = scaleReaderRef->getData();

    String html = htmlHead("Bienenwaage");
    html += "<h1>&#x1F41D; Bienenwaage</h1>";

    // WiFi-Status
    html += "<div class='card'>";
    if (staConnected) {
        html += "<div class='row'><span class='label'>WiFi</span>"
                "<span class='ok'>verbunden</span></div>";
        html += "<div class='row'><span class='label'>IP</span>"
                "<span class='value'>" + WiFi.localIP().toString() + "</span></div>";
        html += "<div class='row'><span class='label'>SSID</span>"
                "<span class='value'>" + WiFi.SSID() + "</span></div>";
    } else {
        html += "<div class='row'><span class='label'>WiFi</span>"
                "<span class='err'>AP-Mode</span></div>";
        html += "<div class='row'><span class='label'>AP-IP</span>"
                "<span class='value'>192.168.4.1</span></div>";
    }
    html += "</div>";

    // Messwerte
    html += "<div class='card'><h2>Messwerte</h2>";
    if (d.isValid) {
        char wbuf[20];
        snprintf(wbuf, sizeof(wbuf), "%.3f kg", d.weightKg);
        html += "<div class='row'><span class='label'>Gewicht (roh)</span>"
                "<span id='wv' class='value ok'>" + String(wbuf) + "</span></div>";
        char wtbuf[24];
        snprintf(wtbuf, sizeof(wtbuf), "%.3f kg%s",
                 d.weightTCorrectedKg, d.tempCorrectionActive ? " *" : "");
        html += "<div class='row'><span class='label'>Gewicht (T-korr.)</span>"
                "<span id='wtcv' class='" +
                String(d.tempCorrectionActive ? "value ok" : "value") + "'>" +
                String(wtbuf) + "</span></div>";
        char wcbuf[24];
        snprintf(wcbuf, sizeof(wcbuf), "%.3f kg%s",
                 d.weightCorrectedKg, d.tempCorrectionActive ? " **" : "");
        html += "<div class='row'><span class='label'>Gewicht (T+PT2-korr.)</span>"
                "<span id='wcv' class='" +
                String(d.tempCorrectionActive ? "value ok" : "value") + "'>" +
                String(wcbuf) + "</span></div>";
        // Ertragswert
        {
            bool   ea  = ertragsAktivRef  && *ertragsAktivRef;
            float  eo  = ertragsOffsetRef ? *ertragsOffsetRef : 0.0f;
            String ecls = ea ? "value ok" : "value";
            String eval;
            if (ea) {
                char ebuf[20];
                float ev = d.weightCorrectedKg - eo;
                snprintf(ebuf, sizeof(ebuf), "%+.3f kg", ev);
                eval = String(ebuf);
            } else {
                eval = "nicht gesetzt";
            }
            html += "<div class='row'><span class='label'>Ertragswert</span>"
                    "<span id='erv' class='" + ecls + "'>" + eval + "</span></div>";
        }
        char sbuf[20];
        snprintf(sbuf, sizeof(sbuf), "%.1f g", d.spreadKg * 1000.0f);
        html += "<div class='row'><span class='label'>&#x03C3; Streuung</span>"
                "<span id='sv' class='value'>" + String(sbuf) + "</span></div>";
        char tbuf2[20];
        snprintf(tbuf2, sizeof(tbuf2), "%.3f kg", d.trimmedMeanKg);
        html += "<div class='row'><span class='label'>Trimmed Mean</span>"
                "<span id='tmv' class='value'>" + String(tbuf2) + "</span></div>";
    } else {
        html += "<div class='row'><span class='label'>Gewicht (roh)</span>"
                "<span id='wv' class='err'>kein g&uuml;ltiger Wert</span></div>";
        html += "<div class='row'><span class='label'>Gewicht (T-korr.)</span>"
                "<span id='wtcv' class='err'>kein g&uuml;ltiger Wert</span></div>";
        html += "<div class='row'><span class='label'>Gewicht (T+PT2-korr.)</span>"
                "<span id='wcv' class='err'>kein g&uuml;ltiger Wert</span></div>";
        html += "<div class='row'><span class='label'>&#x03C3; Streuung</span>"
                "<span id='sv' class='value'>-</span></div>";
        html += "<div class='row'><span class='label'>Trimmed Mean</span>"
                "<span id='tmv' class='value'>-</span></div>";
    }
    if (tempSensorRef) {
        const TempData& t = tempSensorRef->getData();
        if (t.isValid) {
            char tbuf[20];
            snprintf(tbuf, sizeof(tbuf), "%.2f &deg;C", t.tempC);
            html += "<div class='row'><span class='label'>Temperatur</span>"
                    "<span id='tv' class='value ok'>" + String(tbuf) + "</span></div>";
        } else {
            html += "<div class='row'><span class='label'>Temperatur</span>"
                    "<span id='tv' class='err'>kein Sensor</span></div>";
        }
    }
    html += "<div class='row'><span class='label'>HX711</span>"
            "<span class='" + String(d.hx711Ready ? "ok'>bereit" : "err'>nicht bereit") +
            "</span></div>";
    if (d.tempCorrectionActive)
        html += "<div class='row'><span class='label'></span>"
                "<span class='value' style='font-size:.85em'>* T-Korrektur aktiv</span></div>";
    html += "</div>";

    // Navigation
    html += "<div class='nav'>"
            "<a href='/scale'>&#x2696; Waage</a>"
            "<a href='/params'>&#x2699; Parameter</a>"
            "<a href='/mqtt'>&#x1F4E1; MQTT</a>"
            "<a href='/scan'>&#x1F4F6; WiFi</a>"
            "<a href='/ota'>&#x2B06; OTA</a>"
            "</div>";

    html += "<div class='card'>"
            "<form method='post' action='/reboot'>"
            "<button class='btn red' type='submit'>&#x21BB; Neustart</button>"
            "</form></div>";
    html += FPSTR(JS_LIVE_UPDATE);
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /scale - HX711 Detaildaten
// ============================================================

void WebServerManager::handleScale(WiFiClient& client) {
    const ScaleData& d = scaleReaderRef->getData();

    String html = htmlHead("Waage - Details");
    html += "<h1>&#x2696; Waage</h1>";

    html += "<div class='card'><h2>HX711 Messwerte</h2>";

    auto row = [&](const char* lbl, const String& val) {
        html += "<div class='row'><span class='label'>" + String(lbl) +
                "</span><span class='value'>" + val + "</span></div>";
    };

    if (d.isValid) {
        char wbuf[20]; snprintf(wbuf, sizeof(wbuf), "%.3f kg", d.weightKg);
        html += "<div class='row'><span class='label'>Gewicht (roh)</span>"
                "<span id='wv' class='value ok'>" + String(wbuf) + "</span></div>";
        char wcbuf[24];
        snprintf(wcbuf, sizeof(wcbuf), "%.3f kg%s",
                 d.weightCorrectedKg, d.tempCorrectionActive ? " *" : "");
        html += "<div class='row'><span class='label'>Gewicht (korrigiert)</span>"
                "<span id='wcv' class='" +
                String(d.tempCorrectionActive ? "value ok" : "value") + "'>" +
                String(wcbuf) + "</span></div>";
        char sbuf[20]; snprintf(sbuf, sizeof(sbuf), "%.1f g", d.spreadKg * 1000.0f);
        html += "<div class='row'><span class='label'>&#x03C3; Streuung</span>"
                "<span id='sv' class='value'>" + String(sbuf) + "</span></div>";
        char tmbuf[20]; snprintf(tmbuf, sizeof(tmbuf), "%.3f kg", d.trimmedMeanKg);
        html += "<div class='row'><span class='label'>Trimmed Mean</span>"
                "<span id='tmv' class='value'>" + String(tmbuf) + "</span></div>";
    } else {
        html += "<div class='row'><span class='label'>Gewicht (roh)</span>"
                "<span id='wv' class='err'>ungültig</span></div>";
        html += "<div class='row'><span class='label'>Gewicht (korrigiert)</span>"
                "<span id='wcv' class='err'>ungültig</span></div>";
        html += "<div class='row'><span class='label'>&#x03C3; Streuung</span>"
                "<span id='sv' class='value'>-</span></div>";
        html += "<div class='row'><span class='label'>Trimmed Mean</span>"
                "<span id='tmv' class='value'>-</span></div>";
    }
    if (d.tempCorrectionActive) {
        html += "<div class='row'><span class='label'></span>"
                "<span class='value' style='font-size:.85em'>* T-Korrektur aktiv</span></div>";
    }

    if (tempSensorRef) {
        const TempData& t = tempSensorRef->getData();
        if (t.isValid) {
            char tbuf[20]; snprintf(tbuf, sizeof(tbuf), "%.2f &deg;C", t.tempC);
            html += "<div class='row'><span class='label'>Temperatur</span>"
                    "<span id='tv' class='value ok'>" + String(tbuf) + "</span></div>";
        } else {
            html += "<div class='row'><span class='label'>Temperatur</span>"
                    "<span id='tv' class='err'>kein Sensor</span></div>";
        }
    }

    html += "<div class='row'><span class='label'>Rohwert</span>"
            "<span id='rv' class='value'>" + String(d.rawValue) + "</span></div>";
    row("Offset (Tara)",    String(d.offset));

    char fbuf[20]; snprintf(fbuf, sizeof(fbuf), "%.2f", d.calibrationFactor);
    row("Kalibrierfaktor",  String(fbuf));

    String gainStr;
    if      (d.gain == 128) gainStr = "128 (Kanal A)";
    else if (d.gain == 64)  gainStr = "64 (Kanal A)";
    else                    gainStr = "32 (Kanal B)";
    row("Gain-Faktor", gainStr);

    html += "<div class='row'><span class='label'>HX711 bereit</span>"
            "<span class='" + String(d.hx711Ready ? "ok'>Ja" : "err'>Nein") +
            "</span></div>";

    if (d.timestamp > 0) {
        unsigned long ago = (millis() - d.timestamp) / 1000UL;
        row("Letzte Messung", "vor " + String(ago) + " s");
    }
    html += "</div>";

    // Tara-Button
    html += "<div class='card'><h2>Tara</h2>"
            "<p>Leere die Waage und setze den Nullpunkt.</p>"
            "<form method='post' action='/tare'>"
            "<button class='btn' type='submit'>&#x2295; Tara setzen</button>"
            "</form></div>";

    // Ertragsmessung
    {
        bool  ea = ertragsAktivRef  && *ertragsAktivRef;
        float eo = ertragsOffsetRef ? *ertragsOffsetRef : 0.0f;
        html += "<div class='card'><h2>&#x1F4C8; Ertragsmessung</h2>";
        if (ea && d.isValid) {
            char ebuf[24];
            snprintf(ebuf, sizeof(ebuf), "%+.3f kg", d.weightCorrectedKg - eo);
            html += "<div class='row'><span class='label'>Ertragswert</span>"
                    "<span id='erv2' class='value ok'>" + String(ebuf) + "</span></div>";
            char obuf[24];
            snprintf(obuf, sizeof(obuf), "%.3f kg", eo);
            html += "<div class='row'><span class='label'>Referenz (gesetzt bei)</span>"
                    "<span class='value'>" + String(obuf) + "</span></div>";
        } else {
            html += "<div class='row'><span class='label'>Ertragswert</span>"
                    "<span id='erv2' class='value'>nicht gesetzt</span></div>";
        }
        html += "<p style='font-size:.88em;color:#666;margin:6px 0'>"
                "Setzt das aktuelle korrigierte Gewicht als Ertragsbasis.<br>"
                "Der Ertragswert zeigt die Ver&auml;nderung seit diesem Zeitpunkt.</p>"
                "<form method='post' action='/ertragstara'>"
                "<button class='btn' type='submit'>&#x1F4CC; Ertragsbasis jetzt setzen</button>"
                "</form>";
        if (ea) {
            html += "<form method='post' action='/ertragstara-reset'>"
                    "<button class='btn grey' type='submit'>&#x274C; Ertragsmessung zur&uuml;cksetzen</button>"
                    "</form>";
        }
        html += "</div>";
    }

    html += "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>";
    html += FPSTR(JS_LIVE_UPDATE);
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /tare
// ============================================================

void WebServerManager::handleTare(WiFiClient& client) {
    if (tareCb) tareCb();

    String html = htmlHead("Tara");
    html += "<h1>Tara gesetzt</h1>"
            "<div class='card'><p class='ok'>Nullpunkt wurde gesetzt.</p></div>"
            "<div class='card'><meta http-equiv='refresh' content='2;url=/scale'>"
            "<a class='back' href='/scale'>&#x2190; Zur&uuml;ck</a></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

void WebServerManager::handleErtragsTara(WiFiClient& client) {
    if (ertragsAktivRef && ertragsOffsetRef && scaleReaderRef) {
        *ertragsOffsetRef = scaleReaderRef->getData().weightCorrectedKg;
        *ertragsAktivRef  = true;
        DEBUG_PRINTF("[Web] Ertragstara gesetzt: %.3f kg\n", *ertragsOffsetRef);
    }
    String html = htmlHead("Ertragsmessung");
    html += "<h1>&#x1F4C8; Ertragsbasis gesetzt</h1>"
            "<div class='card'><p class='ok'>Ertragsbasis wurde auf das aktuelle Gewicht gesetzt.</p></div>"
            "<meta http-equiv='refresh' content='2;url=/scale'>"
            "<a class='back' href='/scale'>&#x2190; Zur&uuml;ck</a>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

void WebServerManager::handleErtragsTaraReset(WiFiClient& client) {
    if (ertragsAktivRef) *ertragsAktivRef = false;
    DEBUG_PRINTLN("[Web] Ertragsmessung zurückgesetzt");
    String html = htmlHead("Ertragsmessung");
    html += "<h1>Ertragsmessung zur&uuml;ckgesetzt</h1>"
            "<div class='card'><p class='ok'>Ertragsmessung wurde deaktiviert.</p></div>"
            "<meta http-equiv='refresh' content='2;url=/scale'>"
            "<a class='back' href='/scale'>&#x2190; Zur&uuml;ck</a>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /calibrate GET
// ============================================================

void WebServerManager::handleCalibratePage(WiFiClient& client) {
    const ScaleData& d = scaleReaderRef->getData();

    String html = htmlHead("Kalibrierung");
    html += "<h1>&#x1F4D0; Kalibrierung</h1>";

    html += "<div class='card'><h2>Anleitung</h2>"
            "<ol><li>Leere die Waage vollst&auml;ndig.</li>"
            "<li>Gehe zu <a href='/scale'>Waage</a> und setze Tara.</li>"
            "<li>Lege ein bekanntes Gewicht auf.</li>"
            "<li>Trage das Gewicht unten ein und klicke Kalibrieren.</li></ol>"
            "</div>";

    char fbuf[20];
    snprintf(fbuf, sizeof(fbuf), "%.2f", d.calibrationFactor);

    html += "<div class='card'><h2>Bekanntes Gewicht</h2>"
            "<form method='post' action='/calibrate'>"
            "<label>Gewicht (kg):</label>"
            "<input type='number' name='weight' step='0.001' min='0.1' required "
            "placeholder='z.B. 1.000'>"
            "<button class='btn' type='submit'>&#x2714; Kalibrieren</button>"
            "</form></div>";

    html += "<div class='card'><h2>Aktueller Kalibrierfaktor</h2>"
            "<div class='row'><span class='label'>Faktor</span>"
            "<span class='value'>" + String(fbuf) + "</span></div>"
            "<div class='row'><span class='label'>Rohwert</span>"
            "<span class='value'>" + String(d.rawValue) + "</span></div>"
            "</div>";

    html += "<div class='card'><h2>Zur&uuml;cksetzen</h2>"
            "<p>Setzt Kalibrierfaktor, Offset und Gain auf Werkseinstellungen zur&uuml;ck.</p>"
            "<form method='post' action='/reset-scale'>"
            "<button class='btn red' type='submit'>&#x21BA; Auf Werkseinstellungen zur&uuml;cksetzen</button>"
            "</form></div>";

    html += "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /calibrate
// ============================================================

void WebServerManager::handleCalibratePost(WiFiClient& client, const String& body) {
    String wStr = getRequestParam(body, "weight");
    wStr        = urlDecode(wStr);
    float kg    = wStr.toFloat();

    bool ok = false;
    if (kg > 0.0f && calibrateCb) {
        ok = calibrateCb(kg);
    }

    const ScaleData& d = scaleReaderRef->getData();
    char fbuf[20];
    snprintf(fbuf, sizeof(fbuf), "%.2f", d.calibrationFactor);

    String html = htmlHead("Kalibrierung");
    html += "<h1>&#x1F4D0; Kalibrierung</h1><div class='card'>";
    if (ok) {
        html += "<p class='ok'>&#x2714; Kalibrierung erfolgreich!</p>"
                "<div class='row'><span class='label'>Bekanntes Gewicht</span>"
                "<span class='value'>" + String(kg, 3) + " kg</span></div>"
                "<div class='row'><span class='label'>Neuer Faktor</span>"
                "<span class='value'>" + String(fbuf) + "</span></div>";
    } else {
        html += "<p class='err'>&#x2718; Kalibrierung fehlgeschlagen. "
                "Gew&uuml;nschter Wert ung&uuml;ltig oder HX711 nicht bereit.</p>";
    }
    html += "</div><div class='card'>"
            "<a class='btn' href='/calibrate'>&#x21BB; Nochmal</a>"
            "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /reset-scale – Werkseinstellungen wiederherstellen
// ============================================================

void WebServerManager::handleResetScale(WiFiClient& client) {
    // Standardwerte setzen
    ScaleConfig defaults;
    defaults.calibrationFactor = 1.0f;
    defaults.offset            = 0;
    defaults.gain              = HX711_DEFAULT_GAIN;
    defaults.publishInterval   = MQTT_PUBLISH_INTERVAL / 1000;

    if (scaleReaderRef) {
        scaleReaderRef->setGain(defaults.gain);
        // Offset und Faktor direkt im HX711 zurücksetzen
        scaleReaderRef->resetToDefaults();
    }
    if (scaleSaveCb) scaleSaveCb(defaults);

    String html = htmlHead("Werkseinstellungen");
    html += "<h1>&#x21BA; Zur&uuml;ckgesetzt</h1>"
            "<div class='card'><p class='ok'>Alle Waagen-Werte wurden zur&uuml;ckgesetzt.</p>"
            "<div class='row'><span class='label'>Kalibrierfaktor</span><span class='value'>1.00</span></div>"
            "<div class='row'><span class='label'>Offset</span><span class='value'>0</span></div>"
            "<div class='row'><span class='label'>Gain</span><span class='value'>" +
            String(HX711_DEFAULT_GAIN) + "</span></div></div>"
            "<div class='card'><meta http-equiv='refresh' content='2;url=/calibrate'>"
            "<a class='back' href='/calibrate'>&#x2190; Zur&uuml;ck</a></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /gain GET
// ============================================================

void WebServerManager::handleGainPage(WiFiClient& client) {
    const ScaleData& d = scaleReaderRef->getData();

    String html = htmlHead("Gain-Faktor");
    html += "<h1>&#x1F4CA; Gain-Faktor</h1>";

    html += "<div class='card'><h2>HX711 Verst&auml;rkungsfaktor</h2>"
            "<p>Der Gain-Faktor bestimmt Eingangskanal und Verst&auml;rkung des HX711.</p>"
            "<table width='100%' style='font-size:.9em;border-collapse:collapse'>"
            "<tr style='background:#f0f2f5'><th align='left'>Gain</th><th align='left'>Kanal</th>"
            "<th align='left'>Bemerkung</th></tr>"
            "<tr><td>128</td><td>A</td><td>Standard (h&ouml;chste Aufl&ouml;sung)</td></tr>"
            "<tr><td>64</td><td>A</td><td>Alternative f&uuml;r Kanal A</td></tr>"
            "<tr><td>32</td><td>B</td><td>Zweiter Eingang (geringere Aufl&ouml;sung)</td></tr>"
            "</table></div>";

    String sel128 = (d.gain == 128) ? " selected" : "";
    String sel64  = (d.gain == 64)  ? " selected" : "";
    String sel32  = (d.gain == 32)  ? " selected" : "";

    html += "<div class='card'>"
            "<form method='post' action='/gain'>"
            "<label>Gain-Faktor ausw&auml;hlen:</label>"
            "<select name='gain'>"
            "<option value='128'" + sel128 + ">128 – Kanal A (Standard)</option>"
            "<option value='64'"  + sel64  + ">64 – Kanal A</option>"
            "<option value='32'"  + sel32  + ">32 – Kanal B</option>"
            "</select>"
            "<button class='btn' type='submit'>&#x2714; Gain speichern</button>"
            "</form></div>";

    html += "<div class='card'><h2>Aktuell gesetzt</h2>"
            "<div class='row'><span class='label'>Gain</span>"
            "<span class='value'>" + String(d.gain) + "</span></div></div>";

    html += "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /gain
// ============================================================

void WebServerManager::handleGainPost(WiFiClient& client, const String& body) {
    String gStr = getRequestParam(body, "gain");
    int    gain = gStr.toInt();

    if (gain != 32 && gain != 64 && gain != 128) gain = 128;

    // Gain in ScaleReader setzen
    if (scaleReaderRef) scaleReaderRef->setGain((uint8_t)gain);

    // Konfiguration speichern
    if (scaleSaveCb) {
        ScaleData d = scaleReaderRef->getData();
        ScaleConfig sc;
        sc.calibrationFactor = d.calibrationFactor;
        sc.offset            = d.offset;
        sc.gain              = (uint8_t)gain;
        sc.publishInterval   = MQTT_PUBLISH_INTERVAL / 1000;
        scaleSaveCb(sc);
    }

    String gainName = (gain == 32) ? "32 (Kanal B)" :
                      (gain == 64) ? "64 (Kanal A)" : "128 (Kanal A)";

    String html = htmlHead("Gain gespeichert");
    html += "<h1>&#x1F4CA; Gain gespeichert</h1>"
            "<div class='card'><p class='ok'>Gain-Faktor auf <strong>" +
            gainName + "</strong> gesetzt.</p></div>"
            "<div class='card'><meta http-equiv='refresh' content='2;url=/gain'>"
            "<a class='back' href='/gain'>&#x2190; Zur&uuml;ck</a></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /mqtt GET
// ============================================================

void WebServerManager::handleMqttPage(WiFiClient& client) {
    MqttConfig mqttCfg;
    ConfigManager cfg;
    cfg.begin();
    cfg.loadMqttConfig(mqttCfg);

    String html = htmlHead("MQTT Konfiguration");
    html += "<h1>&#x1F4E1; MQTT</h1>";
    html += "<div class='card'>"
            "<form method='post' action='/mqtt'>"
            "<label>Broker (IP oder Hostname):</label>"
            "<input type='text' name='broker' value='" + String(mqttCfg.broker) + "'>"
            "<label>Port:</label>"
            "<input type='number' name='port' value='" + String(mqttCfg.port) + "' min='1' max='65535'>"
            "<label>Benutzername (optional):</label>"
            "<input type='text' name='username' value='" + String(mqttCfg.username) + "'>"
            "<label>Passwort (optional):</label>"
            "<input type='password' name='mqttpw' value='" + String(mqttCfg.password) + "'>"
            "<label>Client-ID:</label>"
            "<input type='text' name='clientId' value='" + String(mqttCfg.clientId) + "'>"
            "<label>Topic-Prefix:</label>"
            "<input type='text' name='topicPrefix' value='" + String(mqttCfg.topicPrefix) + "'>"
            "<label>Publish-Intervall (Sekunden):</label>"
            "<input type='number' name='interval' value='" + String(mqttCfg.publishInterval) +
            "' min='5' max='3600'>"
            "<label>MQTT aktiviert:</label>"
            "<select name='enabled'>"
            "<option value='1'" + String(mqttCfg.enabled ? " selected" : "") + ">Ja</option>"
            "<option value='0'" + String(!mqttCfg.enabled ? " selected" : "") + ">Nein</option>"
            "</select>"
            "<label>HA Auto-Discovery:</label>"
            "<select name='discovery'>"
            "<option value='1'" + String(mqttCfg.autoDiscovery ? " selected" : "") + ">Ja</option>"
            "<option value='0'" + String(!mqttCfg.autoDiscovery ? " selected" : "") + ">Nein</option>"
            "</select>"
            "<button class='btn' type='submit'>&#x1F4BE; Speichern</button>"
            "</form></div>";
    html += "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /mqtt
// ============================================================

void WebServerManager::handleMqttPost(WiFiClient& client, const String& body) {
    MqttConfig cfg;
    memset(&cfg, 0, sizeof(MqttConfig));

    String broker  = urlDecode(getRequestParam(body, "broker"));
    String port    = getRequestParam(body, "port");
    String user    = urlDecode(getRequestParam(body, "username"));
    String pw      = urlDecode(getRequestParam(body, "mqttpw"));
    String cid     = urlDecode(getRequestParam(body, "clientId"));
    String prefix  = urlDecode(getRequestParam(body, "topicPrefix"));
    String ivl     = getRequestParam(body, "interval");
    String enabled = getRequestParam(body, "enabled");
    String disc    = getRequestParam(body, "discovery");

    strncpy(cfg.broker,      broker.c_str(),  sizeof(cfg.broker)      - 1);
    strncpy(cfg.username,    user.c_str(),    sizeof(cfg.username)    - 1);
    strncpy(cfg.password,    pw.c_str(),      sizeof(cfg.password)    - 1);
    strncpy(cfg.clientId,    cid.c_str(),     sizeof(cfg.clientId)    - 1);
    strncpy(cfg.topicPrefix, prefix.c_str(),  sizeof(cfg.topicPrefix) - 1);

    cfg.port            = port.toInt()  ? port.toInt()  : MQTT_DEFAULT_PORT;
    cfg.publishInterval = ivl.toInt()   ? ivl.toInt()   : 60;
    cfg.enabled         = (enabled == "1");
    cfg.autoDiscovery   = (disc    == "1");

    if (mqttSaveCb) mqttSaveCb(cfg);

    String html = htmlHead("MQTT gespeichert");
    html += "<h1>&#x1F4E1; MQTT gespeichert</h1>"
            "<div class='card'><p class='ok'>Einstellungen wurden gespeichert.</p>"
            "<p>Ein Neustart &uuml;bernimmt die neuen Einstellungen vollst&auml;ndig.</p></div>"
            "<div class='card'>"
            "<a class='btn' href='/'>&#x1F3E0; Zur Startseite</a></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /scan - WiFi-Netzwerke
// ============================================================

void WebServerManager::handleWifiScan(WiFiClient& client) {
    int n = WiFi.scanNetworks();

    String html = htmlHead("WiFi Netzwerke");
    html += "<h1>&#x1F4F6; WiFi Netzwerke</h1>";
    html += "<div class='card'><h2>Gefunden (" + String(n) + ")</h2>";

    for (int i = 0; i < n; i++) {
        bool dup = false;
        for (int j = 0; j < i; j++) { if (WiFi.SSID(j) == WiFi.SSID(i)) { dup = true; break; } }
        if (dup) continue;

        String ssid = WiFi.SSID(i);
        String enc  = ssid;
        enc.replace("%", "%25"); enc.replace(" ", "%20");
        enc.replace("+", "%2B"); enc.replace("#", "%23");

        int  bars = map(constrain(WiFi.RSSI(i), -100, -30), -100, -30, 1, 4);
        bool lock = WiFi.encryptionType(i) != ENC_TYPE_NONE;

        String sig;
        for (int b = 0; b < 4; b++) sig += (b < bars) ? "&#x2588;" : "&#x2591;";

        html += "<div class='net' onclick=\"document.getElementById('ssid').value='"
                + enc + "';document.getElementById('pw').focus()\">"
                "<strong>" + ssid + "</strong>&nbsp;<small>" + sig + " " +
                String(WiFi.RSSI(i)) + " dBm</small><br>"
                "<small>" + String(lock ? "&#x1F512; Gesichert" : "&#x1F513; Offen") + "</small></div>";
    }
    html += "</div>";

    String currentSsid = String(cachedWifiConfig.ssid);
    currentSsid.replace("'", "\\'");   // einfache Anführungszeichen im SSID escapen
    html += "<div class='card'><h2>Verbinden</h2>"
            "<form action='/save' method='post'>"
            "<label>SSID:</label>"
            "<input type='text' id='ssid' name='ssid' required value='" + currentSsid + "'>"
            "<label>Passwort:</label>"
            "<input type='password' id='pw' name='password' placeholder='(leer lassen = unver&auml;ndert)'>"
            "<button class='btn' type='submit'>&#x1F4F6; Verbinden</button>"
            "</form>"
            "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
    WiFi.scanDelete();
}

// ============================================================
// POST: /save - WiFi speichern
// ============================================================

void WebServerManager::handleWifiSave(WiFiClient& client, const String& body) {
    String ssid = urlDecode(getRequestParam(body, "ssid"));
    String pw   = urlDecode(getRequestParam(body, "password"));

    // Passwort beibehalten wenn Feld leer gelassen wurde
    String effectivePw = pw.length() > 0 ? pw : String(cachedWifiConfig.password);
    memset(&cachedWifiConfig, 0, sizeof(WifiConfig));
    strncpy(cachedWifiConfig.ssid,     ssid.c_str(),        sizeof(cachedWifiConfig.ssid) - 1);
    strncpy(cachedWifiConfig.password, effectivePw.c_str(), sizeof(cachedWifiConfig.password) - 1);
    cachedWifiConfig.dhcp = true;

    // Einmalig speichern – nur über den Callback (vermeidet doppeltes LittleFS-Begin)
    if (wifiSaveCb) wifiSaveCb(cachedWifiConfig);

    // AP+STA: Verbindung aufbauen während AP noch läuft → TCP-Verbindung bleibt erhalten
    bool connected = startStaMode();

    String html = htmlHead("WiFi Verbindung");
    html.reserve(600);

    if (connected) {
        String newIp = WiFi.localIP().toString();
        html += F("<h1>&#x2714; Verbunden!</h1>");
        html += "<div class='card'>"
                "<div class='row'><span class='label'>Netzwerk</span>"
                "<span class='value ok'>" + ssid + "</span></div>"
                "<div class='row'><span class='label'>IP-Adresse</span>"
                "<span class='value'><strong>" + newIp + "</strong></span></div>"
                "</div>"
                "<div class='card'>"
                "<p>Wechseln Sie zu Ihrem Heimnetzwerk <strong>" + ssid + "</strong>"
                " und &ouml;ffnen Sie:</p>"
                "<a class='btn' href='http://" + newIp + "/'>http://" + newIp + "/</a>"
                "<p><small>IP-Adresse auch auf dem LCD-Display sichtbar.<br>"
                "Dieser Konfigurationspunkt (192.168.4.1) bleibt noch 2&nbsp;Minuten aktiv.</small></p>"
                "</div>";
    } else {
        html += F("<h1>&#x2718; Verbindung fehlgeschlagen</h1>");
        html += "<div class='card'>"
                "<p class='err'>Konnte nicht mit <strong>" + ssid + "</strong> verbinden.</p>"
                "<p>Bitte SSID und Passwort pr&uuml;fen.</p>"
                "</div>"
                "<div class='card'>"
                "<a class='btn' href='/scan'>&#x1F50D; Nochmal versuchen</a>"
                "</div>";
    }
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /status
// ============================================================

void WebServerManager::handleStatus(WiFiClient& client) {
    String html = htmlHead("Verbindungsstatus");
    html += "<h1>Verbindungsstatus</h1><div class='card'>";

    if (staConnected) {
        html += "<div class='row'><span class='label'>Status</span>"
                "<span class='ok'>WiFi verbunden</span></div>"
                "<div class='row'><span class='label'>IP</span>"
                "<span class='value'>" + WiFi.localIP().toString() + "</span></div>"
                "<div class='row'><span class='label'>Gateway</span>"
                "<span class='value'>" + WiFi.gatewayIP().toString() + "</span></div>"
                "<div class='row'><span class='label'>SSID</span>"
                "<span class='value'>" + WiFi.SSID() + "</span></div>"
                "<div class='row'><span class='label'>RSSI</span>"
                "<span class='value'>" + String(WiFi.RSSI()) + " dBm</span></div>";
        html += "</div><div class='card'><a class='btn' href='/'>OK</a></div>";
    } else {
        html += "<p class='err'>Nicht verbunden – AP-Mode aktiv</p></div>"
                "<div class='card'><a class='btn' href='/scan'>Netzwerk w&auml;hlen</a></div>";
    }
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /reboot
// ============================================================

void WebServerManager::handleReboot(WiFiClient& client) {
    sendRedirect(client, "/");
    client.flush();
    client.stop();
    delay(2000);
    ESP.restart();
}

// ============================================================
// SEITE: /ota GET – Formular
// ============================================================

void WebServerManager::handleOtaPage(WiFiClient& client) {
    String html = htmlHead("OTA Firmware-Update");
    html += F("<h1>&#x2B06; OTA Update</h1>"
              "<div class='card'><h2>Firmware hochladen</h2>"
              "<p>Datei aus dem PlatformIO Build-Ordner:</p>"
              "<code style='font-size:.85em'>.pio/build/d1_mini/firmware.bin</code>"
              "<div style='margin:12px 0'>"
              "<input type='file' id='fw' accept='.bin' style='width:100%;margin:6px 0'>"
              "</div>"
              "<div id='prog' style='display:none;margin:8px 0'>"
              "<div style='background:#eee;border-radius:6px;overflow:hidden'>"
              "<div id='bar' style='background:#e6a817;height:14px;width:0;transition:width .2s'></div>"
              "</div>"
              "<p id='msg' style='text-align:center;margin:4px 0;font-size:.9em'></p>"
              "</div>"
              "<button class='btn' id='btn' onclick='doUpload()'>&#x2B06; Hochladen</button>"
              "</div>"
              "<script>"
              "function doUpload(){"
              "var f=document.getElementById('fw').files[0];"
              "if(!f){alert('Keine Datei');return;}"
              "document.getElementById('prog').style.display='block';"
              "document.getElementById('btn').disabled=true;"
              "var x=new XMLHttpRequest();"
              "x.open('POST','/ota');"
              "x.setRequestHeader('Content-Type','application/octet-stream');"
              "x.upload.onprogress=function(e){"
              "if(e.lengthComputable){"
              "var p=Math.round(e.loaded/e.total*100);"
              "document.getElementById('bar').style.width=p+'%';"
              "document.getElementById('msg').textContent=p+'% ('+Math.round(e.loaded/1024)+'/'+Math.round(e.total/1024)+' KB)';"
              "}};"
              "x.onload=function(){document.open();document.write(x.responseText);document.close();};"
              "x.onerror=function(){document.getElementById('msg').textContent='Upload fehlgeschlagen';};"
              "x.send(f);}"
              "</script>");
    html += F("<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>");
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /ota – Firmware chunk-weise in Flash schreiben
// ============================================================

void WebServerManager::handleOtaUpload(WiFiClient& client, int contentLength) {
    if (contentLength <= 0) {
        sendHtml(client, 400,
            htmlHead("OTA Fehler") +
            "<h1>Fehler</h1><div class='card'><p class='err'>Content-Length fehlt</p></div>" +
            HTML_END);
        return;
    }

    DEBUG_PRINTF("[OTA] Start: %d Byte\n", contentLength);

    if (!Update.begin(contentLength, U_FLASH)) {
        String html = htmlHead("OTA Fehler");
        html += "<h1>Fehler</h1><div class='card'><p class='err'>Kein Platz f&uuml;r Update: ";
        html += Update.getErrorString();
        html += "</p></div>";
        html += HTML_END;
        sendHtml(client, 500, html);
        return;
    }

    uint8_t buf[256];
    size_t written  = 0;
    unsigned long t = millis();

    while (written < (size_t)contentLength) {
        if (!client.connected()) break;
        int avail = client.available();
        if (avail == 0) {
            if (millis() - t > 20000UL) break;
            yield();
            continue;
        }
        t = millis();
        int toRead = min((int)sizeof(buf),
                         min(avail, contentLength - (int)written));
        int n = client.read(buf, toRead);
        if (n > 0) {
            Update.write(buf, n);
            written += n;
        }
    }

    DEBUG_PRINTF("[OTA] Geschrieben: %u / %d Byte\n", written, contentLength);

    if (!Update.end() || !Update.isFinished()) {
        String html = htmlHead("OTA Fehler");
        html += "<h1>Fehler</h1><div class='card'><p class='err'>Update fehlgeschlagen: ";
        html += Update.getErrorString();
        html += "</p></div>";
        html += HTML_END;
        sendHtml(client, 500, html);
        return;
    }

    String html = htmlHead("OTA Erfolg");
    html += "<h1>&#x2714; Update erfolgreich</h1>"
            "<div class='card'><p class='ok'>Firmware installiert. Neustart in 5 Sekunden...</p>"
            "<meta http-equiv='refresh' content='8;url=/'></div>";
    html += HTML_END;
    sendHtml(client, 200, html);
    client.flush();
    client.stop();
    delay(500);
    ESP.restart();
}

// ============================================================
// API: /api/data – JSON für Live-Update (kg + °C)
// ============================================================

void WebServerManager::handleApiData(WiFiClient& client) {
    const ScaleData& d = scaleReaderRef->getData();

    String json;
    json.reserve(80);
    json += F("{\"kg\":");
    if (d.isValid) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", d.weightKg);
        json += buf;
        json += F(",\"kgOk\":true");
    } else {
        json += F("0,\"kgOk\":false");
    }

    json += F(",\"trimmedMean\":");
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.3f", d.trimmedMeanKg);
        json += buf;
    }
    json += F(",\"spread\":");
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.4f", d.spreadKg);
        json += buf;
    }
    json += F(",\"kgTCorrected\":");
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", d.weightTCorrectedKg);
        json += buf;
    }
    json += F(",\"kgCorrected\":");
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", d.weightCorrectedKg);
        json += buf;
    }
    json += F(",\"corrActive\":");
    json += d.tempCorrectionActive ? F("true") : F("false");
    json += F(",\"raw\":");
    json += String(d.rawValue);

    json += F(",\"tempC\":");
    if (tempSensorRef) {
        const TempData& t = tempSensorRef->getData();
        if (t.isValid) {
            char buf[10];
            snprintf(buf, sizeof(buf), "%.2f", t.tempC);
            json += buf;
            json += F(",\"tempOk\":true");
        } else {
            json += F("0,\"tempOk\":false");
        }
    } else {
        json += F("0,\"tempOk\":false");
    }

    // Ertragswert
    bool  ea = ertragsAktivRef  && *ertragsAktivRef;
    float eo = ertragsOffsetRef ? *ertragsOffsetRef : 0.0f;
    json += F(",\"ertragsAktiv\":");
    json += ea ? F("true") : F("false");
    json += F(",\"ertragsgewicht\":");
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", ea ? (d.weightCorrectedKg - eo) : 0.0f);
        json += buf;
    }
    json += F("}");

    sendJson(client, json);
}

// ============================================================
// SEITE: /pt2cal – PT2-Fit-Wizard
// ============================================================

static const char PT2CAL_JS[] PROGMEM = R"js(
<script>
var wData=[],tData=[],coeff=null,t2Val=240,dVal=0.7;
function readFile(id,cb){
  var f=document.getElementById(id).files[0];
  if(!f){cb(null);return;}
  var r=new FileReader();
  r.onload=function(e){cb(e.target.result);};
  r.readAsText(f);
}
function parseCSV(txt){
  var lines=txt.trim().split(/\r?\n/);
  if(lines.length<2)return[];
  var sep=lines[0].indexOf(';')>=0?';':',';
  var hi=-1,vi=-1;
  var h0=lines[0].split(sep);
  for(var i=0;i<h0.length;i++){
    var c=h0[i].replace(/"/g,'').trim().toLowerCase();
    if(c==='last_changed'||c.indexOf('time')>=0||c.indexOf('datum')>=0)hi=i;
    if(c==='state'||c.indexOf('value')>=0||c.indexOf('wert')>=0||c.indexOf('kg')>=0||c.indexOf('temp')>=0||c.indexOf('°c')>=0)vi=i;
  }
  if(hi<0)hi=0;if(vi<0)vi=1;
  var out=[];
  for(var i=1;i<lines.length;i++){
    var p=lines[i].split(sep);
    if(p.length<2)continue;
    var ts=p[hi].replace(/"/g,'').trim();
    var vs=p[vi].replace(/"/g,'').trim();
    var ms=isNaN(Number(ts))?new Date(ts).getTime():Number(ts)*1000;
    var v=parseFloat(vs);
    if(isNaN(ms)||isNaN(v))continue;
    out.push({ms:ms,val:v});
  }
  out.sort(function(a,b){return a.ms-b.ms;});
  return out;
}
function interp(arr,ms){
  if(arr.length===0)return null;
  if(ms<=arr[0].ms)return arr[0].val;
  if(ms>=arr[arr.length-1].ms)return arr[arr.length-1].val;
  for(var i=1;i<arr.length;i++){
    if(arr[i].ms>=ms){
      var t=(ms-arr[i-1].ms)/(arr[i].ms-arr[i-1].ms);
      return arr[i-1].val+t*(arr[i].val-arr[i-1].val);
    }
  }
  return null;
}
function applyPT2(tSeries,T2_min,D){
  if(tSeries.length===0)return[];
  var wn=1.0/(T2_min*60.0),wn2=wn*wn;
  var x1=tSeries[0].val,x2=0.0;
  var out=[{ms:tSeries[0].ms,val:x1}];
  for(var i=1;i<tSeries.length;i++){
    var dt=(tSeries[i].ms-tSeries[i-1].ms)/1000.0;
    if(dt>60.0)dt=60.0;
    if(dt<=0.0){out.push({ms:tSeries[i].ms,val:x1});continue;}
    var x1n=x1+dt*x2;
    var x2n=x2+dt*(wn2*(tSeries[i].val-x1)-2.0*D*wn*x2);
    x1=x1n;x2=x2n;
    out.push({ms:tSeries[i].ms,val:x1});
  }
  return out;
}
function fitPoly(pairs){
  var n=pairs.length;
  if(n<3)return null;
  var S0=0,S1=0,S2=0,S3=0,S4=0,R0=0,R1=0,R2=0;
  for(var i=0;i<n;i++){
    var t=pairs[i].t,r=pairs[i].r;
    S0+=1;S1+=t;S2+=t*t;S3+=t*t*t;S4+=t*t*t*t;
    R0+=r;R1+=r*t;R2+=r*t*t;
  }
  var A=[[S4,S3,S2],[S3,S2,S1],[S2,S1,S0]];
  var b=[R2,R1,R0];
  for(var col=0;col<3;col++){
    var mx=col;
    for(var row=col+1;row<3;row++)if(Math.abs(A[row][col])>Math.abs(A[mx][col]))mx=row;
    var tmp=A[col];A[col]=A[mx];A[mx]=tmp;var tb=b[col];b[col]=b[mx];b[mx]=tb;
    if(Math.abs(A[col][col])<1e-12)return null;
    for(var row2=col+1;row2<3;row2++){
      var f=A[row2][col]/A[col][col];
      for(var k=col;k<3;k++)A[row2][k]-=f*A[col][k];
      b[row2]-=f*b[col];
    }
  }
  var x=[0,0,0];
  for(var i=2;i>=0;i--){
    x[i]=b[i];
    for(var j=i+1;j<3;j++)x[i]-=A[i][j]*x[j];
    x[i]/=A[i][i];
  }
  return{a:x[0],b:x[1],c:x[2]};
}
function rSq(pairs,cf){
  var mean=0;
  for(var i=0;i<pairs.length;i++)mean+=pairs[i].r;
  mean/=pairs.length;
  var ss=0,sr=0;
  for(var i=0;i<pairs.length;i++){
    var pred=cf.a*pairs[i].t*pairs[i].t+cf.b*pairs[i].t+cf.c;
    sr+=(pairs[i].r-pred)*(pairs[i].r-pred);
    ss+=(pairs[i].r-mean)*(pairs[i].r-mean);
  }
  return ss>0?1-sr/ss:0;
}
function drawPlot(pairs,cf){
  var cv=document.getElementById('plot');if(!cv)return;
  var W=cv.width,H=cv.height,pad=28;
  var ctx=cv.getContext('2d');ctx.clearRect(0,0,W,H);
  var tMin=pairs[0].t,tMax=pairs[0].t,rMin=pairs[0].r,rMax=pairs[0].r;
  for(var i=1;i<pairs.length;i++){
    if(pairs[i].t<tMin)tMin=pairs[i].t;if(pairs[i].t>tMax)tMax=pairs[i].t;
    if(pairs[i].r<rMin)rMin=pairs[i].r;if(pairs[i].r>rMax)rMax=pairs[i].r;
  }
  if(tMax===tMin)tMax=tMin+1;if(rMax===rMin)rMax=rMin+0.1;
  var px=function(t){return pad+(t-tMin)/(tMax-tMin)*(W-2*pad);};
  var py=function(r){return H-pad-(r-rMin)/(rMax-rMin)*(H-2*pad);};
  if(rMin<0&&rMax>0){
    ctx.setLineDash([4,3]);ctx.strokeStyle='#ddd';
    ctx.beginPath();ctx.moveTo(pad,py(0));ctx.lineTo(W-pad,py(0));ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.fillStyle='#e6a817';
  for(var i=0;i<pairs.length;i++){
    ctx.beginPath();ctx.arc(px(pairs[i].t),py(pairs[i].r),3,0,2*Math.PI);ctx.fill();
  }
  if(cf){
    ctx.strokeStyle='#2980b9';ctx.lineWidth=2;ctx.beginPath();
    for(var s=0;s<=60;s++){
      var t=tMin+(tMax-tMin)*s/60;
      var r=cf.a*t*t+cf.b*t+cf.c;
      if(s===0)ctx.moveTo(px(t),py(r));else ctx.lineTo(px(t),py(r));
    }
    ctx.stroke();
  }
  ctx.fillStyle='#666';ctx.font='10px sans-serif';ctx.textAlign='center';
  ctx.fillText(tMin.toFixed(1),pad,H-4);ctx.fillText(tMax.toFixed(1),W-pad,H-4);
  ctx.fillText('T_pt2 [C]',W/2,H-2);
}
function calculate(){
  t2Val=parseFloat(document.getElementById('t2in').value)||240;
  dVal =parseFloat(document.getElementById('din').value)||0.7;
  readFile('wf',function(wt){
    if(!wt){alert('Bitte Gewicht-CSV auswaehlen');return;}
    readFile('tf',function(tt){
      if(!tt){alert('Bitte Temperatur-CSV auswaehlen');return;}
      wData=parseCSV(wt);tData=parseCSV(tt);
      if(wData.length<3){alert('Gewicht-CSV: weniger als 3 Zeilen gueltige Daten');return;}
      if(tData.length<2){alert('Temperatur-CSV: weniger als 2 Zeilen gueltige Daten');return;}
      var tPT2=applyPT2(tData,t2Val,dVal);
      var pairs=[];
      for(var i=0;i<wData.length;i++){
        var T=interp(tPT2,wData[i].ms);
        if(T===null)continue;
        pairs.push({t:T,r:wData[i].val,ms:wData[i].ms});
      }
      if(pairs.length<3){alert('Zu wenig ueberlappende Zeitbereiche');return;}
      var mean=0;for(var i=0;i<pairs.length;i++)mean+=pairs[i].r;mean/=pairs.length;
      for(var i=0;i<pairs.length;i++)pairs[i].r-=mean;
      coeff=fitPoly(pairs);
      if(!coeff){alert('Gleichungssystem nicht loesbar');return;}
      var r2=rSq(pairs,coeff);
      document.getElementById('res').style.display='block';
      document.getElementById('ca').textContent=coeff.a.toFixed(8);
      document.getElementById('cb').textContent=coeff.b.toFixed(8);
      document.getElementById('cc').textContent=coeff.c.toFixed(6);
      document.getElementById('cr2').textContent=(r2*100).toFixed(2)+'%';
      document.getElementById('cn').textContent=pairs.length+' Wertepaare';
      drawPlot(pairs,coeff);
    });
  });
}
function saveCoeff(){
  if(!coeff){alert('Zuerst berechnen');return;}
  var en=document.getElementById('enSel').value==='1';
  fetch('/api/pt2cal',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({T2_min:t2Val,D:dVal,a:coeff.a,b:coeff.b,c:coeff.c,enabled:en})
  }).then(function(r){return r.json();}).then(function(d){
    if(d.ok)alert('Gespeichert!');else alert('Fehler beim Speichern');
  }).catch(function(){alert('Netzwerkfehler');});
}
window.onload=function(){
  fetch('/api/pt2cal').then(function(r){return r.json();}).then(function(d){
    var st=document.getElementById('curState');
    if(st){
      st.innerHTML=(d.enabled?'<span class="ok">aktiv</span>':'<span>inaktiv</span>')+
        ' &nbsp; T2='+d.T2_min.toFixed(0)+'min D='+d.D.toFixed(3)+
        ' a='+d.a.toFixed(8)+' b='+d.b.toFixed(8)+' c='+d.c.toFixed(6);
    }
    document.getElementById('t2in').value=d.T2_min.toFixed(0);
    document.getElementById('din').value=d.D.toFixed(3);
  }).catch(function(){});
};
</script>
)js";

void WebServerManager::handlePT2CalPage(WiFiClient& client) {
    String html = htmlHead("PT2 Korrektur");
    html += F("<h1>&#x23F3; PT2 Korrektur (Stufe 2)</h1>");

    html += F("<div class='card'><h2>Aktueller Status</h2>"
              "<div id='curState' class='row'>Lade...</div></div>");

    html += F("<div class='card'><h2>Anleitung</h2>"
              "<ol>"
              "<li>Messe &uuml;ber mehrere Stunden/Tage bei <strong>konstantem Gewicht</strong>.</li>"
              "<li>Exportiere zwei CSV-Dateien: Gewicht und Temperatur.</li>"
              "<li>W&auml;hle T&sub2; und D (Startwerte; Einfluss im Diagramm sichtbar).</li>"
              "<li>Klicke <b>Berechnen</b> – der PT2-Filter wird auf die Temperatur angewendet,<br>"
              "dann wird Poly2 auf (T_pt2, Gewichtsd&auml;mpfung) gefittet.</li>"
              "<li>R&sup2; &gt; 0,8 anstreben; T&sub2; anpassen wenn n&ouml;tig.</li>"
              "<li>Speichern aktiviert Stufe 2 optional.</li>"
              "</ol>"
              "<p><small>T&sub2; = Zeitkonstante des PT2-Filters in Minuten (z.B. 240 = 4 h).<br>"
              "D = D&auml;mpfungsgrad (0,5 = schwingungsf&auml;hig, 0,7 = kritisch ged&auml;mpft, 1,0 = &uuml;berd&auml;mpft)."
              "</small></p></div>");

    html += F("<div class='card'><h2>Filter-Parameter</h2>"
              "<label>T&#x2082; (Minuten):</label>"
              "<input type='number' id='t2in' value='240' min='1' step='10'>"
              "<label>D (D&auml;mpfung):</label>"
              "<input type='number' id='din' value='0.7' min='0.1' max='2' step='0.05'></div>");

    html += F("<div class='card'><h2>CSV-Dateien ausw&auml;hlen</h2>"
              "<label>Gewicht-CSV:</label>"
              "<input type='file' id='wf' accept='.csv,.txt' style='width:100%;margin:4px 0 12px 0'>"
              "<label>Temperatur-CSV:</label>"
              "<input type='file' id='tf' accept='.csv,.txt' style='width:100%;margin:4px 0 12px 0'>"
              "<button class='btn' onclick='calculate()'>&#x1F4D0; Berechnen</button></div>");

    html += F("<div id='res' style='display:none'>"
              "<div class='card'><h2>Ergebnis des Fits</h2>"
              "<div class='row'><span class='label'>a (T&#x2082;-Koeff.)</span>"
              "<span id='ca' class='value'>-</span></div>"
              "<div class='row'><span class='label'>b (T-Koeff.)</span>"
              "<span id='cb' class='value'>-</span></div>"
              "<div class='row'><span class='label'>c (Konstante)</span>"
              "<span id='cc' class='value'>-</span></div>"
              "<div class='row'><span class='label'>R&sup2; (G&uuml;te)</span>"
              "<span id='cr2' class='value'>-</span></div>"
              "<div class='row'><span class='label'>Datenpunkte</span>"
              "<span id='cn' class='value'>-</span></div>"
              "<canvas id='plot' width='300' height='180' "
              "style='width:100%;border:1px solid #eee;border-radius:6px;margin:10px 0'></canvas>"
              "</div>"
              "<div class='card'>"
              "<label>Korrektur nach dem Speichern aktivieren:</label>"
              "<select id='enSel' style='width:100%;padding:9px;margin:4px 0 10px 0;"
              "border:1px solid #ddd;border-radius:7px;font-size:1em'>"
              "<option value='1'>Ja</option><option value='0'>Nein</option></select>"
              "<button class='btn' onclick='saveCoeff()'>&#x1F4BE; Speichern</button>"
              "</div></div>");

    html += F("<a class='back' href='/params'>&#x2190; Zur&uuml;ck zu Parameter</a>");
    html += FPSTR(PT2CAL_JS);
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// SEITE: /tempcal – Temperaturkorrektur
// ============================================================

// JavaScript und HTML als PROGMEM-Strings um Heap zu schonen
static const char TEMPCAL_JS[] PROGMEM = R"js(
<script>
var wData=[],tData=[],coeff=null;
function readFile(id,cb){
  var f=document.getElementById(id).files[0];
  if(!f){cb(null);return;}
  var r=new FileReader();
  r.onload=function(e){cb(e.target.result);};
  r.readAsText(f);
}
function parseCSV(txt){
  var lines=txt.trim().split(/\r?\n/);
  if(lines.length<2)return[];
  var sep=lines[0].indexOf(';')>=0?';':',';
  // Spaltenindex automatisch erkennen:
  // Home Assistant-Export: "entity_id,state,last_changed" → tsCol=2, valCol=1
  // Generisches Format:    "timestamp,value"             → tsCol=0, valCol=1
  var hdr=lines[0].toLowerCase();
  var haFmt=hdr.indexOf('entity_id')>=0&&hdr.indexOf('last_changed')>=0;
  var tsCol=haFmt?2:0, valCol=1;
  var res=[];
  for(var i=1;i<lines.length;i++){
    var cols=lines[i].split(sep);
    if(cols.length<=Math.max(tsCol,valCol))continue;
    var ts=cols[tsCol].trim().replace(/"/g,'');
    var v=parseFloat(cols[valCol].trim().replace(/"/g,'').replace(',','.'));
    if(isNaN(v))continue;
    var ms;
    var num=parseFloat(ts);
    if(!isNaN(num)&&num>1e9){ms=num*1000;}
    else{var d=new Date(ts.replace(' ','T'));if(isNaN(d.getTime()))continue;ms=d.getTime();}
    res.push({ms:ms,val:v});
  }
  res.sort(function(a,b){return a.ms-b.ms;});
  return res;
}
function interp(data,ms){
  if(!data.length)return null;
  if(ms<=data[0].ms)return data[0].val;
  if(ms>=data[data.length-1].ms)return data[data.length-1].val;
  var lo=0,hi=data.length-1;
  while(hi-lo>1){var m=(lo+hi)>>1;if(data[m].ms<=ms)lo=m;else hi=m;}
  var f=(ms-data[lo].ms)/(data[hi].ms-data[lo].ms);
  return data[lo].val+f*(data[hi].val-data[lo].val);
}
function gauss3(M){
  for(var c=0;c<3;c++){
    var mr=c;
    for(var r=c+1;r<3;r++)if(Math.abs(M[r][c])>Math.abs(M[mr][c]))mr=r;
    var tmp=M[c];M[c]=M[mr];M[mr]=tmp;
    if(Math.abs(M[c][c])<1e-12)return null;
    for(var r2=c+1;r2<3;r2++){
      var fac=M[r2][c]/M[c][c];
      for(var k=c;k<=3;k++)M[r2][k]-=fac*M[c][k];
    }
  }
  var x=[0,0,0];
  for(var i=2;i>=0;i--){
    x[i]=M[i][3];
    for(var j=i+1;j<3;j++)x[i]-=M[i][j]*x[j];
    x[i]/=M[i][i];
  }
  return{a:x[0],b:x[1],c:x[2]};
}
function fitPoly(pairs){
  var n=pairs.length;
  var S0=n,S1=0,S2=0,S3=0,S4=0,R0=0,R1=0,R2=0;
  for(var i=0;i<n;i++){
    var t=pairs[i].t,r=pairs[i].r,t2=t*t;
    S1+=t;S2+=t2;S3+=t2*t;S4+=t2*t2;
    R0+=r;R1+=t*r;R2+=t2*r;
  }
  return gauss3([[S4,S3,S2,R2],[S3,S2,S1,R1],[S2,S1,S0,R0]]);
}
function rSq(pairs,cf){
  var mean=0;for(var i=0;i<pairs.length;i++)mean+=pairs[i].r;mean/=pairs.length;
  var ss=0,se=0;
  for(var i=0;i<pairs.length;i++){
    var d=pairs[i].r-mean;ss+=d*d;
    var p=cf.a*pairs[i].t*pairs[i].t+cf.b*pairs[i].t+cf.c;
    var e=pairs[i].r-p;se+=e*e;
  }
  return 1-se/ss;
}
function drawPlot(pairs,cf){
  var cv=document.getElementById('plot');
  var ctx=cv.getContext('2d');
  var W=cv.width,H=cv.height,pad=28;
  ctx.clearRect(0,0,W,H);
  if(pairs.length<2)return;
  var tMin=Infinity,tMax=-Infinity,rMin=Infinity,rMax=-Infinity;
  for(var i=0;i<pairs.length;i++){
    if(pairs[i].t<tMin)tMin=pairs[i].t;if(pairs[i].t>tMax)tMax=pairs[i].t;
    if(pairs[i].r<rMin)rMin=pairs[i].r;if(pairs[i].r>rMax)rMax=pairs[i].r;
  }
  if(cf){
    for(var s=0;s<=50;s++){
      var t=tMin+(tMax-tMin)*s/50;
      var r=cf.a*t*t+cf.b*t+cf.c;
      if(r<rMin)rMin=r;if(r>rMax)rMax=r;
    }
  }
  var dT=tMax-tMin||1,dR=rMax-rMin||1;
  var sX=(W-2*pad)/dT,sY=(H-2*pad)/dR;
  function px(t){return pad+(t-tMin)*sX;}
  function py(r){return H-pad-(r-rMin)*sY;}
  ctx.strokeStyle='#bbb';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(pad,pad);ctx.lineTo(pad,H-pad);ctx.lineTo(W-pad,H-pad);ctx.stroke();
  if(rMin<=0&&rMax>=0){
    ctx.setLineDash([4,3]);ctx.strokeStyle='#ddd';
    ctx.beginPath();ctx.moveTo(pad,py(0));ctx.lineTo(W-pad,py(0));ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.fillStyle='#e6a817';
  for(var i=0;i<pairs.length;i++){
    ctx.beginPath();ctx.arc(px(pairs[i].t),py(pairs[i].r),3,0,2*Math.PI);ctx.fill();
  }
  if(cf){
    ctx.strokeStyle='#c0392b';ctx.lineWidth=2;ctx.beginPath();
    for(var s=0;s<=60;s++){
      var t=tMin+(tMax-tMin)*s/60;
      var r=cf.a*t*t+cf.b*t+cf.c;
      if(s===0)ctx.moveTo(px(t),py(r));else ctx.lineTo(px(t),py(r));
    }
    ctx.stroke();
  }
  ctx.fillStyle='#666';ctx.font='10px sans-serif';ctx.textAlign='center';
  ctx.fillText(tMin.toFixed(1),pad,H-4);ctx.fillText(tMax.toFixed(1),W-pad,H-4);
  ctx.fillText('T [C]',W/2,H-2);
}
function calculate(){
  readFile('wf',function(wt){
    if(!wt){alert('Bitte Gewicht-CSV auswählen');return;}
    readFile('tf',function(tt){
      if(!tt){alert('Bitte Temperatur-CSV auswählen');return;}
      wData=parseCSV(wt);tData=parseCSV(tt);
      if(wData.length<3){alert('Gewicht-CSV: weniger als 3 Zeilen gültige Daten');return;}
      if(tData.length<2){alert('Temperatur-CSV: weniger als 2 Zeilen gültige Daten');return;}
      var pairs=[];
      for(var i=0;i<wData.length;i++){
        var T=interp(tData,wData[i].ms);
        if(T===null)continue;
        pairs.push({t:T,r:wData[i].val,ms:wData[i].ms});
      }
      if(pairs.length<3){alert('Zu wenig überlappende Zeitbereiche');return;}
      var mean=0;for(var i=0;i<pairs.length;i++)mean+=pairs[i].r;mean/=pairs.length;
      for(var i=0;i<pairs.length;i++)pairs[i].r-=mean;
      coeff=fitPoly(pairs);
      if(!coeff){alert('Gleichungssystem nicht lösbar (zu wenig Temperaturvariation?)');return;}
      var r2=rSq(pairs,coeff);
      document.getElementById('res').style.display='block';
      document.getElementById('ca').textContent=coeff.a.toFixed(8);
      document.getElementById('cb').textContent=coeff.b.toFixed(8);
      document.getElementById('cc').textContent=coeff.c.toFixed(6);
      document.getElementById('cr2').textContent=(r2*100).toFixed(2)+'%';
      document.getElementById('cn').textContent=pairs.length+' Wertepaare';
      drawPlot(pairs,coeff);
    });
  });
}
function saveCoeff(){
  if(!coeff){alert('Zuerst berechnen');return;}
  var en=document.getElementById('enSel').value==='1';
  fetch('/api/tempcal',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({a:coeff.a,b:coeff.b,c:coeff.c,enabled:en})
  }).then(function(r){return r.json();}).then(function(d){
    if(d.ok)alert('Gespeichert!');else alert('Fehler beim Speichern');
  }).catch(function(){alert('Netzwerkfehler');});
}
function toggleEnabled(){
  fetch('/api/tempcal').then(function(r){return r.json();}).then(function(d){
    var en=!d.enabled;
    fetch('/api/tempcal',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({a:d.a,b:d.b,c:d.c,enabled:en})
    }).then(function(r){return r.json();}).then(function(d2){
      if(d2.ok)location.reload();
    });
  });
}
window.onload=function(){
  fetch('/api/tempcal').then(function(r){return r.json();}).then(function(d){
    var st=document.getElementById('curState');
    if(st){
      st.innerHTML=(d.enabled?'<span class="ok">aktiv</span>':'<span>inaktiv</span>')+
        ' &nbsp; a='+d.a.toFixed(8)+' b='+d.b.toFixed(8)+' c='+d.c.toFixed(6);
    }
  }).catch(function(){});
};
</script>
)js";

// ============================================================
// SEITE: /params – Alle Parameter editierbar
// ============================================================

void WebServerManager::handleParams(WiFiClient& client) {
    const ScaleData& d = scaleReaderRef ? scaleReaderRef->getData() : ScaleData{};
    TempCalConfig tc = tempCalRef ? *tempCalRef : TempCalConfig{};
    PT2CalConfig  p2 = pt2CalRef  ? *pt2CalRef  : PT2CalConfig{};

    String html = htmlHead("Parameter");
    html += "<h1>&#x2699; Parameter</h1>";

    // --- Tara ---
    html += F("<div class='card'><h2>&#x21A9; Tara</h2>");
    html += "<div class='row'><span class='label'>Aktueller Offset</span>"
            "<span class='value'>" + String(d.offset) + "</span></div>";
    html += F("<p style='font-size:.88em;color:#666;margin:6px 0'>"
              "Entleere die Waage und setze den Nullpunkt.</p>"
              "<form method='post' action='/tare'>"
              "<button class='btn' type='submit'>&#x2295; Tara setzen</button>"
              "</form></div>");

    // --- Kalibrierung ---
    {
        char fbuf[20];
        snprintf(fbuf, sizeof(fbuf), "%.2f", d.calibrationFactor);
        html += F("<div class='card'><h2>&#x1F4D0; Kalibrierung</h2>");
        html += "<div class='row'><span class='label'>Kalibrierfaktor</span>"
                "<span class='value'>" + String(fbuf) + "</span></div>";
        html += F("<p style='font-size:.88em;color:#666;margin:6px 0'>"
                  "Tara setzen, dann bekanntes Gewicht auflegen und eintragen.</p>"
                  "<form method='post' action='/calibrate'>"
                  "<label>Bekanntes Gewicht (kg):</label>"
                  "<input type='number' name='weight' step='0.001' min='0.1' required "
                  "placeholder='z.B. 1.000'>"
                  "<button class='btn' type='submit'>&#x1F4D0; Kalibrieren</button>"
                  "</form></div>");
    }

    // --- Gain ---
    {
        String s128 = (d.gain == 128) ? " selected" : "";
        String s64  = (d.gain == 64)  ? " selected" : "";
        String s32  = (d.gain == 32)  ? " selected" : "";
        html += F("<div class='card'><h2>&#x1F4CA; Gain-Faktor</h2>"
                  "<form method='post' action='/gain'>"
                  "<select name='gain'>"
                  "<option value='128'");
        html += s128 + F(">128 – Kanal A (Standard)</option>"
                  "<option value='64'");
        html += s64  + F(">64 – Kanal A</option>"
                  "<option value='32'");
        html += s32  + F(">32 – Kanal B</option>"
                  "</select>"
                  "<button class='btn' type='submit'>&#x2714; Speichern</button>"
                  "</form></div>");
    }

    // --- Poly2 T-Korrektur (Stufe 1) ---
    {
        char abuf[20], bbuf[20], cbuf[20];
        snprintf(abuf, sizeof(abuf), "%.8f", tc.a);
        snprintf(bbuf, sizeof(bbuf), "%.8f", tc.b);
        snprintf(cbuf, sizeof(cbuf), "%.6f",  tc.c);
        html += F("<div class='card'><h2>&#x1F321; Poly2 T-Korrektur (Stufe 1)</h2>"
                  "<p style='font-size:.88em;color:#666;margin:0 0 8px 0'>"
                  "Korrektur(T) = a&middot;T&sup2; + b&middot;T + c &nbsp;&rarr;&nbsp; "
                  "Gew. (T-korr.) = Rohgew. &minus; Korrektur(T)</p>");
        html += "<form method='post' action='/params/tempcal'>"
                "<div class='row'><span class='label'>Aktiv</span>"
                "<input type='checkbox' name='enabled' value='1'" +
                String(tc.enabled ? " checked" : "") + "></div>"
                "<label>a:</label><input type='number' name='a' step='any' value='" + String(abuf) + "'>"
                "<label>b:</label><input type='number' name='b' step='any' value='" + String(bbuf) + "'>"
                "<label>c:</label><input type='number' name='c' step='any' value='" + String(cbuf) + "'>"
                "<button class='btn' type='submit'>&#x1F4BE; Speichern</button>"
                "</form>"
                "<a href='/tempcal' style='font-size:.88em;color:#e6a817'>"
                "&#x1F4C8; CSV-Fit Assistent &rarr;</a></div>";
    }

    // --- PT2 Korrektur (Stufe 2) ---
    {
        char t2buf[16], dbuf[12], abuf[20], bbuf[20], cbuf[20];
        snprintf(t2buf, sizeof(t2buf), "%.1f",  p2.T2_min);
        snprintf(dbuf,  sizeof(dbuf),  "%.3f",  p2.D);
        snprintf(abuf,  sizeof(abuf),  "%.8f",  p2.a);
        snprintf(bbuf,  sizeof(bbuf),  "%.8f",  p2.b);
        snprintf(cbuf,  sizeof(cbuf),  "%.6f",  p2.c);
        html += F("<div class='card'><h2>&#x23F3; PT2 Korrektur (Stufe 2)</h2>"
                  "<p style='font-size:.88em;color:#666;margin:0 0 8px 0'>"
                  "PT2-Filter glattet die Temperatur mit Zeitkonstante T&sub2;.<br>"
                  "Korrektur(T_pt2) = a&middot;T_pt2&sup2; + b&middot;T_pt2 + c</p>");
        html += "<form method='post' action='/params/pt2cal'>"
                "<div class='row'><span class='label'>Aktiv</span>"
                "<input type='checkbox' name='enabled' value='1'" +
                String(p2.enabled ? " checked" : "") + "></div>"
                "<label>T&#x2082; (Minuten):</label>"
                "<input type='number' name='t2' step='1' min='1' value='" + String(t2buf) + "'>"
                "<label>D (D&auml;mpfung, z.B. 0.7):</label>"
                "<input type='number' name='d' step='0.01' min='0.1' max='2' value='" + String(dbuf) + "'>"
                "<label>a:</label><input type='number' name='a' step='any' value='" + String(abuf) + "'>"
                "<label>b:</label><input type='number' name='b' step='any' value='" + String(bbuf) + "'>"
                "<label>c:</label><input type='number' name='c' step='any' value='" + String(cbuf) + "'>"
                "<button class='btn' type='submit'>&#x1F4BE; Speichern</button>"
                "</form>"
                "<a href='/pt2cal' style='font-size:.88em;color:#e6a817'>"
                "&#x1F4C8; CSV-Fit Assistent (PT2) &rarr;</a></div>";
    }

    html += "<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>";
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// POST: /params/tempcal – Poly2 Formular-Speicherung
// ============================================================

void WebServerManager::handleParamsTempCalPost(WiFiClient& client, const String& body) {
    TempCalConfig cfg;
    cfg.enabled = getRequestParam(body, "enabled") == "1";
    cfg.a = urlDecode(getRequestParam(body, "a")).toFloat();
    cfg.b = urlDecode(getRequestParam(body, "b")).toFloat();
    cfg.c = urlDecode(getRequestParam(body, "c")).toFloat();
    if (tempCalSaveCb) tempCalSaveCb(cfg);
    sendRedirect(client, "/params");
}

// ============================================================
// POST: /params/pt2cal – PT2 Formular-Speicherung
// ============================================================

void WebServerManager::handleParamsPT2CalPost(WiFiClient& client, const String& body) {
    PT2CalConfig cfg;
    cfg.enabled = getRequestParam(body, "enabled") == "1";
    cfg.T2_min  = urlDecode(getRequestParam(body, "t2")).toFloat();
    cfg.D       = urlDecode(getRequestParam(body, "d")).toFloat();
    cfg.a       = urlDecode(getRequestParam(body, "a")).toFloat();
    cfg.b       = urlDecode(getRequestParam(body, "b")).toFloat();
    cfg.c       = urlDecode(getRequestParam(body, "c")).toFloat();
    if (cfg.T2_min < 1.0f) cfg.T2_min = 1.0f;
    if (cfg.D      < 0.1f) cfg.D      = 0.1f;
    if (pt2CalSaveCb) pt2CalSaveCb(cfg);
    sendRedirect(client, "/params");
}

// ============================================================
// API: GET /api/pt2cal
// ============================================================

void WebServerManager::handlePT2CalApiGet(WiFiClient& client) {
    PT2CalConfig cfg;
    if (pt2CalRef) cfg = *pt2CalRef;
    char json[128];
    snprintf(json, sizeof(json),
             "{\"enabled\":%s,\"T2_min\":%.1f,\"D\":%.3f,"
             "\"a\":%.8f,\"b\":%.8f,\"c\":%.6f}",
             cfg.enabled ? "true" : "false",
             cfg.T2_min, cfg.D, cfg.a, cfg.b, cfg.c);
    sendJson(client, String(json));
}

// ============================================================
// API: POST /api/pt2cal
// ============================================================

void WebServerManager::handlePT2CalApiPost(WiFiClient& client, const String& body) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        sendJson(client, F("{\"ok\":false,\"err\":\"JSON ungueltig\"}"));
        return;
    }
    PT2CalConfig cfg;
    cfg.enabled = doc["enabled"] | false;
    cfg.T2_min  = doc["T2_min"]  | 240.0f;
    cfg.D       = doc["D"]       | 0.5f;
    cfg.a       = doc["a"]       | 0.0f;
    cfg.b       = doc["b"]       | 0.0f;
    cfg.c       = doc["c"]       | 0.0f;
    if (pt2CalSaveCb) pt2CalSaveCb(cfg);
    sendJson(client, F("{\"ok\":true}"));
}

// ============================================================
void WebServerManager::handleTempCalPage(WiFiClient& client) {
    String html = htmlHead("Temperaturkorrektur");
    html += F("<h1>&#x1F321; Temperaturkorrektur</h1>");

    // Aktueller Status (wird per JS nachgeladen)
    html += F("<div class='card'><h2>Aktueller Status</h2>"
              "<div id='curState' class='row'>Lade...</div>"
              "<form method='post' action='/tempcal' style='margin-top:8px'>"
              "</form>"
              "<button class='btn grey' onclick='toggleEnabled()' style='margin-top:6px'>"
              "&#x21C4; Korrektur ein/aus</button></div>");

    // Anleitung
    html += F("<div class='card'><h2>Anleitung</h2>"
              "<ol><li>Messe &uuml;ber mehrere Stunden/Tage bei <strong>konstantem Gewicht</strong> "
              "(leere Beute oder bekanntes Testgewicht).</li>"
              "<li>Exportiere zwei CSV-Dateien aus deinem Logging-System:<br>"
              "&nbsp;&bull; <b>Gewicht-CSV:</b> Spalten <code>Zeitstempel, kg</code><br>"
              "&nbsp;&bull; <b>Temperatur-CSV:</b> Spalten <code>Zeitstempel, &deg;C</code></li>"
              "<li>W&auml;hle beide Dateien unten aus und klicke <b>Berechnen</b>.</li>"
              "<li>Pr&uuml;fe das Diagramm (R&sup2; sollte &gt; 0,8 sein) und speichere.</li>"
              "</ol>"
              "<p><small>"
              "<b>Home Assistant History-Export</b> (entity_id, state, last_changed) "
              "wird automatisch erkannt. "
              "Alternativ: generisches Format mit zwei Spalten (Zeitstempel, Wert). "
              "Trennzeichen Komma oder Semikolon, ISO-8601-Zeitstempel oder Unix-Timestamp."
              "</small></p></div>");

    // Datei-Auswahl
    html += F("<div class='card'><h2>CSV-Dateien ausw&auml;hlen</h2>"
              "<label>Gewicht-CSV:</label>"
              "<input type='file' id='wf' accept='.csv,.txt' style='width:100%;margin:4px 0 12px 0'>"
              "<label>Temperatur-CSV:</label>"
              "<input type='file' id='tf' accept='.csv,.txt' style='width:100%;margin:4px 0 12px 0'>"
              "<button class='btn' onclick='calculate()'>&#x1F4D0; Berechnen</button></div>");

    // Ergebnis (initially hidden)
    html += F("<div id='res' style='display:none'>"
              "<div class='card'><h2>Ergebnis des Fits</h2>"
              "<div class='row'><span class='label'>a (T&sup2;-Koeffizient)</span>"
              "<span id='ca' class='value'>-</span></div>"
              "<div class='row'><span class='label'>b (T-Koeffizient)</span>"
              "<span id='cb' class='value'>-</span></div>"
              "<div class='row'><span class='label'>c (Konstante)</span>"
              "<span id='cc' class='value'>-</span></div>"
              "<div class='row'><span class='label'>R&sup2; (G&uuml;te)</span>"
              "<span id='cr2' class='value'>-</span></div>"
              "<div class='row'><span class='label'>Datenpunkte</span>"
              "<span id='cn' class='value'>-</span></div>"
              "<canvas id='plot' width='300' height='180' "
              "style='width:100%;border:1px solid #eee;border-radius:6px;margin:10px 0'></canvas>"
              "</div>"
              "<div class='card'>"
              "<label>Korrektur nach dem Speichern aktivieren:</label>"
              "<select id='enSel' style='width:100%;padding:9px;margin:4px 0 10px 0;"
              "border:1px solid #ddd;border-radius:7px;font-size:1em'>"
              "<option value='1'>Ja</option><option value='0'>Nein</option></select>"
              "<button class='btn' onclick='saveCoeff()'>&#x1F4BE; Speichern</button>"
              "</div></div>");

    html += F("<a class='back' href='/'>&#x2190; Zur&uuml;ck</a>");
    html += FPSTR(TEMPCAL_JS);
    html += HTML_END;
    sendHtml(client, 200, html);
}

// ============================================================
// API: GET /api/tempcal
// ============================================================

void WebServerManager::handleTempCalApiGet(WiFiClient& client) {
    TempCalConfig cfg;
    if (tempCalRef) {
        cfg = *tempCalRef;
    }

    char json[120];
    snprintf(json, sizeof(json),
             "{\"enabled\":%s,\"a\":%.8f,\"b\":%.8f,\"c\":%.6f}",
             cfg.enabled ? "true" : "false", cfg.a, cfg.b, cfg.c);
    sendJson(client, String(json));
}

// ============================================================
// API: POST /api/tempcal – Koeffizienten speichern
// ============================================================

void WebServerManager::handleTempCalApiPost(WiFiClient& client, const String& body) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        sendJson(client, String(F("{\"ok\":false,\"err\":\"JSON ungueltig\"}")));
        return;
    }

    TempCalConfig cfg;
    cfg.enabled = doc["enabled"] | false;
    cfg.a       = doc["a"]       | 0.0f;
    cfg.b       = doc["b"]       | 0.0f;
    cfg.c       = doc["c"]       | 0.0f;

    if (tempCalSaveCb) tempCalSaveCb(cfg);

    sendJson(client, String(F("{\"ok\":true}")));
}

void WebServerManager::sendJson(WiFiClient& client, const String& json) {
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println(F("Cache-Control: no-cache"));
    client.println();
    client.print(json);
}

// ============================================================
// HELPER
// ============================================================

String WebServerManager::getRequestParam(const String& request, const char* param) {
    String search = String(param) + "=";
    int start = request.indexOf(search);
    if (start == -1) return "";
    start += search.length();
    int end = request.indexOf('&', start);
    if (end == -1) end = request.length();
    return request.substring(start, end);
}

String WebServerManager::urlDecode(const String& str) {
    String result = str;
    result.replace("+", " ");
    int i = 0;
    while (i < (int)result.length() - 2) {
        if (result[i] == '%') {
            String hex = result.substring(i + 1, i + 3);
            char c = (char)strtol(hex.c_str(), nullptr, 16);
            result = result.substring(0, i) + String(c) + result.substring(i + 3);
        } else {
            i++;
        }
    }
    return result;
}
