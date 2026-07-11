#include "motor_module.h"
#include <Arduino.h>

const int FREQUENCIA_PWM = 20000; // 20 kHz tentativa de eliminar ruido do motor 
const int RESOLUCAO_PWM = 8;      // 8 bits (valores de comando vão de 0 a 255)

motor::motor(int rpwm, int lpwm){
  this->rpwm = rpwm;
  this->lpwm = lpwm;
}

void motor::init(){
  pinMode(rpwm, OUTPUT);
  pinMode(lpwm, OUTPUT);

  // ledcAttach(pino, frequência, resolução)
  ledcAttach(rpwm, FREQUENCIA_PWM, RESOLUCAO_PWM);
  ledcAttach(lpwm, FREQUENCIA_PWM, RESOLUCAO_PWM);

  // Garante que o motor inicie totalmente parado
  ledcWrite(rpwm, 0);
  ledcWrite(lpwm, 0);
}

void motor::acionaMotor(int pwm){
  if(pwm > 0){
    ledcWrite(lpwm, 0);
    ledcWrite(rpwm, pwm);
  }
  else if(pwm < 0){
    ledcWrite(rpwm, 0);
    ledcWrite(lpwm, abs(pwm));
  }else{
    ledcWrite(rpwm, 0);
    ledcWrite(lpwm, 0);
  }
}