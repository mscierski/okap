#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <Preferences.h>

// Zewnętrzne zmienne globalne
extern int currentSpeed;
extern int defaultSpeed;
extern String webhookUrl;
extern float temperature;
extern float humidity;
extern bool gestureControlEnabled;
extern Preferences preferences;  // Add this line

// Funkcje do obsługi serwera WWW
void setupWebServer();
void sendWebhookRequest(int speed);
void notifyClients();
void updateSensorData();
void logGestureEvent(int oldSpeed, int newSpeed, const String& details);

// Keep the default argument in the declaration
void addLog(const String& cause, int fromSpeed, int toSpeed, const String& details = "");

// Remove the duplicate declaration and consolidate webhook functions
void sendWebhookRequest(int speed, const String& cause = "", int previousSpeed = -1);

void sendPeriodicWebhook();

#endif
