/**
 * button_handler.cpp - Taster-Zustandsautomat
 */

#include "button_handler.h"

ButtonHandler::ButtonHandler()
    : state(State::IDLE), lastRaw(false), debounced(false),
      debounceMs(0), pressStartMs(0), confirmStartMs(0), confirmWindowOpen(false) {
}

void ButtonHandler::begin() {
#if BUTTON_ACTIVE_HIGH
    pinMode(BUTTON_PIN, INPUT);          // externer Pull-Down, kein interner Pull-Up
    bool initial = (digitalRead(BUTTON_PIN) == HIGH);
#else
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    bool initial = (digitalRead(BUTTON_PIN) == LOW);
#endif
    debounced = initial;
    lastRaw   = initial;
    DEBUG_PRINTLN("[Button] GPIO" + String(BUTTON_PIN));
}

ButtonEvent ButtonHandler::update() {
    // ---- Entprellung ----
#if BUTTON_ACTIVE_HIGH
    bool raw = (digitalRead(BUTTON_PIN) == HIGH);
#else
    bool raw = (digitalRead(BUTTON_PIN) == LOW);
#endif
    if (raw != lastRaw) {
        debounceMs = millis();
        lastRaw    = raw;
    }
    bool prevDebounced = debounced;
    if (millis() - debounceMs >= BUTTON_DEBOUNCE_MS) {
        debounced = raw;
    }
    bool risingEdge  = ( debounced && !prevDebounced);  // gedrückt
    bool fallingEdge = (!debounced &&  prevDebounced);  // losgelassen

    // ---- Zustandsautomat ----
    switch (state) {

        case State::IDLE:
            if (risingEdge) {
                state        = State::PRESSED;
                pressStartMs = millis();
            }
            break;

        case State::PRESSED:
            if (fallingEdge) {
                // Losgelassen vor 5 s → SHORT_PRESS
                state = State::IDLE;
                return ButtonEvent::SHORT_PRESS;
            }
            if (debounced && (millis() - pressStartMs >= BUTTON_LONG_PRESS_MS)) {
                // 5 s gehalten → Signal senden, warte auf Loslassen
                state = State::LONG_FIRED;
                return ButtonEvent::LONG_HELD;
            }
            break;

        case State::LONG_FIRED:
            // Warte bis Taster losgelassen, dann Bestätigungsfenster öffnen
            if (fallingEdge) {
                state             = State::CONFIRM_WINDOW;
                confirmStartMs    = millis();
                confirmWindowOpen = true;
            }
            break;

        case State::CONFIRM_WINDOW:
            if (millis() - confirmStartMs >= BUTTON_CONFIRM_WINDOW_MS) {
                state             = State::IDLE;
                confirmWindowOpen = false;
                return ButtonEvent::CONFIRM_TIMEOUT;
            }
            if (fallingEdge) {
                // Kurzer Druck + Loslassen = Bestätigung
                state             = State::IDLE;
                confirmWindowOpen = false;
                return ButtonEvent::CONFIRM;
            }
            break;
    }

    return ButtonEvent::NONE;
}

unsigned long ButtonHandler::confirmWindowRemaining() const {
    if (!confirmWindowOpen) return 0;
    unsigned long elapsed = millis() - confirmStartMs;
    if (elapsed >= BUTTON_CONFIRM_WINDOW_MS) return 0;
    return BUTTON_CONFIRM_WINDOW_MS - elapsed;
}
