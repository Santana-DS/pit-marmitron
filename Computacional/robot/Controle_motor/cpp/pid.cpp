#include "pid.h"

void pid_init(PID *pid, float kp, float ki, float kd, float min, float max) {
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->integral = 0;
    pid->erro_anterior = 0;
    pid->output_min = min;
    pid->output_max = max;
}

float pid_update(PID *pid, float erro, float dt) {
    pid->integral += erro * dt;

    if (pid->integral > 50)
        pid->integral = 50;

    if (pid->integral < -50)
        pid->integral = -50;

    float derivada = (erro - pid->erro_anterior) / dt;

    float output = pid->Kp * erro + pid->Ki * pid->integral + pid->Kd * derivada;

    // Clamp
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    pid->erro_anterior = erro;

    return output;
}