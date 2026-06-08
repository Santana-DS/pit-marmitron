#pragma once

#include <Arduino.h>
#include "motor.cpp"

void IRAM_ATTR lerEncoderISR();

void ControleDeVelocidade_setup(motor* motor1, int pino_encoder);
void ControleDeVelocidade_loop(motor* motor1, float vel_ref, int pino_encoder);
void acionarMotor(int pwm);
float zonaMorta(float sinal, float zona_morta);