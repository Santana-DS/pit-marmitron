#include <Arduino.h>

class motor{
  public:
    int renable, rpwm, lpwm, lenable;

    motor(int renable, int lenable, int rpwm, int lpwm){
      this->renable = renable;
      this->lenable = lenable;
      this->rpwm = rpwm;
      this->lpwm = lpwm;
    }

    void setup(){
      pinMode(renable, OUTPUT);
      pinMode(lenable, OUTPUT);
      pinMode(rpwm, OUTPUT);
      pinMode(lpwm, OUTPUT);

      digitalWrite(renable, HIGH);
      digitalWrite(lenable, HIGH);

      analogWrite(rpwm, 0);
      analogWrite(lpwm, 0);
    }

    void acionaMotor(int pwm){
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
};
