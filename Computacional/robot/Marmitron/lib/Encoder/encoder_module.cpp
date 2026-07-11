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
  unsigned long t_atual = micros();
  unsigned long delta = t_atual - this->t_anterior_encoder;

  // =========================================================================
  // FILTRO DE DEBOUNCE (Rejeição de Ruído Elétrico)
  // Se o intervalo for menor que 500us, o motor teria que estar a > 3000 RPM.
  // Como isso é impossível, descartamos o pulso falso imediatamente.
  // =========================================================================
  if (delta < 500) {
    return; // Ignora o ruído e sai da interrupção
  }

  this->t_encoder = t_atual;
  this->periodo_quarta_volta = delta; 
  this->t_anterior_encoder = this->t_encoder;
}

// ==========================================
// 3. INICIALIZAÇÃO E LEITURA
// ==========================================
void EncoderISR::init() {
  pinMode(this->pinoA, INPUT);
  // Alterado para RISING para maior precisão física e leitura constante
  attachInterruptArg(digitalPinToInterrupt(this->pinoA), isrWrapper, this, CHANGE);
}

float EncoderISR::lerVelocidadeRPM() {
  unsigned long t_agora = micros();
  unsigned long periodo_copia = 0;
  unsigned long t_ultimo_pulso = 0;
  
  portENTER_CRITICAL(&this->mux);
  periodo_copia = this->periodo_quarta_volta;
  t_ultimo_pulso = this->t_anterior_encoder;
  portEXIT_CRITICAL(&this->mux);

  // Quanto tempo se passou desde o último pulso físico recebido?
  unsigned long delta_t = t_agora - t_ultimo_pulso;

  // 1. Proteção contra Motor Parado (Timeout reduzido para 100ms para resposta mais esperta)
  if (delta_t > 100000) {
    vel_filtrada = 0.0;
    return 0.0;
  } 

  // 2. MÁGICA DO DECAIMENTO: Se o delta_t atual já superou o último período, 
  // significa que o motor está desacelerando. Forçamos o período a crescer.
  if (delta_t > periodo_copia) {
    periodo_copia = delta_t;
  }

  float vel_medida = 0.0;
  if (periodo_copia != 0) {
    // Fórmula: (60 segundos * 1.000.000 microssegundos) / (60 mudanças * periodo)
    // Simplificando: 2*1.000.000 / periodo
    vel_medida = (float)1000000.0 / periodo_copia;
  }

  Serial.print(">periodo_copia"); Serial.print(periodo_copia);
  Serial.print(",vel_medida:"); Serial.print(vel_medida);
  Serial.print(",vel_filtrada:"); Serial.println(vel_filtrada);
  this->vel_filtrada = (this->alpha * this->vel_filtrada) + ((1.0 - this->alpha) * vel_medida);

  return this->vel_filtrada;
}