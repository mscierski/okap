#ifndef CONFIG_H
#define CONFIG_H

// GPIO Pins for relays
#define RELAY_PIN1 5
#define RELAY_PIN2 32
#define RELAY_PIN3 33

// Default fan speed
#define DEFAULT_SPEED 1

// I2C Pins for VL53L0X
#define SDA_PIN 14
#define SCL_PIN 15


// Deklaracje globalnych zmiennych
extern int currentSpeed;
extern int defaultSpeed;
extern String webhookUrl; // Deklaracja zmiennej webhookUrl jako extern
#endif
