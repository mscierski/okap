#include <Adafruit_BME280.h>

#define DEFAULT_TEMP_THRESHOLD 2.0f
#define DEFAULT_HUM_THRESHOLD 5.0f
#define DEFAULT_CHECK_INTERVAL 60000

// Definicja zmiennej bme
Adafruit_BME280 bme;
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
