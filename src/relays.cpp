#include <Arduino.h>
#include "relays.h"
#include "config.h"
#include "webserver.h"  // Add this include

void setupRelays() {
    // Konfiguracja pinów przekaźników jako wyjścia
    pinMode(RELAY_PIN1, OUTPUT);
    pinMode(RELAY_PIN2, OUTPUT);
    pinMode(RELAY_PIN3, OUTPUT);

    // Ustawienie początkowego stanu przekaźników na wyłączony (stan wysoki)
    digitalWrite(RELAY_PIN1, HIGH);
    digitalWrite(RELAY_PIN2, HIGH);
    digitalWrite(RELAY_PIN3, HIGH);
}

void setFanSpeed(int speed) {
    int oldSpeed = currentSpeed;
    // Sterowanie prędkością wentylatora przy użyciu przekaźników
    switch (speed) {
        case 0: 
            digitalWrite(RELAY_PIN1, HIGH);
            digitalWrite(RELAY_PIN2, HIGH);
            digitalWrite(RELAY_PIN3, HIGH);
            break;
        case 1: 
            digitalWrite(RELAY_PIN1, HIGH);
            digitalWrite(RELAY_PIN2, HIGH);
            digitalWrite(RELAY_PIN3, LOW);
            break;
        case 2: 
            digitalWrite(RELAY_PIN1, HIGH);
            digitalWrite(RELAY_PIN2, LOW);
            digitalWrite(RELAY_PIN3, HIGH);
            break;
        case 3: 
            digitalWrite(RELAY_PIN1, HIGH);
            digitalWrite(RELAY_PIN2, LOW);
            digitalWrite(RELAY_PIN3, LOW);
            break;
        case 4: 
            digitalWrite(RELAY_PIN1, LOW);
            digitalWrite(RELAY_PIN2, HIGH);
            digitalWrite(RELAY_PIN3, LOW);
            break;
    }

    if (speed == 0) {
        isFanRunning = false;
    } else if (oldSpeed == 0) {
        isFanRunning = true;
        fanStartTime = millis();
    }

    currentSpeed = speed;
}
