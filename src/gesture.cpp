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

void setupGesture() {
    if (!lox.begin()) {
        Serial.println("Błąd inicjalizacji VL53L0X! Sprawdź połączenia.");
        while (1);
    }
    Serial.println("VL53L0X zainicjalizowany!");
}

void processGesture() {
    static unsigned long presenceStartTime = 0; // Czas początku obecności ręki w zasięgu
    static unsigned long lastStepTime = 0;     // Czas ostatniej zmiany biegu przy przytrzymaniu
    static int lastValidDistance = 0;          // Ostatnia poprawna odległość

    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4) { // 4 = brak danych
        int distance = measure.RangeMilliMeter;

        if (distance >= 50 && distance <= 2000) { // Zakres odczytów
           // Serial.print("Odległość: ");
            //Serial.print(distance);
            //Serial.println(" mm");

            if (distance <= 200) { // Ręka w odległości 0–20 cm
                if (presenceStartTime == 0) {
                    presenceStartTime = millis(); // Zaczynamy liczyć czas obecności ręki
                    lastStepTime = millis();      // Inicjujemy licznik do zmiany biegów
                }

                // Sprawdź, czy trzymamy rękę wystarczająco długo na przytrzymanie
                if (millis() - presenceStartTime > 3000) { // 3 sekundy przytrzymania
                    if (millis() - lastStepTime >= 3000) { // Zwiększamy bieg co kolejne 3 sekundy
                        Serial.println("Wykryto przytrzymanie ręki - zwiększanie biegu!");
                        currentSpeed = (currentSpeed + 1) % 5; // Biegi: 0–4, zapętlenie
                        setFanSpeed(currentSpeed);
                        lastStepTime = millis(); // Aktualizuj czas ostatniej zmiany biegu
                        sendWebhookRequest(currentSpeed);
                        notifyClients();
                    }
                }
            } else {
                // Jeśli ręka opuściła zasięg przed upływem 3 sekund → "kliknięcie"
                if (presenceStartTime > 0 && millis() - presenceStartTime <= 3000) {
                    Serial.println("Wykryto kliknięcie - ON/OFF!");
                    // Przełącz pomiędzy OFF a biegiem domyślnym
                    currentSpeed = (currentSpeed == 0) ? defaultSpeed : 0;
                    setFanSpeed(currentSpeed);
                    sendWebhookRequest(currentSpeed);
                    notifyClients();
                }

                // Reset flag
                presenceStartTime = 0;
                holdDetected = false;
            }

            lastValidDistance = distance; // Aktualizuj ostatnią poprawną odległość
        }
    }

    delay(50); // Krótkie opóźnienie dla stabilności pętli
}
