#include "encoder_module.h"

// ==========================================
// 1. CONSTRUTOR
// ==========================================
EncoderISR::EncoderISR(uint8_t pinA, float alpha_filtro) {
  this->pinoA = pinA;
  this->alpha = alpha_filtro;
  
  this->t_encoder = 0;
  this->t_anterior_encoder = 0;
  this->periodo_quarta_volta = 0;
  this->conta_encoder = 0;
  
  this->vel_filtrada = 0.0;
  this->mux = portMUX_INITIALIZER_UNLOCKED;
}

// ==========================================
// 2. AS ROTINAS DE INTERRUPÇÃO (O Segredo)
// ==========================================
// Esta é a função exigida pelo ESP32. Ela recebe 'arg', que é o próprio objeto!
void IRAM_ATTR EncoderISR::isrWrapper(void* arg) {
  // Converte o argumento genérico de volta para a classe EncoderISR
  EncoderISR* instancia = static_cast<EncoderISR*>(arg);
  
  // Chama a matemática real pertencente a esta roda específica
  instancia->rotinaInterrupcao();
}

// A sua lógica impecável de medição de período para evitar quantização
void IRAM_ATTR EncoderISR::rotinaInterrupcao() {
  this->conta_encoder++;
  if (this->conta_encoder >= 10) {
    this->t_encoder = micros();
    this->periodo_quarta_volta = this->t_encoder - this->t_anterior_encoder; 
    this->t_anterior_encoder = this->t_encoder;
    this->conta_encoder = 0;
  }
}

// ==========================================
// 3. INICIALIZAÇÃO E LEITURA
// ==========================================
void EncoderISR::init() {
  pinMode(this->pinoA, INPUT);
  
  // A MÁGICA: Passamos o pino, o wrapper, e o 'this' (a identidade desta roda)
  attachInterruptArg(digitalPinToInterrupt(this->pinoA), isrWrapper, this, CHANGE);
}

float EncoderISR::lerVelocidadeRPM() {
  unsigned long t_agora = micros();
  unsigned long periodo_copia = 0;
  unsigned long t_ultimo_pulso = 0;
  
  // Cópia segura das variáveis protegidas
  portENTER_CRITICAL(&this->mux);
  periodo_copia = this->periodo_quarta_volta;
  t_ultimo_pulso = this->t_anterior_encoder;
  portEXIT_CRITICAL(&this->mux);

  float vel_medida = 0.0;

  // Proteção do "Motor Parado" (Timeout de 200ms)
  if (t_agora - t_ultimo_pulso > 200000) {
    vel_medida = 0.0;
    portENTER_CRITICAL(&this->mux);
    this->periodo_quarta_volta = 0;
    portEXIT_CRITICAL(&this->mux);
  } 
  // O seu cálculo de RPM (40 furos lidos a cada 10)
  else if (periodo_copia != 0) {
    vel_medida = (float)60.0 * 1000000.0 / (4.0 * periodo_copia);
  }

  // O seu filtro passa-baixas
  this->vel_filtrada = (this->alpha * this->vel_filtrada) + ((1.0 - this->alpha) * vel_medida);

  return this->vel_filtrada;
}