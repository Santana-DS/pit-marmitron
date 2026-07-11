#pragma once 
#include <Arduino.h>

// ==========================================
// MPU9250 - BARRAMENTO VSPI
// ==========================================
constexpr uint8_t PIN_SPI_MOSI = 23; // SDA
constexpr uint8_t PIN_SPI_MISO = 19; // AD0
constexpr uint8_t PIN_SPI_SCK  = 18; // SCL
constexpr uint8_t PIN_SPI_CS   = 5;  // NCS

// ==========================================
// UBLOX NEO-6M - UART2
// ==========================================
constexpr uint8_t PIN_GPS_RX = 17; // O pino 17 do ESP32 (TX2) vai no RX do GPS.
constexpr uint8_t PIN_GPS_TX = 16; // O pino 16 do ESP32 (RX2) vai no TX do GPS.

// ==========================================
// HC-SR04 - GPIO (ULTRASSÔNICO)
// ==========================================
constexpr uint8_t PIN_SONAR_TRIG = 32; // Utilizar divisor de tensão de 5V para 3.3V no pino ECHO!
constexpr uint8_t PIN_SONAR_ECHO = 33;

// ==========================================
// Encoders - GPIO (PCNT)
// ==========================================
constexpr uint8_t PIN_ENC_ESQ = 33;
constexpr uint8_t PIN_ENC_DIR = 32;

// ==========================================
// Ponte H - GPIO (PWM)
// ==========================================
constexpr uint8_t PIN_PWMR_ESQ = 25;
constexpr uint8_t PIN_PWML_ESQ = 26;
constexpr uint8_t PIN_PWMR_DIR = 27;
constexpr uint8_t PIN_PWML_DIR = 14;