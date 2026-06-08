#pragma once

#include <Arduino.h>
#include "motor_module.h"

void IRAM_ATTR lerEncoderISR();

void ControleDeVelocidade_setup(motor* motor1);
void ControleDeVelocidade_loop(motor* motor1, float vel_ref);
void acionarMotor(int pwm);
float zonaMorta(float sinal, float zona_morta);