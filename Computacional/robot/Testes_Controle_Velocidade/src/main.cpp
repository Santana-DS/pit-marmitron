#include <Arduino.h>
#include "OTA_updater.h"
#include "controle_de_velocidade.h"

#define ENCODER_1 27
#define MOTOR1_R_PWM 32
#define MOTOR1_L_PWM 33
#define MOTOR1_R_EN 25
#define MOTOR1_L_EN 26

#define ENCODER_2 23
#define MOTOR2_R_PWM 22
#define MOTOR2_L_PWM 1
#define MOTOR2_R_EN 3
#define MOTOR2_L_EN 21

QueueHandle_t VelRefQueue = NULL;

void OTATask(void *parameter) {
  OTA_setup();
  for (;;){
  Serial.println("OTATask rodando...");
  float vel_ref = 100;
  xQueueSend(VelRefQueue, &vel_ref, portMAX_DELAY);
  OTA_loop();
  }
}

void ControleTask(void *parameter) {  
  // Setup motores e controle de velocidade
  motor motor1(MOTOR1_R_EN, MOTOR1_L_EN, MOTOR1_R_PWM, MOTOR1_L_PWM);
  motor1.setup();
  motor* p_motor1 = &motor1;
  motor motor2(MOTOR2_R_EN, MOTOR2_L_EN, MOTOR2_R_PWM, MOTOR2_L_PWM);
  motor2.setup();
  motor* p_motor2 = &motor2;
  ControleDeVelocidade_setup(p_motor1, ENCODER_R);
  ControleDeVelocidade_setup(p_motor2, ENCODER_L);

  Serial.println("Setup controle rodou");
  float vel_ref_buf = 100;
  float vel_ref = 100;
  for (;;) { //Loop
  //if (xQueueReceive(VelRefQueue, &vel_ref_buf, portMAX_DELAY)){
    vel_ref = vel_ref_buf;
  //};
  ControleDeVelocidade_loop(p_motor1, vel_ref, ENCODER_R);
  ControleDeVelocidade_loop(p_motor2, vel_ref, ENCODER_L);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup rodando...");

  VelRefQueue = xQueueCreate(5, sizeof(float));

  TaskHandle_t OTATaskHandle = NULL;
  xTaskCreatePinnedToCore(
    OTATask,
    "OTATask",
    10000,
    NULL,
    2,
    &OTATaskHandle,
    0
  );

  TaskHandle_t ControleTaskHandle = NULL;
  xTaskCreate(
    ControleTask,
    "ControleTask",
    10000,
    NULL,
    1,
    &ControleTaskHandle
  );
};

void loop() {

}