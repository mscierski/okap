#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "webserver.h"
#include "relays.h"
#include "config.h"
#include "gesture.h"

extern int currentSpeed;
extern int defaultSpeed;
extern float temperature;
extern float humidity;
extern bool gestureControlEnabled;

Preferences preferences;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Funkcja do powiadamiania klientów przez WebSocket
void notifyClients() {
    StaticJsonDocument<256> jsonResponse;
    jsonResponse["currentSpeed"] = currentSpeed;
    jsonResponse["temperature"] = temperature;
    jsonResponse["humidity"] = humidity;
    jsonResponse["gestureControlEnabled"] = gestureControlEnabled;
    jsonResponse["gestureDetected"] = gestureDetected;
    jsonResponse["holdDetected"] = holdDetected;
    jsonResponse["tempRiseThreshold"] = tempRiseThreshold;
    jsonResponse["humRiseThreshold"] = humRiseThreshold;
    jsonResponse["monitoringInterval"] = monitoringInterval;
    jsonResponse["autoActivationEnabled"] = autoActivationEnabled;
    String response;
    serializeJson(jsonResponse, response);
    ws.textAll(response);
}

// Obsługa zdarzeń WebSocket
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.println("WebSocket client connected");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.println("WebSocket client disconnected");
    }
}

// Wysyłanie requesta na webhook
void sendWebhookRequest(int speed) {
    if (webhookUrl.isEmpty()) {
        Serial.println("Webhook URL jest pusty. Nie wysyłamy żądania.");
        return;
    }

    HTTPClient http;
    http.begin(webhookUrl);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"speed\":" + String(speed) + "}";
    Serial.println("Wysyłanie requesta na webhook: " + webhookUrl);
    Serial.println("Payload: " + payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        Serial.printf("Webhook wysłany! Kod odpowiedzi: %d\n", httpResponseCode);
    } else {
        Serial.printf("Błąd podczas wysyłania webhooka: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
}

// ...existing code...

void updateSensorData() {
    float newTemperature = bme.readTemperature();
    float newHumidity = bme.readHumidity();
    
    // Calculate rate of change per minute
    unsigned long timeDiff = (millis() - lastMonitoringTime) / 1000.0f; // Convert to seconds
    if (autoActivationEnabled && timeDiff >= (monitoringInterval / 1000)) { // Check every monitoringInterval seconds
        float tempChangeRate = ((newTemperature - lastTemperature) / timeDiff) * 60.0f; // Change per minute
        float humChangeRate = ((newHumidity - lastHumidity) / timeDiff) * 60.0f;       // Change per minute

        // If fan is off and we detect significant changes, activate it
        if (currentSpeed == 0 && 
            (tempChangeRate >= tempRiseThreshold || humChangeRate >= humRiseThreshold)) {
            Serial.println("Detected cooking activity! Activating fan.");
            Serial.printf("Temperature change rate: %.2f°C/min, Humidity change rate: %.2f%%/min\n", 
                        tempChangeRate, humChangeRate);
            
            currentSpeed = defaultSpeed;
            setFanSpeed(currentSpeed);
            sendWebhookRequest(currentSpeed);
        }

        lastTemperature = newTemperature;
        lastHumidity = newHumidity;
        lastMonitoringTime = millis();
    }

    temperature = newTemperature;
    humidity = newHumidity;
    notifyClients();
}

void setupWebServer() {
    preferences.begin("okap", false);

    // Check if defaults are stored, if not, store them
    if (!preferences.isKey("defaultTempThreshold")) {
        preferences.putFloat("defaultTempThreshold", DEFAULT_TEMP_THRESHOLD);
    }
    if (!preferences.isKey("defaultHumThreshold")) {
        preferences.putFloat("defaultHumThreshold", DEFAULT_HUM_THRESHOLD);
    }
    if (!preferences.isKey("defaultCheckInterval")) {
        preferences.putULong("defaultCheckInterval", DEFAULT_CHECK_INTERVAL);
    }

    // Load configuration
    webhookUrl = preferences.getString("webhook", "");
    defaultSpeed = preferences.getInt("defaultSpeed", 1);
    gestureControlEnabled = preferences.getBool("gestureEnabled", true);
    
    // Load thresholds from preferences, using stored defaults as fallback
    float defaultTemp = preferences.getFloat("defaultTempThreshold", DEFAULT_TEMP_THRESHOLD);
    float defaultHum = preferences.getFloat("defaultHumThreshold", DEFAULT_HUM_THRESHOLD);
    unsigned long defaultInterval = preferences.getULong("defaultCheckInterval", DEFAULT_CHECK_INTERVAL);
    
    tempRiseThreshold = preferences.getFloat("tempThreshold", defaultTemp);
    humRiseThreshold = preferences.getFloat("humThreshold", defaultHum);
    monitoringInterval = preferences.getULong("monitorInterval", defaultInterval);
    autoActivationEnabled = preferences.getBool("autoActivation", true);

    Serial.println("Załadowano adres webhooka: " + webhookUrl);
    Serial.printf("Załadowano domyślny bieg: %d\n", defaultSpeed);

    if (!bme.begin(0x76)) {
        Serial.println("Błąd inicjalizacji BME280!");
        while (1);
    }

    // Główna strona HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<!DOCTYPE html>";
        html += "<html lang=\"pl\">";
        html += "<head>";
        html += "<meta charset=\"UTF-8\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
        html += "<title>Okap - Sterowanie</title>";
        html += "<style>";
        html += "body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; background-color: #f4f4f4; }";
        html += "h1 { margin: 20px 0; font-size: 2.5em; }";
        html += "h3 { margin: 10px 0; font-size: 1.5em; }";
        html += "button { margin: 5px; padding: 15px 25px; font-size: 18px; border: none; border-radius: 5px; background-color: #007BFF; color: white; cursor: pointer; transition: background-color 0.3s ease; }";
        html += "button:hover { background-color: #0056b3; }";
        html += "input[type=\"text\"], input[type=\"number\"] { padding: 10px; font-size: 18px; width: 60%; margin-right: 10px; text-align: center; }";
        html += "#container { max-width: 600px; margin: auto; padding: 20px; background: white; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1); border-radius: 10px; }";
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div id=\"container\">";
        html += "<h1>OKAP</h1>";
        html += "<h3>Aktualny bieg: <span id=\"currentSpeed\">" + String(currentSpeed) + "</span></h3>";
        html += "<h3>Temperatura: <span id=\"temperature\">" + String(temperature, 1) + " °C</span></h3>";
        html += "<h3>Wilgotność: <span id=\"humidity\">" + String(humidity, 1) + " %</span></h3>";
        html += "<h3>Gesty: <input type=\"checkbox\" id=\"gestureControl\" " + String(gestureControlEnabled ? "checked" : "") + " onchange=\"toggleGestureControl()\"></h3>";
        html += "<form id=\"defaultForm\">";
        html += "<h3>Ustaw domyślny bieg:</h3>";
        html += "<input type=\"number\" id=\"defaultInput\" min=\"0\" max=\"4\" value=\"" + String(defaultSpeed) + "\">";
        html += "<button type=\"button\" onclick=\"setDefault()\">Ustaw</button>";
        html += "</form>";
        html += "<h3>Ustaw prędkość:</h3>";
        html += "<div>";
        for (int i = 0; i <= 4; i++) {
            html += "<button onclick=\"setSpeed(" + String(i) + ")\">" + String(i) + "</button>";
        }
        html += "</div>";
        html += "<h3>Webhook:</h3>";
        html += "<form id=\"webhookForm\">";
        html += "<input type=\"text\" id=\"webhookUrl\" placeholder=\"Podaj adres webhooka\" value=\"" + webhookUrl + "\">";
        html += "<button type=\"button\" onclick=\"setWebhook()\">Zapisz</button>";
        html += "</form>";
        html += "<h3>Automatyka:</h3>";
        html += "<div style='text-align: left; padding: 10px;'>";
        html += "<label>Włączona: <input type='checkbox' id='autoActivation' " + String(autoActivationEnabled ? "checked" : "") + " onchange='updateAutoSettings()'></label><br>";
        html += "<label>Próg temperatury (°C/min): <input type='number' id='tempThreshold' value='" + String(tempRiseThreshold) + "' step='0.1' min='0.1' max='10' style='width:100px'></label><br>";
        html += "<label>Próg wilgotności (%/min): <input type='number' id='humThreshold' value='" + String(humRiseThreshold) + "' step='0.1' min='0.1' max='20' style='width:100px'></label><br>";
        html += "<label>Interwał sprawdzania (s): <input type='number' id='checkInterval' value='" + String(monitoringInterval/1000) + "' min='1' max='60' style='width:100px'></label><br>";
        html += "<button onclick='updateAutoSettings()'>Zapisz ustawienia</button>";
        html += "</div>";
        html += "</div>";
        html += "<script>";
        html += "const ws = new WebSocket('ws://' + location.host + '/ws');";
        html += "ws.onmessage = function(event) {";
        html += "  const data = JSON.parse(event.data);";
        html += "  document.getElementById('currentSpeed').innerText = data.currentSpeed;";
        html += "  document.getElementById('temperature').innerText = data.temperature.toFixed(1) + ' °C';";
        html += "  document.getElementById('humidity').innerText = data.humidity.toFixed(1) + ' %';";
        html += "  document.getElementById('gestureControl').checked = data.gestureControlEnabled;";
        html += "};";
        html += "function setSpeed(speed) {";
        html += "  fetch('/state', {";
        html += "    method: 'POST',";
        html += "    headers: { 'Content-Type': 'application/json' },";
        html += "    body: JSON.stringify({ speed: speed })";
        html += "  });";
        html += "}";
        html += "function setDefault() {";
        html += "  const defaultSpeed = document.getElementById('defaultInput').value;";
        html += "  fetch('/default', {";
        html += "    method: 'POST',";
        html += "    headers: { 'Content-Type': 'application/json' },";
        html += "    body: JSON.stringify({ default: defaultSpeed })";
        html += "  });";
        html += "}";
        html += "function setWebhook() {";
        html += "  const url = document.getElementById('webhookUrl').value;";
        html += "  fetch('/webhook', {";
        html += "    method: 'POST',";
        html += "    headers: { 'Content-Type': 'application/json' },";
        html += "    body: JSON.stringify({ url: url })";
        html += "  });";
        html += "}";
        html += "function toggleGestureControl() {";
        html += "  const enabled = document.getElementById('gestureControl').checked;";
        html += "  fetch('/gesture', {";
        html += "    method: 'POST',";
        html += "    headers: { 'Content-Type': 'application/json' },";
        html += "    body: JSON.stringify({ enabled: enabled })";
        html += "  });";
        html += "}";
        html += "function updateAutoSettings() {";
        html += "  const data = {";
        html += "    enabled: document.getElementById('autoActivation').checked,";
        html += "    tempThreshold: parseFloat(document.getElementById('tempThreshold').value),";
        html += "    humThreshold: parseFloat(document.getElementById('humThreshold').value),";
        html += "    interval: parseInt(document.getElementById('checkInterval').value) * 1000";
        html += "  };";
        html += "  fetch('/autoSettings', {";
        html += "    method: 'POST',";
        html += "    headers: { 'Content-Type': 'application/json' },";
        html += "    body: JSON.stringify(data)";
        html += "  });";
        html += "}";
        html += "</script>";
        html += "</body>";
        html += "</html>";
        request->send(200, "text/html", html);
    });

    server.on("/state", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        DynamicJsonDocument doc(static_cast<size_t>(200));
        deserializeJson(doc, body);
        int speed = doc["speed"];
        setFanSpeed(speed);
        currentSpeed = speed;
        sendWebhookRequest(currentSpeed);
        notifyClients();
        request->send(200);
    });

    // Obsługa ustawiania domyślnego biegu
    server.on("/default", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        DynamicJsonDocument doc(static_cast<size_t>(200));
        deserializeJson(doc, body);
        int speed = doc["default"];
        if (speed < 0 || speed > 4) {
            request->send(400, "application/json", "{\"error\":\"Invalid speed\"}");
            return;
        }
        defaultSpeed = speed;
        preferences.putInt("defaultSpeed", defaultSpeed); // Zapisanie w pamięci
        Serial.printf("Ustawiono domyślny bieg na: %d\n", defaultSpeed);
        request->send(200);
    });

    server.on("/gesture", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        DynamicJsonDocument doc(static_cast<size_t>(200));
        deserializeJson(doc, body);
        gestureControlEnabled = doc["enabled"];
        preferences.putBool("gestureEnabled", gestureControlEnabled);  // Add this line
        notifyClients();  // Add this line
        request->send(200);
    });

    server.on("/autoSettings", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, 
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char *)data).substring(0, len);
            DynamicJsonDocument doc(200);
            deserializeJson(doc, body);
            
            autoActivationEnabled = doc["enabled"];
            tempRiseThreshold = doc["tempThreshold"];
            humRiseThreshold = doc["humThreshold"];
            monitoringInterval = doc["interval"];

            preferences.putBool("autoActivation", autoActivationEnabled);
            preferences.putFloat("tempThreshold", tempRiseThreshold);
            preferences.putFloat("humThreshold", humRiseThreshold);
            preferences.putULong("monitorInterval", monitoringInterval);

            notifyClients();
            request->send(200);
    });

    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
    server.begin();
}
