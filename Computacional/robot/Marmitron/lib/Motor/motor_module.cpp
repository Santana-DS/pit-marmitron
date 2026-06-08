#include "motor_module.h"

motor::motor(int renable, int lenable, int rpwm, int lpwm){
  this->renable = renable;
  this->lenable = lenable;
  this->rpwm = rpwm;
  this->lpwm = lpwm;
}

void motor::init(){
  pinMode(renable, OUTPUT);
  pinMode(lenable, OUTPUT);
  pinMode(rpwm, OUTPUT);
  pinMode(lpwm, OUTPUT);

  digitalWrite(renable, HIGH);
  digitalWrite(lenable, HIGH);

  analogWrite(rpwm, 0);
  analogWrite(lpwm, 0);
}

void motor::acionaMotor(int pwm){
  if(pwm > 0){
    analogWrite(lpwm, 0);
    analogWrite(rpwm, pwm);
  }
  else if(pwm < 0){
    analogWrite(rpwm, 0);
    analogWrite(lpwm, abs(pwm));
  }else{
    analogWrite(rpwm, 0);
    analogWrite(lpwm, 0);
  }
}
