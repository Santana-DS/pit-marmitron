#pragma once 
#include <Arduino.h>

// ==========================================
// Encoders - GPIO (PCNT)
// ==========================================
constexpr uint8_t PIN_ENC_ESQ = 33;
constexpr uint8_t PIN_ENC_DIR = 32; // Também conhecido como VP

// ==========================================
// Ponte H - GPIO (PWM)
// ==========================================
constexpr uint8_t PIN_PWMR_ESQ = 25;
constexpr uint8_t PIN_PWML_ESQ = 26;
constexpr uint8_t PIN_PWMR_DIR = 27;
constexpr uint8_t PIN_PWML_DIR = 14;