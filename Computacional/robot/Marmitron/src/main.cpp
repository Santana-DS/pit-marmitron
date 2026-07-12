#include <Arduino.h>
#include "pinout.h"
#include "config.h"

#include "imu_module.h"
#include "gps_module.h"
#include "sonar_module.h"

#include "encoder_module.h"
#include "motor_module.h"
#include "ControlePID.h"

#include "webserver_module.h" 

motor motorEsq(PIN_PWMR_ESQ, PIN_PWML_ESQ);
motor motorDir(PIN_PWMR_DIR, PIN_PWML_DIR);

PIDController pidEsq(0.9, 0.1, 0.0); 
PIDController pidDir(0.9, 0.1, 0.0);

EncoderISR encEsq(PIN_ENC_ESQ, 0.1);
EncoderISR encDir(PIN_ENC_DIR, 0.1);

// ==========================================
// COMUNICAÇÃO ENTRE TAREFAS (FreeRTOS)
// ==========================================
QueueHandle_t filaVelocidade;

// Identificadores das tarefas
TaskHandle_t TaskSensores;
TaskHandle_t TaskMotores;

// ==========================================
// TAREFA 1: SENSORES (Rodando no CORE 1)
// ==========================================
void codigoTaskSensores(void * parameter) {
  imu_init();
  //gps_init();
  //sonar_init();

  TickType_t xUltimoDelay;
  TickType_t xFreq = pdMS_TO_TICKS(5);

  xUltimoDelay = xTaskGetTickCount();
  
  for(;;) {
    imu_update();
    //gps_update();
    //sonar_update();

    imu_print_ros_format();
    //gps_print_ros_format();
    //sonar_print_ros_format();

    vTaskDelayUntil(&xUltimoDelay, xFreq); 
  }
}

// ==========================================
// TAREFA 2: MOTORES & PID (Rodando no CORE 1)
// ==========================================
void codigoTaskMotores(void * parameter) {

  motorEsq.init();
  motorDir.init();
  encEsq.init();
  encDir.init();

  float vel_ref_esq = 0.0; 
  float vel_ref_dir = 0.0; 
  const float BASE_RPM = 255.0; 

  for(;;) {
    float comando;
    bool novo_comando = false;
    
    while (xQueueReceive(filaVelocidade, &comando, 0) == pdTRUE) {
      novo_comando = true;
    }

    if (novo_comando) {
      Serial.print("[TASK MOTORES] Comando Atualizado: ");
      Serial.println(comando);

      if (comando == 1.0) {         // FRENTE
        vel_ref_esq = BASE_RPM;
        vel_ref_dir = BASE_RPM;
      } 
      else if (comando == -1.0) {   // TRÁS
        vel_ref_esq = -BASE_RPM;
        vel_ref_dir = -BASE_RPM;
      } 
      else if (comando == 2.0) {    // ESQUERDA
        vel_ref_esq = -BASE_RPM;
        vel_ref_dir = BASE_RPM;
      } 
      else if (comando == 3.0) {    // DIREITA
        vel_ref_esq = BASE_RPM;
        vel_ref_dir = -BASE_RPM;
      } 
      else if (comando == 0.0) {    // PARAR
        vel_ref_esq = 0.0;
        vel_ref_dir = 0.0;
      }
    }

    // 2. Lê a velocidade real atual medida pelos encoders
    float rpm_esq = encEsq.lerVelocidadeRPM(); 
    float rpm_dir = encDir.lerVelocidadeRPM();

    // Atualiza o sentido com base no sinal da referência
    pidEsq.setSentido(vel_ref_esq);
    pidDir.setSentido(vel_ref_dir);

    // 3. O PID calcula o valor de PWM necessário
    int pwm_esq = pidEsq.controle(vel_ref_esq, rpm_esq);
    int pwm_dir = pidDir.controle(vel_ref_dir, rpm_dir);

    // CORREÇÃO 4: Garante corte elétrico imediato na ponte H se o alvo for zero
    if (vel_ref_esq == 0.0) pwm_esq = 0;
    if (vel_ref_dir == 0.0) pwm_dir = 0;

    // 4. Aplica os sinais calculados na ponte H
    motorEsq.acionaMotor(0);
    motorDir.acionaMotor(255);
    
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// SETUP PRINCIPAL (O MAESTRO)
// ==========================================
void setup() {
  Serial.begin(921600);
  delay(2000); 
  Serial.println("=== Iniciando Sistema (FreeRTOS) ===");

  // 1. Inicializa a Fila de Comunicação para até 5 comandos do tipo float
  filaVelocidade = xQueueCreate(5, sizeof(float));

  // 2. Inicializa o Webserver assíncrono (em modo Access Point)
  webserver_init(WIFI_SSID, WIFI_PASS);

  // 3. Cria e lança a Tarefa dos Sensores no CORE 1
  xTaskCreatePinnedToCore(
    codigoTaskSensores,  
    "TaskSensores",      
    10000,               
    NULL,                
    1,                   
    &TaskSensores,       
    1);                  

  // 4. Cria e lança a Tarefa dos Motores no CORE 1
  xTaskCreatePinnedToCore(
    codigoTaskMotores, 
    "TaskMotores", 
    10000, 
    NULL, 
    2,                   // Prioridade ALTA (2) - Malha de controle crítica!
    &TaskMotores, 
    1);                  

  Serial.println("Tarefas criadas. O FreeRTOS assumiu o controle.");
}

void loop() {
  // Deleta a tarefa padrão do loop para liberar recursos de memória RAM
  vTaskDelete(NULL);
}