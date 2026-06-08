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


motor motorEsq(PIN_ENR_ESQ, PIN_ENL_ESQ, PIN_PWMR_ESQ, PIN_PWML_ESQ);
motor motorDir(PIN_ENR_DIR, PIN_ENL_DIR, PIN_PWMR_DIR, PIN_PWML_DIR);

PIDController pidEsq(2.0, 0.5, 0.1); 
PIDController pidDir(2.0, 0.5, 0.1);

EncoderISR encEsq(PIN_ENC_ESQ);
EncoderISR encDir(PIN_ENC_DIR);

// ==========================================
// COMUNICAÇÃO ENTRE TAREFAS (FreeRTOS)
// ==========================================
// Criamos uma "caixa de correio" para a velocidade.
QueueHandle_t filaVelocidade;

// Identificadores das tarefas
TaskHandle_t TaskSensores;
TaskHandle_t TaskMotores;

// ==========================================
// TAREFA 1: SENSORES (Rodando no CORE 1)
// ==========================================
void codigoTaskSensores(void * parameter) {
  // 1. Setup exclusivo desta tarefa
  imu_init();
  gps_init();
  sonar_init();

  // 2. Loop infinito da tarefa
  for(;;) {
    // -> Coletar dados dos sensores
    imu_update();
    gps_update();
    sonar_update();

    // -> Transmitir dados para o ROS
    Serial.println("---");
    imu_print_ros_format();
    gps_print_ros_format();
    sonar_print_ros_format();

    vTaskDelay(5 / portTICK_PERIOD_MS); 
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

  float vel_ref = 0.0; // RPM alvo

  for(;;) {

    float nova_vel;
    if (xQueueReceive(filaVelocidade, &nova_vel, 0) == pdTRUE) {
      vel_ref = nova_vel;
      Serial.print("[TASK MOTORES] Nova Velocidade Alvo: ");
      Serial.println(vel_ref);
    }
    // 1. Lê a velocidade de forma totalmente independente e paralela!
    float rpm_esq = encEsq.lerVelocidadeRPM(); 
    float rpm_dir = encDir.lerVelocidadeRPM();

    // 2. O PID calcula o PWM
    int pwm_esq = pidEsq.controle(vel_ref, rpm_esq);
    int pwm_dir = pidDir.controle(vel_ref, rpm_dir);

    // 3. Aplica na ponte H
    motorEsq.acionaMotor(pwm_esq);
    motorDir.acionaMotor(pwm_dir);

    vTaskDelay(10 / portTICK_PERIOD_MS); // Roda a 100Hz exatos
  }
}

// ==========================================
// SETUP PRINCIPAL (O MAESTRO)
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("=== Iniciando Sistema (FreeRTOS) ===");

  // 1. Inicializa a Fila de Comunicação
  // Criamos espaço para guardar até 5 valores do tipo 'float'
  filaVelocidade = xQueueCreate(5, sizeof(float));

  // 2. Inicializa o Webserver (Geralmente ele roda de forma assíncrona no Core 0)
  webserver_init(WIFI_SSID, WIFI_PASS);

  // 3. Cria e lança a Tarefa dos Sensores no CORE 1
  xTaskCreatePinnedToCore(
    codigoTaskSensores,  /* Função da tarefa */
    "TaskSensores",      /* Nome para debug */
    10000,               /* Memória reservada (Stack) */
    NULL,                /* Parâmetros extras */
    1,                   /* Prioridade Normal (1) */
    &TaskSensores,       /* Handle da tarefa */
    1);                  /* Fixado no CORE 1 */

  // 4. Cria e lança a Tarefa dos Motores no CORE 1
  xTaskCreatePinnedToCore(
    codigoTaskMotores, 
    "TaskMotores", 
    10000, 
    NULL, 
    2,                   /* Prioridade ALTA (2) - Motores são críticos! */
    &TaskMotores, 
    1);                  /* Fixado no CORE 1 */

  Serial.println("Tarefas criadas. O FreeRTOS assumiu o controle.");
}

void loop() {
  // O loop padrão virou inútil porque nós criamos nossas próprias tarefas.
  // Deletamos a tarefa padrão do loop para devolver essa memória RAM ao ESP32.
  vTaskDelete(NULL);
}