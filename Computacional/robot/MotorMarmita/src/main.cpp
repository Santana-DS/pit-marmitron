#include <Arduino.h>
#include "pinout.h"
#include "config.h"

#include "encoder_module.h"
#include "motor_module.h"
#include "ControlePID.h"

#include "webserver_module.h" 
#include "lock_service.h"


motor motorEsq(PIN_PWMR_ESQ, PIN_PWML_ESQ);
motor motorDir(PIN_PWMR_DIR, PIN_PWML_DIR);

PIDController pidEsq(KPesq, KIesq, KDesq); //0.9 0.1 0.0
PIDController pidDir(KPdir, KIdir, KDdir);

EncoderISR encEsq(PIN_ENC_ESQ, 0.95);
EncoderISR encDir(PIN_ENC_DIR, 0.95);

// ==========================================
// COMUNICAÇÃO ENTRE TAREFAS (FreeRTOS)
// ==========================================
// Criamos uma "caixa de correio" para a velocidade.
QueueHandle_t filaVelocidadeEsq;
QueueHandle_t filaVelocidadeDir;

// Identificadores das tarefas
TaskHandle_t TaskEncoders;
TaskHandle_t TaskMotores;
TaskHandle_t TaskInterface;

static LockService lockService;

// ==========================================
// TAREFA 1: MOTORES & PID (Rodando no CORE 1)
// ==========================================
void codigoTaskMotores(void * parameter) {

  motorEsq.init();
  motorDir.init();
  encEsq.init();
  encDir.init();

  float vel_ref_esq = 0.0; // RPM alvo
  float vel_ref_dir = 0.0; 
  float rpm_esq = 0.0;
  float rpm_dir = 0.0;
  const float BASE_RPM = 200.0;

  for(;;) {
    float comando;
    bool novo_comando = false;
    
    while (xQueueReceive(filaVelocidadeEsq, &comando, 0) == pdTRUE) {
      novo_comando = true;
    }

    if (novo_comando) {
      Serial.print("[TASK MOTORES] Comando Atualizado: ");
      Serial.println(comando);

      if (comando == 1.0) {         // FRENTE
        vel_ref_esq = BASE_RPM;
        vel_ref_dir = BASE_RPM;
        pidEsq.reset();
        pidDir.reset();
      } 
      else if (comando == -1.0) {   // TRÁS
        vel_ref_esq = -BASE_RPM;
        vel_ref_dir = -BASE_RPM;
        pidEsq.reset();
        pidDir.reset();
      } 
      else if (comando == 2.0) {    // ESQUERDA
        vel_ref_esq = -BASE_RPM;
        vel_ref_dir = BASE_RPM;
        pidEsq.reset();
        pidDir.reset();
      } 
      else if (comando == 3.0) {    // DIREITA
        vel_ref_esq = BASE_RPM;
        vel_ref_dir = -BASE_RPM;
        pidEsq.reset();
        pidDir.reset();
      } 
      else if (comando == 0.0) {    // PARAR
        vel_ref_esq = 0.0;
        vel_ref_dir = 0.0;
        pidEsq.reset();
        pidDir.reset();
      }
    }

    // 2. Lê a velocidade real atual medida pelos encoders
    float rpm_esq = encEsq.lerVelocidadeRPM(); 
    float rpm_dir = encDir.lerVelocidadeRPM();

    //Printa leituras encoders para uso na odometria
    Serial.print(encEsq.timestamp); Serial.print(",");
    Serial.print(encEsq.sequencia); Serial.print(",");
    encEsq.printLeitura(vel_ref_esq); Serial.print(",");
    Serial.print(encDir.timestamp); Serial.print(",");
    Serial.print(encDir.sequencia); Serial.print(",");
    encDir.printLeitura(vel_ref_dir); Serial.println();

    // Atualiza o sentido com base no sinal da referência
    pidEsq.setSentido(vel_ref_esq);
    pidDir.setSentido(vel_ref_dir);

    // 3. O PID calcula o valor de PWM necessário
    int pwm_esq = pidEsq.controle(abs(vel_ref_esq), rpm_esq);
    int pwm_dir = pidDir.controle(abs(vel_ref_dir), rpm_dir);

    // CORREÇÃO 4: Garante corte elétrico imediato na ponte H se o alvo for zero
    if (vel_ref_esq == 0.0) pwm_esq = 0;
    if (vel_ref_dir == 0.0) pwm_dir = 0;

    // 4. Aplica os sinais calculados na ponte H
    motorEsq.acionaMotor(pwm_esq);
    motorDir.acionaMotor(pwm_dir);

    vTaskDelay((1000/FREQ_MOTORES_HZ) / portTICK_PERIOD_MS);
  }
}

// Network, display and actuator work never run in the motor/PID task. This
// keeps MQTT connection latency and SPI drawing away from the 50 Hz controller.
void codigoTaskInterface(void * parameter) {
  lockService.begin();

  for (;;) {
    lockService.tick();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==========================================
// SETUP PRINCIPAL (O MAESTRO)
// ==========================================
void setup() {
  Serial.begin(921600);
  delay(2000); 
  Serial.println("=== Iniciando Sistema (FreeRTOS) ===");

  // 1. Inicializa a Fila de Comunicação
  // Criamos espaço para guardar até 5 valores do tipo 'float'
  filaVelocidadeEsq = xQueueCreate(5, sizeof(float));
  filaVelocidadeDir = xQueueCreate(5, sizeof(float));

  // 2. Inicializa o Webserver assíncrono (em modo Access Point)
  webserver_init(WIFI_SSID, WIFI_PASS);

  xTaskCreatePinnedToCore(
    codigoTaskMotores, 
    "TaskMotores", 
    10000, 
    NULL, 
    1,
    &TaskMotores, 
    1);                  /* Fixado no CORE 1 */

  xTaskCreatePinnedToCore(
    codigoTaskInterface,
    "TaskInterface",
    12288,
    NULL,
    1,
    &TaskInterface,
    0);                  /* Wi-Fi, MQTT e display no CORE 0 */

  Serial.println("Tarefas criadas. O FreeRTOS assumiu o controle.");
}

void loop() {
  // Deleta a tarefa padrão do loop para liberar recursos de memória RAM
  vTaskDelete(NULL);
}
