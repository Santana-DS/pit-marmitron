#include "sonar_module.h"
#include "pinout.h"
#include <Arduino.h>

// ==========================================
// VARIÁVEIS INTERNAS (Arrays)
// ==========================================
static float distance_m[NUM_SONARS];

// Variáveis voláteis modificadas pela ISR
static volatile unsigned long echo_start_us[NUM_SONARS];
static volatile unsigned long echo_duration_us[NUM_SONARS];
static volatile bool novo_dado_disponivel[NUM_SONARS];

// Fechaduras do FreeRTOS
static portMUX_TYPE muxSonar = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE muxSonarPrint = portMUX_INITIALIZER_UNLOCKED;

uint32_t timestamp_sonar;
uint32_t seq_sonar = 0;

// Variável que diz de quem é a vez de atirar na rodada (Round-Robin)
static int sonar_da_vez = 0; 

// ==========================================
// ROTINA DE INTERRUPÇÃO (ISR) ÚNICA
// ==========================================
void IRAM_ATTR sonar_echo_isr(void* arg) {
  // O cast duplo garante que não há avisos de compilação em arquiteturas de 32-bits
  int id = (int)(intptr_t)arg; 
  unsigned long tempo_agora = micros();
  
  if (digitalRead(PIN_SONAR_ECHO[id]) == HIGH) {
    echo_start_us[id] = tempo_agora;
  } 
  else {
    if (echo_start_us[id] != 0) {
      echo_duration_us[id] = tempo_agora - echo_start_us[id];
      novo_dado_disponivel[id] = true;
    }
  }
}

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

bool sonar_init() {
  for(int i = 0; i < NUM_SONARS; i++) {
    pinMode(PIN_SONAR_TRIG[i], OUTPUT);
    pinMode(PIN_SONAR_ECHO[i], INPUT);
    
    // Garante que todos iniciem em silêncio
    digitalWrite(PIN_SONAR_TRIG[i], LOW);
    distance_m[i] = -1.0;
    
    // Passa o ID do sonar para a interrupção associada
    attachInterruptArg(digitalPinToInterrupt(PIN_SONAR_ECHO[i]), sonar_echo_isr, (void*)(intptr_t)i, CHANGE);
  }
  
  Serial.print("Inicializados ");
  Serial.print(NUM_SONARS);
  Serial.println(" sensores HC-SR04 em modo Sequencial (Round-Robin).");
  return true;
}

void sonar_update() {
  // 1. LÊ OS RESULTADOS DE TODOS OS SENSORES
  for(int i = 0; i < NUM_SONARS; i++) {
    unsigned long duracao_copia = 0;
    bool dados_novos = false;

    portENTER_CRITICAL(&muxSonar);
    dados_novos = novo_dado_disponivel[i];
    duracao_copia = echo_duration_us[i];
    novo_dado_disponivel[i] = false;
    
    // Regista o timestamp apenas uma vez por ciclo completo de atualização
    if (i == 0) timestamp_sonar = micros();
    portEXIT_CRITICAL(&muxSonar);

    if (dados_novos && duracao_copia < 30000) { 
      float distance_cm = (duracao_copia * 0.0343) / 2.0;
      
      portENTER_CRITICAL(&muxSonarPrint);
      distance_m[i] = distance_cm / 100.0;
      portEXIT_CRITICAL(&muxSonarPrint);
      
    } else if (dados_novos) { // Recebeu dado, mas extrapolou o limite de segurança (Timeout)
      portENTER_CRITICAL(&muxSonarPrint);
      distance_m[i] = -1.0;
      portEXIT_CRITICAL(&muxSonarPrint);
    }
  }

  // Incrementa a sequência global do pacote
  portENTER_CRITICAL(&muxSonarPrint);
  seq_sonar += 1;
  portEXIT_CRITICAL(&muxSonarPrint);

  // 2. DISPARA APENAS O SONAR DA VEZ
  digitalWrite(PIN_SONAR_TRIG[sonar_da_vez], LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_SONAR_TRIG[sonar_da_vez], HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_SONAR_TRIG[sonar_da_vez], LOW);

  // 3. PASSA A VEZ PARA O PRÓXIMO SENSOR (Avança a esteira)
  sonar_da_vez++;
  if (sonar_da_vez >= NUM_SONARS) {
    sonar_da_vez = 0;
  }
}

void sonar_print_ros_format() {
  uint32_t timestamp, seq;
  float distancias[NUM_SONARS];

  // Cópia da RAM protegida para evitar corrupção durante a impressão
  portENTER_CRITICAL(&muxSonarPrint);
  timestamp = timestamp_sonar;
  seq = seq_sonar;
  for(int i = 0; i < NUM_SONARS; i++) {
    distancias[i] = distance_m[i];
  }
  portEXIT_CRITICAL(&muxSonarPrint);
  
  // Imprime no formato compatível: seq,timestamp,dist1,dist2,dist3,dist4,dist5,
  Serial.print(seq); Serial.print(",");
  Serial.print(timestamp); Serial.print(",");
  
  for(int i = 0; i < NUM_SONARS; i++) {
    Serial.print(distancias[i], 4);
    Serial.print(","); 
  }
}