#include "motor.h"
#include <cmath>

// Modelo simples de motor (1ª ordem)
float motor_update(float pwm, float velocidade, float dt) {

    // Saturação PWM
    if (pwm > 255) pwm = 255;
    if (pwm < -255) pwm = -255;

    // Zona morta
    if (abs(pwm) < 20)
        pwm = 0;

    // Conversão PWM → torque
    float torque = pwm * 0.01;

    // Atrito
    float atrito = 0.5 * velocidade;

    // Dinâmica
    float aceleracao = torque - atrito;

    // Integra velocidade
    velocidade += aceleracao * dt;

    return velocidade;
}