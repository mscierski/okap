#include "gesture.h"
#include "config.h"
#include "relays.h"
#include "webserver.h"
// Definicja sensora VL53L0X
Adafruit_VL53L0X lox;

// Zmienne związane z gestami
unsigned long gestureStartTime = 0;
bool gestureDetected = false;
bool holdDetected = false;
const int gestureThreshold = 200;  // Czas (ms) na wykrycie machnięcia ręką
const int holdThreshold = 1000;    // Czas (ms) na wykrycie przytrzymania ręki

// Add global variable
int currentDistance = 0;

void setupGesture() {
    if (!lox.begin()) {
        Serial.println("Błąd inicjalizacji VL53L0X! Sprawdź połączenia.");
        while (1);
    }
    Serial.println("VL53L0X zainicjalizowany!");
}

void processGesture() {
    static unsigned long presenceStartTime = 0;
    static unsigned long lastStepTime = 0;
    static int lastValidDistance = 0;

    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4) {
        currentDistance = measure.RangeMilliMeter;

        // Only process gestures when distance is within valid range (50-200mm)
        if (currentDistance >= 50 && currentDistance <= 200) {
            if (presenceStartTime == 0) {
                presenceStartTime = millis();
                lastStepTime = millis();
            }

            // Hold gesture detection
            if (millis() - presenceStartTime > 3000) {
                if (millis() - lastStepTime >= 3000) {
                    Serial.println("Wykryto przytrzymanie ręki - zwiększanie biegu!");
                    int newSpeed = (currentSpeed + 1) % 5;
                    String details = "Hand hold - changing speed (distance: " + String(currentDistance) + "mm)";
                    int oldSpeed = currentSpeed;
                    logGestureEvent(currentSpeed, newSpeed, details);
                    currentSpeed = newSpeed;
                    setFanSpeed(currentSpeed);
                    lastStepTime = millis();
                    sendWebhookRequest(currentSpeed, "GESTURE", oldSpeed);
                    notifyClients();
                }
            }
        } else {
            // Hand moved away - check if it was a quick gesture
            if (presenceStartTime > 0 && millis() - presenceStartTime <= 3000) {
                Serial.println("Wykryto kliknięcie - ON/OFF!");
                int oldSpeed = currentSpeed;
                currentSpeed = (currentSpeed == 0) ? defaultSpeed : 0;
                String details = "Hand gesture - " + 
                       String(currentSpeed == 0 ? "turning off" : "turning on") +
                       " (distance: " + String(currentDistance) + "mm)";
                
                if (currentSpeed == 0) {
                    logGestureEvent(defaultSpeed, 0, details);
                } else {
                    logGestureEvent(0, defaultSpeed, details);
                }
                
                setFanSpeed(currentSpeed);
                sendWebhookRequest(currentSpeed, "GESTURE", oldSpeed);
                notifyClients();
            }
            
            // Reset when hand is away
            presenceStartTime = 0;
            holdDetected = false;
        }

        lastValidDistance = currentDistance;
    } else {
        currentDistance = -1;
    }

    delay(50);
}
