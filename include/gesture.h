#ifndef GESTURE_H
#define GESTURE_H

#include <Adafruit_VL53L0X.h>

// Deklaracja zewnętrzna sensora VL53L0X
extern Adafruit_VL53L0X lox;

// Funkcje związane z gestami
void setupGesture();
void processGesture();

// Deklaracje globalnych zmiennych związanych z gestami
extern unsigned long gestureStartTime;
extern bool gestureDetected;
extern bool holdDetected;
extern const int gestureThreshold;
extern const int holdThreshold;

#endif
