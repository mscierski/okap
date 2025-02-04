#ifndef CONFIG_H
#define CONFIG_H

#include <Adafruit_BME280.h> // Include the Adafruit_BME280 header

// GPIO Pins for relays
#define RELAY_PIN1 5
#define RELAY_PIN2 32
#define RELAY_PIN3 33

// Default fan speed
#define DEFAULT_SPEED 1

// I2C Pins for VL53L0X
#define SDA_PIN 14
#define SCL_PIN 15

// Auto-activation thresholds
#define TEMP_RISE_THRESHOLD 1.0f    // Temperature rise threshold in °C per minute
#define HUM_RISE_THRESHOLD 3.0f     // Humidity rise threshold in % per minute

// Monitoring window
#define MONITORING_INTERVAL 10000    // Check every 10 seconds

// Default values for auto-activation
#define DEFAULT_TEMP_THRESHOLD 1.0f
#define DEFAULT_HUM_THRESHOLD 3.0f
#define DEFAULT_CHECK_INTERVAL 10000

// Deklaracje globalnych zmiennych
extern int currentSpeed;
extern int defaultSpeed;
extern String webhookUrl; // Deklaracja zmiennej webhookUrl jako extern
extern Adafruit_BME280 bme; // Deklaracja zmiennej bme jako extern

extern float lastTemperature;
extern float lastHumidity;
extern unsigned long lastMonitoringTime;

extern float tempRiseThreshold;
extern float humRiseThreshold;
extern unsigned long monitoringInterval;
extern bool autoActivationEnabled;

// Network settings
extern bool dhcpEnabled;
extern String staticIP;
extern String staticGateway;
extern String staticNetmask;  // Add this line

#endif
