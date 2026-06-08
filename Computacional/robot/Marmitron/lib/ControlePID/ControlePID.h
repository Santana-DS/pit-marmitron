#pragma once
#include <Arduino.h>

class PIDController {
  private:
    float Kp, Ki, Kd;
    float erro_anterior;
    float erro_integral;
    unsigned long ultimo_tempo;
    int Sentido;
    // float limite_saida_min;
    // float limite_saida_max;

  public:
    PIDController(float kp, float ki, float kd);
    void setGanhos(float kp, float ki, float kd);
    void setSentido(int sentido);
    int controle(float vel_ref, float vel_filtrada);
    // void setLimites(float min, float max);
    // float computar(float valor_alvo, float valor_atual);
};