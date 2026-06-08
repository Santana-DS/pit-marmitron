#pragma once 
#include <Arduino.h>

// ==========================================
// MPU9250 - BARRAMENTO VSPI
// ==========================================
constexpr uint8_t PIN_SPI_MOSI = 23;
constexpr uint8_t PIN_SPI_MISO = 19;
constexpr uint8_t PIN_SPI_SCK  = 18;
constexpr uint8_t PIN_SPI_CS   = 5;

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
constexpr uint8_t PIN_ENC_ESQ = 34;
constexpr uint8_t PIN_ENC_DIR = 36; // Também conhecido como VP

// ==========================================
// Ponte H - GPIO (PWM)
// ==========================================
constexpr uint8_t PIN_PWMR_ESQ = 25;
constexpr uint8_t PIN_PWML_ESQ = 26;
constexpr uint8_t PIN_PWMR_DIR = 27;
constexpr uint8_t PIN_PWML_DIR = 14;
constexpr uint8_t PIN_ENR_ESQ = 12;
constexpr uint8_t PIN_ENL_ESQ = 13;
constexpr uint8_t PIN_ENR_DIR = 26;
constexpr uint8_t PIN_ENL_DIR = 25;