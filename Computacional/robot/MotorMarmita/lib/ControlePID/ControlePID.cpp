#include "ControlePID.h"

PIDController::PIDController(float kp, float ki, float kd) {
  this->Kp = kp;
  this->Ki = ki;
  this->Kd = kd;
  this->erro_anterior = 0.0;
  this->erro_integral = 0.0;
  this->Sentido = 1; // 1 para frente, -1 para ré
  // this->limite_saida_min = -255.0; // Padrão genérico de PWM
  // this->limite_saida_max = 255.0;
  this->ultimo_tempo = micros();
}

void PIDController::setGanhos(float kp, float ki, float kd) {
  this->Kp = kp;
  this->Ki = ki;
  this->Kd = kd;
}

void PIDController::setSentido(int sentido) {
  this->Sentido = (sentido >= 0) ? 1 : -1;
}

int PIDController::controle(float vel_ref,float vel_filtrada){
  unsigned long tempo_agora = micros();
  
  float dt = (float)(tempo_agora - ultimo_tempo) / 1.0e6; // Converte para segundos
  if (dt <= 0.0) dt = 0.0001;
  
  if (vel_ref == 0.0) {
    erro_integral = 0.0;
  }

  float erro = abs(vel_ref) - abs(vel_filtrada);

  erro = (abs(erro) < 0.001) ? 0.0 : erro; // Zona morta de 0.001 para evitar ruído
  erro_integral += erro * dt;
  //erro_integral = constrain(erro_integral, -150.0, 150.0);

  float P = Kp * erro;
  float I = Ki * erro_integral;
  float D = Kd * ((erro - erro_anterior) / dt);

  float saida = P + I + D;
  int pwm = (int) constrain(saida, 0, 255);

  ultimo_tempo = tempo_agora;
  erro_anterior = erro;

  //Serial.print(">vel_filtrada:");Serial.print(vel_filtrada);Serial.print(",vel_ref:");Serial.print(vel_ref);
  //Serial.print(",P:"); Serial.print(P); Serial.print(",I:"); Serial.print(",D:"); Serial.print(D);
  //Serial.print(",integral:");Serial.print(this->erro_integral);
  //Serial.print(",sinalPID:");Serial.println(saida);
  return Sentido*pwm;
}

void PIDController::reset(){
  erro_integral = 0.0;
  erro_anterior = 0.0;
}