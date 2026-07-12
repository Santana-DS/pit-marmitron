#pragma once 
#include <Arduino.h>

// ==========================================
// MPU9250 - BARRAMENTO VSPI
// ==========================================
constexpr uint8_t PIN_SPI_MOSI = 5; // SDA
constexpr uint8_t PIN_SPI_MISO = 6; // AD0
constexpr uint8_t PIN_SPI_SCK  = 4; // SCL
constexpr uint8_t PIN_SPI_CS   = 7;  // NCS

// ==========================================
// UBLOX NEO-6M - UART2
// ==========================================
constexpr uint8_t PIN_GPS_RX = 17; 
constexpr uint8_t PIN_GPS_TX = 18;

// ==========================================
// HC-SR04 - GPIO (ULTRASSÔNICO)
// ==========================================
constexpr int NUM_SONARS = 5;
constexpr uint8_t PIN_SONAR_TRIG[NUM_SONARS] = {9, 13, 10, 11, 12};
constexpr uint8_t PIN_SONAR_ECHO[NUM_SONARS] = {1, 2, 41, 40, 39}; 
