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

portMUX_TYPE muxImu = portMUX_INITIALIZER_UNLOCKED;
uint32_t accgyr_timestamp_us;
uint32_t accgyr_seq;

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

  //Serial.println("Calibrando IMU. NAO MOVA O ROBO...");
  //delay(1000); // Dá um tempo para as vibrações mecânicas pararem
  //myMPU9250.autoOffsets();
  //Serial.println("IMU calibrada com sucesso!");

  // Configurações e Filtros para reduzir o ruído dos motores no ROS
  myMPU9250.enableGyrDLPF();
  myMPU9250.setGyrDLPF(MPU9250_DLPF_3);
  myMPU9250.setGyrRange(MPU9250_GYRO_RANGE_2000);

  myMPU9250.setAccRange(MPU9250_ACC_RANGE_8G);
  myMPU9250.enableAccDLPF(true);
  myMPU9250.setAccDLPF(MPU9250_DLPF_3);

  myMPU9250.setSampleRateDivider(1);

  return true;
}

void accgyr_update() {
  uint32_t t = micros();
  xyzFloat gValue = myMPU9250.getGValues();
  xyzFloat gyrValue = myMPU9250.getGyrValues();
  float acx_offset = 0.385;
  float acy_offset = 0.32;
  float acz_offset = -1.065;
  float acx_escala = 1;
  float acy_escala = 1;
  float acz_escala = 1;
  float gyrx_offset = -0.04;
  float gyry_offset = 0.02;
  float gyrz_offset = 0.02;
  // 2. Converte para m/s²
  if (gValue.x == 0 && gValue.y == 0 && gValue.z == 0 && gyrValue.x == 0 && gyrValue.y == 0 && gyrValue.z == 0) {
    return;
  }
  portENTER_CRITICAL(&muxImu);
  accgyr_timestamp_us = t;
  accgyr_seq += 1;

  acc_x_ms2 = (gValue.x * 9.80665)*acx_escala - acx_offset;
  acc_y_ms2 = (gValue.y * 9.80665)*acy_escala - acy_offset;
  acc_z_ms2 = (gValue.z * 9.80665)*acz_escala - acz_offset;

  // 3. Converte para rad/s (DEG_TO_RAD é nativo do Arduino)
  gyr_x_rads = (gyrValue.x * DEG_TO_RAD) - gyrx_offset;
  gyr_y_rads = (gyrValue.y * DEG_TO_RAD) - gyry_offset;
  gyr_z_rads = (gyrValue.z * DEG_TO_RAD) - gyrz_offset;
  portEXIT_CRITICAL(&muxImu);
}

void mag_update() {
  // 1. Leituras brutas
  xyzFloat magValue = myMPU9250.getMagValues(); 

  portENTER_CRITICAL(&muxImu);
  mag_x_T = magValue.x / 1000000.0;
  mag_y_T = magValue.y / 1000000.0;
  mag_z_T = magValue.z / 1000000.0;
  portEXIT_CRITICAL(&muxImu);
}

void accgyr_print_ros_format() {
  xyzFloat acc, gyr;
  uint32_t timestamp;
  uint32_t seq;

  portENTER_CRITICAL(&muxImu);
  acc.x = acc_x_ms2; acc.y = acc_y_ms2; acc.z = acc_z_ms2;
  gyr.x = gyr_x_rads; gyr.y = gyr_y_rads; gyr.z = gyr_z_rads;
  timestamp = accgyr_timestamp_us; seq = accgyr_seq;
  portEXIT_CRITICAL(&muxImu);

  Serial.print(seq); Serial.print(",");
  Serial.print(timestamp); Serial.print(",");
  // Acelerômetro
  Serial.print(acc.x, 4); Serial.print(",");
  Serial.print(acc.y, 4); Serial.print(",");
  Serial.print(acc.z, 4); Serial.print(",");

  // Giroscópio
  Serial.print(gyr.x, 4); Serial.print(",");
  Serial.print(gyr.y, 4); Serial.print(",");
  Serial.print(gyr.z, 4); Serial.print(",");
}

void mag_print_ros_format() {
  xyzFloat mag;
  portENTER_CRITICAL(&muxImu);
  mag.x = mag_x_T; mag.y = mag_y_T; mag.z = mag_z_T;
  portEXIT_CRITICAL(&muxImu);

  // Magnetômetro
  Serial.print(mag.x, 8); Serial.print(",");
  Serial.print(mag.y, 8); Serial.print(",");
  Serial.print(mag.z, 8); Serial.print(",");
}

// void imu_update() {
//   // 1. Leituras brutas
//   xyzFloat gValue = myMPU9250.getGValues();
//   xyzFloat gyrValue = myMPU9250.getGyrValues();
//   xyzFloat magValue = myMPU9250.getMagValues(); 

//   // 2. Converte para m/s²
//   acc_x_ms2 = gValue.x * 9.80665;
//   acc_y_ms2 = gValue.y * 9.80665;
//   acc_z_ms2 = gValue.z * 9.80665;

//   // 3. Converte para rad/s (DEG_TO_RAD é nativo do Arduino)
//   gyr_x_rads = gyrValue.x * DEG_TO_RAD;
//   gyr_y_rads = gyrValue.y * DEG_TO_RAD;
//   gyr_z_rads = gyrValue.z * DEG_TO_RAD;

//   // 4. Converte microTeslas para Teslas
//   mag_x_T = magValue.x / 1000000.0;
//   mag_y_T = magValue.y / 1000000.0;
//   mag_z_T = magValue.z / 1000000.0;
// }

// void imu_print_ros_format() {
//   // Acelerômetro
//   Serial.print(acc_x_ms2, 4); Serial.print(",");
//   Serial.print(acc_y_ms2, 4); Serial.print(",");
//   Serial.print(acc_z_ms2, 4); Serial.print(",");

//   // Giroscópio
//   Serial.print(gyr_x_rads, 4); Serial.print(",");
//   Serial.print(gyr_y_rads, 4); Serial.print(",");
//   Serial.print(gyr_z_rads, 4); Serial.print(",");

//   // Magnetômetro
//   Serial.print(mag_x_T, 8); Serial.print(",");
//   Serial.print(mag_y_T, 8); Serial.print(",");
//   Serial.print(mag_z_T, 8); Serial.print(",");
// }