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

void updateSensorData() {
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    notifyClients();
}

void setupWebServer() {
    preferences.begin("okap", false);

    // Wczytanie konfiguracji z pamięci
    webhookUrl = preferences.getString("webhook", "");
    defaultSpeed = preferences.getInt("defaultSpeed", 1);
    gestureControlEnabled = preferences.getBool("gestureEnabled", true);  // Add this line
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

    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
    server.begin();
}
