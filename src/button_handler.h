/**
 * button_handler.h - Taster-Zustandsautomat
 * SD3 (GPIO10), aktiv LOW, mit Entprellung
 *
 * Ereignisse:
 *   SHORT_PRESS  – Taster kürzer als BUTTON_LONG_PRESS_MS gedrückt
 *   LONG_HELD    – Taster genau BUTTON_LONG_PRESS_MS gehalten (noch gedrückt)
 *   CONFIRM      – Kurzer Druck während Bestätigungsfenster offen
 *   CONFIRM_TIMEOUT – Bestätigungsfenster abgelaufen ohne Druck
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,       // Kurzer Druck (< 5 s)
    LONG_HELD,         // Taster seit 5 s gehalten → Bestätigungsfenster öffnen
    CONFIRM,           // Bestätigung im 5-s-Fenster
    CONFIRM_TIMEOUT    // Fenster abgelaufen, keine Bestätigung
};

class ButtonHandler {
public:
    ButtonHandler();
    void begin();

    // Im loop() aufrufen – gibt das aktuelle Ereignis zurück (einmalig pro Ereignis)
    ButtonEvent update();

    bool isConfirmWindowOpen() const { return confirmWindowOpen; }
    unsigned long confirmWindowRemaining() const;

private:
    enum class State {
        IDLE,
        PRESSED,          // Taster gedrückt, Zeit läuft
        LONG_FIRED,       // LONG_HELD wurde gemeldet, warte auf Loslassen
        CONFIRM_WINDOW    // 5-s-Bestätigungsfenster läuft
    };

    State         state;
    bool          lastRaw;            // letzter entprellter Zustand (true=gedrückt)
    bool          debounced;
    unsigned long debounceMs;
    unsigned long pressStartMs;
    unsigned long confirmStartMs;
    bool          confirmWindowOpen;
};

#endif // BUTTON_HANDLER_H
