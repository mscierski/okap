#include <ETH.h>
#include <Wire.h>
#include <ESPmDNS.h>  // This is the correct include for ESP32
#include "config.h"
#include "webserver.h"
#include "relays.h"
#include "gesture.h"
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <esp_system.h>
#include <time.h>

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
#define ETH_MDIO_PIN     18

#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"
#define GMT_OFFSET_SEC 3600  // GMT+1 for Poland
#define DAYLIGHT_OFFSET_SEC 3600  // 1 hour daylight saving

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

void setupTime() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
    
    Serial.println("Waiting for NTP time sync...");
    time_t now = time(nullptr);
    int retry = 0;
    while (now < 24 * 3600 && retry < 10) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        retry++;
    }
    Serial.println();
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println("Time synchronized!");
        char timeStringBuff[50];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
        Serial.println(timeStringBuff);
    } else {
        Serial.println("Failed to obtain time");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    preferences.begin("okap", false);

    // Load network settings first
    dhcpEnabled = preferences.getBool("dhcpEnabled", true);
    staticIP = preferences.getString("staticIP", "192.168.1.200");
    staticGateway = preferences.getString("staticGateway", "192.168.1.1");
    staticNetmask = preferences.getString("staticNetmask", "255.255.255.0");

    Serial.println("Network settings loaded:");
    Serial.printf("DHCP: %s\n", dhcpEnabled ? "true" : "false");
    Serial.printf("Static IP: %s\n", staticIP.c_str());
    Serial.printf("Gateway: %s\n", staticGateway.c_str());
    Serial.printf("Netmask: %s\n", staticNetmask.c_str());

    // Power up Ethernet
    if (ETH_POWER_PIN >= 0) {
        pinMode(ETH_POWER_PIN, OUTPUT);
        digitalWrite(ETH_POWER_PIN, HIGH);
        delay(100);
    }

    // Configure network before starting Ethernet
    if (!dhcpEnabled) {
        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
        
        if (ip.fromString(staticIP) && gateway.fromString(staticGateway) && subnet.fromString(staticNetmask)) {
            // Configure static IP directly
            ETH.config(ip, gateway, subnet);
            Serial.println("Static IP configured");
        } else {
            Serial.println("Invalid IP format - falling back to DHCP");
            dhcpEnabled = true;
        }
    }

    // Start Ethernet with new settings
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    
    // Wait for connection
    int timeout = 0;
    Serial.println("Waiting for connection...");
    while (!ETH.linkUp() && timeout < 10) { // 5 second timeout
        delay(500);
        Serial.print(".");
        timeout++;
    }

    if (!ETH.linkUp()) {
        Serial.println("\nEthernet connection failed!");
        // Maybe add some error handling here
    } else {
        Serial.println("\nConnected!");
        Serial.print("IP Address: ");
        Serial.println(ETH.localIP());
        Serial.print("Gateway: ");
        Serial.println(ETH.gatewayIP());
        Serial.print("Subnet Mask: ");
        Serial.println(ETH.subnetMask());

        setupTime();

        // After Ethernet is connected, initialize mDNS
        if (!MDNS.begin("okap")) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            MDNS.addService("http", "tcp", 80);  // Add this line to advertise HTTP service
            Serial.println("mDNS responder started");
            Serial.println("Device will be accessible at okap.local");
        }
    }

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

    // Take initial readings to prevent false triggers on boot
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    lastTemperature = temperature;
    lastHumidity = humidity;
    lastMonitoringTime = millis();
}

void loop() {
    // Monitorowanie połączenia Ethernet
    if (!ETH.linkUp()) {
        Serial.println("Ethernet rozłączony!");
    }
    
    ArduinoOTA.handle();
    
    if (millis() - lastSensorUpdateTime >= 1000) {
        updateSensorData();
        lastSensorUpdateTime = millis();
    }

    if (gestureControlEnabled) {
        processGesture();
    }

    // Optionally add periodic time sync check (every hour)
    static unsigned long lastTimeSyncMillis = 0;
    if (millis() - lastTimeSyncMillis > 3600000) {  // 1 hour
        time_t now = time(nullptr);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStringBuff[50];
            strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
            Serial.printf("Current time: %s\n", timeStringBuff);
        }
        lastTimeSyncMillis = millis();
    }

    // Remove MDNS.update() as it's not needed on ESP32
}
