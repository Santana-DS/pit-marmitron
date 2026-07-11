#include <Arduino.h>
#include "pinout.h"
#include "config.h"

#include "imu_module.h"
#include "gps_module.h"
#include "sonar_module.h"


// ==========================================
// COMUNICAÇÃO ENTRE TAREFAS (FreeRTOS)
// ==========================================
// Criamos uma "caixa de correio" para a velocidade.
QueueHandle_t filaVelocidadeEsq;
QueueHandle_t filaVelocidadeDir;

// Identificadores das tarefas
TaskHandle_t TaskSensores;
TaskHandle_t TaskAccGyr;
TaskHandle_t TaskMag;
TaskHandle_t TaskGPS;
TaskHandle_t TaskSonar;
TaskHandle_t TaskMotores;

// ==========================================
// TAREFA 1: SENSORES (Rodando no CORE 1)
// ==========================================

void codigoTaskAccGyr(void * parameter) {
  imu_init();
  TickType_t tempoExecAnterior;
  const TickType_t periodoExec = (1000/FREQ_ACCGYR_HZ) / portTICK_PERIOD_MS;

  for(;;) {
    accgyr_update();
    vTaskDelayUntil(&tempoExecAnterior, periodoExec);
  }
}

void codigoTaskMag(void * parameter) {
  for(;;) {
    mag_update();
    vTaskDelay((1000/FREQ_MAG_HZ) / portTICK_PERIOD_MS); // Roda a 100Hz exatos
  }
}

void codigoTaskGPS(void * parameter) {
  gps_init();

  for(;;) {
    gps_update();
    vTaskDelay((1000/FREQ_GPS_HZ) / portTICK_PERIOD_MS); // Roda a 10Hz exatos
  }
}

void codigoTaskSonar(void * parameter) {
  sonar_init();

  for(;;) {
    sonar_update();
    vTaskDelay((1000/FREQ_SONAR_HZ) / portTICK_PERIOD_MS); // Roda a 20Hz exatos
  }
}

void codigoTaskSensores(void * parameter) {
  // 2. Loop infinito da tarefa
  TickType_t tempoExecAnterior;
  const TickType_t periodoExec = (1000/FREQ_SENSORES_HZ) / portTICK_PERIOD_MS;
  Serial.println("TaskSensores inicializada");
  for(;;) {
      // -> Transmitir dados para o ROS
      accgyr_print_ros_format();
      mag_print_ros_format();
      sonar_print_ros_format();
      gps_print_ros_format();
      Serial.println();
      vTaskDelayUntil(&tempoExecAnterior, periodoExec); 
  }
}


void setup() {
  Serial.begin(921600);
  delay(2000); 
  Serial.println("=== Iniciando Sistema (FreeRTOS) ===");

  // 1. Inicializa a Fila de Comunicação
  // Criamos espaço para guardar até 5 valores do tipo 'float'
  //filaVelocidadeEsq = xQueueCreate(5, sizeof(float));
  //filaVelocidadeDir = xQueueCreate(5, sizeof(float));


  // 3. Cria e lança a Tarefa dos Sensores no CORE 1
  xTaskCreate(
    codigoTaskSensores,  // Função da tarefa 
    "TaskSensores",      // Nome para debug 
    10000,               // Memória reservada (Stack) 
    NULL,                // Parâmetros extras 
    1,                   // Prioridade Normal (1)
    &TaskSensores);       // Handle da tarefa

  xTaskCreatePinnedToCore(
    codigoTaskAccGyr, 
    "TaskAccGyr", 
    10000, 
    NULL, 
    2,                   
    &TaskAccGyr, 
    1);                  // Fixado no CORE 1 

  xTaskCreatePinnedToCore( 
    codigoTaskMag, 
    "TaskMag", 
    10000, 
    NULL, 
    1,                   // Prioridade Normal (1) 
    &TaskMag, 
    1);                  // Fixado no CORE 1 

  xTaskCreatePinnedToCore(
    codigoTaskGPS, 
    "TaskGPS", 
    10000, 
    NULL, 
    3,                   // Prioridade Normal (3) 
    &TaskGPS, 
    1);                  // Fixado no CORE 1 

  xTaskCreatePinnedToCore(
    codigoTaskSonar, 
    "TaskSonar", 
    10000, 
    NULL, 
    3,                   // Prioridade Normal (3)
    &TaskSonar, 
    1);                 // Fixado no CORE 1

  Serial.println("Tarefas criadas. O FreeRTOS assumiu o controle.");
}

void loop() {
  // O loop padrão virou inútil porque nós criamos nossas próprias tarefas.
  // Deletamos a tarefa padrão do loop para devolver essa memória RAM ao ESP32.
  vTaskDelete(NULL);
}