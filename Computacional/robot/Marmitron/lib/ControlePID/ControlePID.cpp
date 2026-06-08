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
  this->ultimo_tempo = millis();
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
  float erro = vel_ref - vel_filtrada;
  erro = (abs(erro) < 0.001) ? 0.0 : erro; // Zona morta de 0.001 para evitar ruído
  erro_integral += erro * dt;

  float P = Kp * erro;
  float I = Ki * erro_integral;
  float D = Kd * ((erro - erro_anterior) / dt);

  float saida = P + I + D;
  int pwm = (int) constrain(saida, 0, 255);

  ultimo_tempo = tempo_agora;
  erro_anterior = erro;

  return Sentido*pwm;
}

// void PIDController::setLimites(float min, float max) {
//   this->limite_saida_min = min;
//   this->limite_saida_max = max;
// }


// float PIDController::computar(float valor_alvo, float valor_atual) {
//   unsigned long tempo_agora = millis();
//   // Calcula a diferença de tempo (dt) em segundos
//   float dt = (float)(tempo_agora - ultimo_tempo) / 1000.0;
//   if (dt <= 0.0) return 0.0; // Evita divisão por zero se rodar rápido demais
//   // 1. Calcula o Erro
//   float erro = valor_alvo - valor_atual;
//   // 2. Proporcional (Força bruta de correção)
//   float P = Kp * erro;
//   // 3. Integral (Acumula o erro no tempo para vencer resistências, como rampas)
//   erro_integral += erro * dt;
//   float I = Ki * erro_integral;
//   // 4. Derivativo (Prevê o futuro e freia o sistema para não passar do alvo)
//   float D = Kd * ((erro - erro_anterior) / dt);
//   // 5. Soma tudo
//   float saida = P + I + D;
//   // 6. Saturação (Anti-Windup básico): Impede que o PID peça mais que o motor aguenta
//   if (saida > limite_saida_max) saida = limite_saida_max;
//   if (saida < limite_saida_min) saida = limite_saida_min;
//   // Guarda os dados para a próxima rodada
//   erro_anterior = erro;
//   ultimo_tempo = tempo_agora;
//   return saida;
// }
