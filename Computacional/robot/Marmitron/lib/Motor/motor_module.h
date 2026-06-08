#pragma once
#include <Arduino.h>

class motor{
  private:
    int renable, rpwm, lpwm, lenable;

  public:
    motor(int enabler, int enablel, int pwmr, int pwml);

    void init();
    void acionaMotor(int pwm);
};