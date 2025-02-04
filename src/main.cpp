#include <ETH.h>
#include <Wire.h>
#include "config.h"
#include "webserver.h"
#include "relays.h"
#include "gesture.h"
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Definicje zmiennych globalnych
int currentSpeed = 0;     // Domyślnie wentylator wyłączony
int defaultSpeed = 1;     // Domyślny bieg wentylatora
String webhookUrl = "";   // URL webhooka, domyślnie pusty

unsigned long lastRelayLogTime = 0; // Zmienna do śledzenia czasu dla logowania
float temperature = 0.0;
float humidity = 0.0;
bool gestureControlEnabled = true;

unsigned long lastSensorUpdateTime = 0;

// Konfiguracja Ethernetu dla WT32-ETH01
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
#define ETH_POWER_PIN   16
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        1
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
void setupOTA() {
    ArduinoOTA.setHostname("Okap-OTA"); // Ustawienie własnej nazwy urządzenia OTA

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("OTA Start - Aktualizacja: " + type);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Koniec - Restartowanie...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progres: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Błąd OTA [%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Błąd uwierzytelniania");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Błąd inicjalizacji");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Błąd połączenia");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Błąd odbierania");
        else if (error == OTA_END_ERROR) Serial.println("Błąd zakończenia");
    });

    ArduinoOTA.begin();
}
void logRelayStates() {
    // Pobieramy stany przekaźników
    int relay1 = digitalRead(RELAY_PIN1);
    int relay2 = digitalRead(RELAY_PIN2);
    int relay3 = digitalRead(RELAY_PIN3);

    // Wypisujemy stan przekaźników w formacie "przekazniki x x x"
    //Serial.printf("przekazniki %d %d %d\n", relay1, relay2, relay3);
    Serial.printf("aktualny bieg:  %d ", currentSpeed);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Inicjalizacja Ethernet...");
    // Włączenie zasilania Ethernetu
    if (ETH_POWER_PIN >= 0) {
        pinMode(ETH_POWER_PIN, OUTPUT);
        digitalWrite(ETH_POWER_PIN, HIGH);
        delay(100);
    }

    // Inicjalizacja Ethernetu
    if (!ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE)) {
        Serial.println("Błąd inicjalizacji Ethernet!");
        while (true) delay(1000);
    }

    Serial.println("Czekam na połączenie...");
    while (!ETH.linkUp()) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nPołączono!");
    Serial.print("Adres IP: ");
    Serial.println(ETH.localIP());
    Serial.print("MAC: ");
    Serial.println(ETH.macAddress());
    setupOTA();  // Inicjalizacja OTA
    // Inicjalizacja przekaźników
    setupRelays();

    // Inicjalizacja magistrali I2C i VL53L0X
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000); // Zegar I2C 100 kHz

    setupGesture();

    // Inicjalizacja serwera Web GUI i API
    setupWebServer();

    if (!bme.begin(0x76)) {
        Serial.println("Błąd inicjalizacji BME280!");
        while (1);
    }
}

void loop() {
    // Monitorowanie połączenia Ethernet
    if (!ETH.linkUp()) {
        Serial.println("Ethernet rozłączony!");
    }
    
    ArduinoOTA.handle();
    // Logowanie stanu przekaźników co sekundę
    //if (millis() - lastRelayLogTime >= 1000) {
    //    logRelayStates();
    //    lastRelayLogTime = millis();
    //}

    if (millis() - lastSensorUpdateTime >= 1000) {
        updateSensorData();
        lastSensorUpdateTime = millis();
    }

    if (gestureControlEnabled) {
        processGesture();
    }
}
