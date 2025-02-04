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
    
    // Initialize last values if they are zero (first run)
    if (lastTemperature == 0) {
        lastTemperature = newTemperature;
        lastHumidity = newHumidity;
        lastMonitoringTime = millis();
        temperature = newTemperature;
        humidity = newHumidity;
        notifyClients();
        return; // Skip the first reading to avoid false triggers
    }
    
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
        html += "<link href=\"https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500&display=swap\" rel=\"stylesheet\">";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Roboto', sans-serif; background-color: #121212; color: #ffffff; }";
        html += "h1 { font-size: 3.5em; font-weight: 700; margin: 0; text-align: center; letter-spacing: 8px; transform: scaleX(1.2); text-transform: uppercase; }";
        html += ".title-card { background: #1e1e1e; border-radius: 8px; padding: 15px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.2); }";
        html += "h3 { font-size: 1.2em; font-weight: 400; margin: 15px 0; }";
        html += ".container { max-width: 600px; margin: 0 auto; padding: 20px; }";
        html += ".card { background: #1e1e1e; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.2); }";
        html += ".separator { height: 1px; background: #303030; margin: 15px 0; }";
        html += ".speed-btns { display: flex; gap: 8px; justify-content: center; margin: 15px 0; }";
        html += ".btn { background: #0288d1; color: white; border: none; padding: 12px 24px; border-radius: 4px; cursor: pointer; transition: background 0.3s; }";
        html += ".btn:hover { background: #039be5; }";
        html += ".btn-off { background: #424242; }";
        html += ".btn-off:hover { background: #616161; }";
        html += ".current-speed { font-size: 3.5em; font-weight: 300; color: #0288d1; margin: 20px 0; text-align: center; }";
        html += ".sensor-value { font-size: 1.5em; color: #0288d1; }";
        html += ".speed-indicator { display: flex; justify-content: center; gap: 4px; margin: 15px 0; }";
        html += ".speed-bar { width: 8px; height: 30px; background: #303030; border-radius: 4px; }";
        html += ".speed-bar.active { background: #0288d1; }";
        html += ".switch { position: relative; display: inline-block; width: 60px; height: 34px; }";
        html += ".switch input { opacity: 0; width: 0; height: 0; }";
        html += ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #303030; transition: .4s; border-radius: 34px; }";
        html += ".slider:before { position: absolute; content: \"\"; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }";
        html += "input:checked + .slider { background-color: #0288d1; }";
        html += "input:checked + .slider:before { transform: translateX(26px); }";
        html += ".setting-row { display: flex; justify-content: space-between; align-items: center; margin: 10px 0; }";
        html += "input[type=\"number\"], input[type=\"text\"] { background: #303030; border: none; color: white; padding: 8px; border-radius: 4px; width: 120px; }";
        html += "</style>";
        
        html += "<body>";
        html += "<div class=\"container\">";
        
        // Separate title card
        html += "<div class=\"title-card\">";
        html += "<h1>OKAP</h1>";
        html += "</div>";
        
        // Speed control card (remove the title from here)
        html += "<div class=\"card\">";
        html += "<div class=\"current-speed\" id=\"currentSpeed\">" + String(currentSpeed == 0 ? "OFF" : String(currentSpeed)) + "</div>";
        html += "<div class=\"speed-indicator\">";
        for (int i = 1; i <= 4; i++) {
            html += "<div class=\"speed-bar" + String(currentSpeed >= i ? " active" : "") + "\"></div>";
        }
        html += "</div>";
        // 3. Speed Control Buttons
        html += "<div class=\"speed-btns\">";
        html += "<button class=\"btn btn-off\" onclick=\"setSpeed(0)\">OFF</button>";
        for (int i = 1; i <= 4; i++) {
            html += "<button class=\"btn\" onclick=\"setSpeed(" + String(i) + ")\">" + String(i) + "</button>";
        }
        html += "</div>";
        html += "</div>";

        // 4. Temperature and Humidity
        html += "<div class=\"card\">";
        html += "<div class=\"setting-row\">";
        html += "<h3>Temperatura:</h3>";
        html += "<span class=\"sensor-value\" id=\"temperature\">" + String(temperature, 1) + " °C</span>";
        html += "</div>";
        html += "<div class=\"setting-row\">";
        html += "<h3>Wilgotność:</h3>";
        html += "<span class=\"sensor-value\" id=\"humidity\">" + String(humidity, 1) + " %</span>";
        html += "</div>";
        html += "</div>";

        // 5. Gesture Control
        html += "<div class=\"card\">";
        html += "<div class=\"setting-row\">";
        html += "<h3>Sterowanie gestami</h3>";
        html += "<label class=\"switch\"><input type=\"checkbox\" id=\"gestureControl\" " + String(gestureControlEnabled ? "checked" : "") + " onchange=\"toggleGestureControl()\"><span class=\"slider\"></span></label>";
        html += "</div>";
        html += "</div>";

        // 6. Automation Control and 7. Settings
        html += "<div class=\"card\">";
        html += "<div class=\"setting-row\">";
        html += "<h3>Automatyka</h3>";
        html += "<label class=\"switch\"><input type=\"checkbox\" id=\"autoActivation\" " + String(autoActivationEnabled ? "checked" : "") + " onchange=\"updateAutoSettings()\"><span class=\"slider\"></span></label>";
        html += "</div>";
        html += "<div class=\"separator\"></div>";
        html += "<div class=\"setting-row\">";
        html += "<label>Próg temperatury (°C/min):</label>";
        html += "<input type=\"number\" id=\"tempThreshold\" value=\"" + String(tempRiseThreshold) + "\" step=\"0.1\" min=\"0.1\" max=\"10\">";
        html += "</div>";
        html += "<div class=\"setting-row\">";
        html += "<label>Próg wilgotności (%/min):</label>";
        html += "<input type=\"number\" id=\"humThreshold\" value=\"" + String(humRiseThreshold) + "\" step=\"0.1\" min=\"0.1\" max=\"20\">";
        html += "</div>";
        html += "<div class=\"setting-row\">";
        html += "<label>Interwał sprawdzania (s):</label>";
        html += "<input type=\"number\" id=\"checkInterval\" value=\"" + String(monitoringInterval/1000) + "\" min=\"1\" max=\"60\">";
        html += "</div>";
        html += "<button class=\"btn\" onclick=\"updateAutoSettings()\">Zapisz ustawienia</button>";
        html += "</div>";

        // Default Speed Setting (before webhook)
        html += "<div class=\"card\">";
        html += "<div class=\"setting-row\">";
        html += "<h3>Domyślny bieg:</h3>";
        html += "<input type=\"number\" id=\"defaultInput\" min=\"1\" max=\"4\" value=\"" + String(defaultSpeed) + "\">";
        html += "<button class=\"btn\" onclick=\"setDefault()\">Ustaw</button>";
        html += "</div>";
        html += "</div>";

        // 8. Webhook
        html += "<div class=\"card\">";
        html += "<h3>Webhook URL:</h3>";
        html += "<div class=\"setting-row\">";
        html += "<input type=\"text\" id=\"webhookUrl\" placeholder=\"Podaj adres webhooka\" value=\"" + webhookUrl + "\">";
        html += "<button class=\"btn\" onclick=\"setWebhook()\">Zapisz</button>";
        html += "</div>";
        html += "</div>";

        html += "</div>"; // Close container

        // Keep existing JavaScript
        html += "<script>";
        html += "const ws = new WebSocket('ws://' + location.host + '/ws');";
        html += "ws.onmessage = function(event) {";
        html += "  const data = JSON.parse(event.data);";
        html += "  document.getElementById('currentSpeed').innerText = data.currentSpeed === 0 ? 'OFF' : data.currentSpeed;";
        html += "  const bars = document.querySelectorAll('.speed-bar');";
        html += "  bars.forEach((bar, index) => {";
        html += "    bar.classList.toggle('active', data.currentSpeed > index);";
        html += "  });";
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
