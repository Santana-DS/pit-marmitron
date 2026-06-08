#ifndef PID_H
#define PID_H

typedef struct {
    float Kp, Ki, Kd;
    float integral;
    float erro_anterior;
    float output_min;
    float output_max;
} PID;

void pid_init(PID *pid, float kp, float ki, float kd, float min, float max);
float pid_update(PID *pid, float erro, float dt);

#endif