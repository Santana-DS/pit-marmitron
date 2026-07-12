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
  this->vel_medida = 0.0;
  this->t_ultima_leitura = micros();
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
  this->conta_encoder += 1;
  unsigned long t_atual = micros(); 
  if (this->conta_encoder >= 30){
      if (t_atual-this->t_anterior_encoder<500){
        this->conta_encoder = 0;
        this->t_anterior_encoder = t_atual;
        return; //Se o tempo entre contagens de 30 transições for muito pequeno (que resultaria em medida alta de velocidade) não atualiza periodo_quarta_volta.
      } 
      this->conta_encoder = 0;
      this->periodo_quarta_volta = t_atual - this->t_anterior_encoder;
      this->t_anterior_encoder = t_atual;
    }
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
  unsigned long t_ultimo_pulso;
  unsigned long periodo_copia = 0;
  int conta_copia;
  
  portENTER_CRITICAL(&this->mux);
  periodo_copia = this->periodo_quarta_volta;
  t_ultimo_pulso = this->t_anterior_encoder;
  conta_copia = this->conta_encoder;
  portEXIT_CRITICAL(&this->mux);
  
  //vel_medida é atualizada a taxa menor para que o filtro passa-baixas oscile lentamente entre os picos de medidas
  if (t_agora-t_ultima_leitura>150000 and periodo_copia != 0) {
    this->vel_medida = 60.0 * 1000000.0 /(4.0 * periodo_copia);
    this->t_ultima_leitura = t_agora;
    this->sequencia += 1;
  }

  this->timestamp = micros();
  this->vel_filtrada = (this->alpha * this->vel_filtrada) + ((1.0 - this->alpha) * this->vel_medida);

  if (t_agora-t_ultimo_pulso > 500000) {
    this->vel_filtrada = 0.0;
    //Serial.println("motor parado");
    this->t_ultima_leitura = t_agora;
    this->sequencia += 1;
    return 0.0;
  }

  //Serial.print(">periodo_quarta_volta:"); Serial.print(periodo_copia);
  //Serial.print(",conta_copia:"); Serial.print(conta_copia);
  //Serial.print(",vel_medida:"); Serial.print(vel_medida);
  //Serial.print(",vel_filtrada:"); Serial.println(vel_filtrada);
  return this->vel_filtrada;
}

void EncoderISR::printLeitura(float sentido_){
  float sentido = (sentido_ >=0) ? 1 : -1;
  Serial.print(this->vel_filtrada*sentido);
}