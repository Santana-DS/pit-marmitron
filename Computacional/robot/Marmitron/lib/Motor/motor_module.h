#pragma once
#include <Arduino.h>

class motor{
  private:
    int rpwm, lpwm;

  public:
    motor(int pwmr, int pwml);

    void init();
    void acionaMotor(int pwm);
};