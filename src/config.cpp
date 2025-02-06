#include <vector>
#include "config.h"
#include <Adafruit_BME280.h>

// Definicja zmiennej bme
Adafruit_BME280 bme;

// Define the actual storage for speedLogs
std::vector<LogEntry> speedLogs;

float lastTemperature = 0.0f;
float lastHumidity = 0.0f;
unsigned long lastMonitoringTime = 0;

float tempRiseThreshold = DEFAULT_TEMP_THRESHOLD;
float humRiseThreshold = DEFAULT_HUM_THRESHOLD;
unsigned long monitoringInterval = DEFAULT_CHECK_INTERVAL;
bool autoActivationEnabled = true;

// Network settings
bool dhcpEnabled = true;
String staticIP = "192.168.0.200";
String staticGateway = "192.168.0.1";
String staticNetmask = "255.255.255.0";  // Add this line

// ...existing code...

unsigned long fanStartTime = 0;
bool isFanRunning = false;
