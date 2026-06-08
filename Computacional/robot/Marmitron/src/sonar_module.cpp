#include "sonar_module.h"
#include "pinout.h"
#include <Arduino.h>

// ==========================================
// VARIÁVEIS INTERNAS DO MÓDULO
// ==========================================
static float distance_m = 0.0;

// O limite de tempo (em microssegundos) que vamos esperar o pulso.
// 30.000 us = 30 ms (equivalente a ~5 metros de distância de ida e volta).
static const unsigned long TIMEOUT_US = 30000; 

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

bool sonar_init() {
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  pinMode(PIN_SONAR_ECHO, INPUT);
  
  // Garante que o gatilho comece desligado
  digitalWrite(PIN_SONAR_TRIG, LOW);
  
  Serial.println("HC-SR04 inicializado.");
  return true;
}

void sonar_update() {
  // 1. Gera um pulso limpo de 10 microssegundos no pino TRIG
  digitalWrite(PIN_SONAR_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_SONAR_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_SONAR_TRIG, LOW);

  // 2. Mede o tempo que o pino ECHO fica em nível ALTO, com o nosso Timeout de segurança
  unsigned long duration_us = pulseIn(PIN_SONAR_ECHO, HIGH, TIMEOUT_US);

  // 3. Verifica se deu timeout (retorna 0) e calcula a distância
  if (duration_us == 0) {
    // Fora de alcance. Enviamos -1.0 para o ROS saber que deve ignorar essa leitura
    distance_m = -1.0; 
  } else {
    // Matemática:
    // Velocidade do som = 343 m/s ou 0.0343 cm/us
    // Distância (cm) = (duração * 0.0343) / 2 (ida e volta)
    float distance_cm = (duration_us * 0.0343) / 2.0;
    
    // Converte para metros para o ROS
    distance_m = distance_cm / 100.0;
  }
}

void sonar_print_ros_format() {
  // Imprime apenas um valor (Ex: 1.2500)
  Serial.println(distance_m, 4);
}