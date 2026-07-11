#include "imu_module.h"
#include "pinout.h"
#include <Arduino.h>
#include <SPI.h>
#include <MPU9250_WE.h>

// ==========================================
// VARIÁVEIS INTERNAS DO MÓDULO (Escondidas)
// ==========================================
static const bool useSPI = true;
// Passamos o pino CS importado lá do pinout.h
static MPU9250_WE myMPU9250 = MPU9250_WE(&SPI, PIN_SPI_CS, useSPI);

// Variáveis para guardar os dados já convertidos (Padrão ROS)
static float acc_x_ms2, acc_y_ms2, acc_z_ms2;
static float gyr_x_rads, gyr_y_rads, gyr_z_rads;
static float mag_x_T, mag_y_T, mag_z_T;

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

bool imu_init() {
  // Inicializa o barramento SPI com os pinos exatos do nosso robô
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
  
  if (!myMPU9250.init()) {
    Serial.println("ERRO: MPU9250 nao responde no SPI.");
    return false;
  }

  if (!myMPU9250.initMagnetometer()) {
    Serial.println("ERRO: Magnetometro AK8963 nao responde.");
    return false;
  }

  Serial.println("Calibrando IMU. NAO MOVA O ROBO...");
  delay(1000); // Dá um tempo para as vibrações mecânicas pararem
  myMPU9250.autoOffsets();
  Serial.println("IMU calibrada com sucesso!");

  // Configurações e Filtros para reduzir o ruído dos motores no ROS
  myMPU9250.enableGyrDLPF();
  myMPU9250.setGyrDLPF(MPU9250_DLPF_6);
  myMPU9250.setGyrRange(MPU9250_GYRO_RANGE_250);

  myMPU9250.setAccRange(MPU9250_ACC_RANGE_2G);
  myMPU9250.enableAccDLPF(true);
  myMPU9250.setAccDLPF(MPU9250_DLPF_6);

  return true;
}

void imu_update() {
  // 1. Leituras brutas
  xyzFloat gValue = myMPU9250.getGValues();
  xyzFloat gyrValue = myMPU9250.getGyrValues();
  //xyzFloat magValue = myMPU9250.getMagValues(); 

  // 2. Converte para m/s²
  acc_x_ms2 = gValue.x * 9.80665;
  acc_y_ms2 = gValue.y * 9.80665;
  acc_z_ms2 = gValue.z * 9.80665;

  // 3. Converte para rad/s (DEG_TO_RAD é nativo do Arduino)
  gyr_x_rads = gyrValue.x * DEG_TO_RAD;
  gyr_y_rads = gyrValue.y * DEG_TO_RAD;
  gyr_z_rads = gyrValue.z * DEG_TO_RAD;

  // 4. Converte microTeslas para Teslas
  //mag_x_T = magValue.x / 1000000.0;
  //mag_y_T = magValue.y / 1000000.0;
  //mag_z_T = magValue.z / 1000000.0;
}

void imu_print_ros_format() {
  // Acelerômetro
  //Serial.print(acc_x_ms2, 4); Serial.print(",");
  //Serial.print(acc_y_ms2, 4); Serial.print(",");
  //Serial.print(acc_z_ms2, 4); Serial.print(",");

  // Giroscópio
  //Serial.print(gyr_x_rads, 4); Serial.print(",");
  //Serial.print(gyr_y_rads, 4); Serial.print(",");
  //Serial.print(gyr_z_rads, 4); Serial.print(",");

  // Magnetômetro
  //Serial.print(0.0, 8); Serial.print(",");
  //Serial.print(0.0, 8); Serial.print(",");
  //Serial.println(0.0, 8); //Serial.print(",");
}