#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>

// Zewnętrzne zmienne globalne
extern int currentSpeed;
extern int defaultSpeed;
extern String webhookUrl;

// Funkcje do obsługi serwera WWW
void setupWebServer();
void sendWebhookRequest(int speed);
void notifyClients();
#endif
