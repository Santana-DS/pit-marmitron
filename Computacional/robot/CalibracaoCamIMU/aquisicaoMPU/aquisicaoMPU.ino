#include <Wire.h>
#include <MPU9250.h>

MPU9250 mpu;

// Configuração de amostragem (200 Hz = período de 5000 microssegundos)
const unsigned long SAMPLE_PERIOD_US = 5000; 
unsigned long lastSampleTime = 0;

void setup() {
    Serial.begin(921600); // Baud rate alto para evitar gargalo na transmissão
    Wire.begin(21, 22);   // Pinos I2C padrão da ESP32: SDA=21, SCL=22
    delay(2000);

    if (!mpu.setup(0x68)) {  // Endereço I2C padrão
        while (1) {
            Serial.println("Erro: MPU9250 nao encontrada!");
            delay(5000);
        }
    }

    // Configurações para reduzir ruído interno do chip (Low Pass Filter)
    mpu.setFilterBandwidth(MPU9250::DLPF_BW_41); // Filtro interno ~41Hz
    mpu.setGyroRange(MPU9250::GYRO_RANGE_2000DPS);
    mpu.setAccelRange(MPU9250::ACCEL_RANGE_8G);
}

void loop() {
    unsigned long currentTime = micros();

    // Garante a amostragem rigorosa a 200Hz
    if (currentTime - lastSampleTime >= SAMPLE_PERIOD_US) {
        lastSampleTime = currentTime;

        if (mpu.update()) {
            // Captura o tempo atual em milissegundos desde o boot da ESP32
            unsigned long timestamp_ms = millis();

            // Envia no formato CSV: timestamp,ax,ay,az,gx,gy,gz
            // Aceleração em m/s² e Giroscópio em rad/s (padrão ROS/ORB-SLAM3)
            Serial.print(timestamp_ms); Serial.print(",");
            Serial.print(mpu.getAccX() * 9.80665); Serial.print(",");
            Serial.print(mpu.getAccY() * 9.80665); Serial.print(",");
            Serial.print(mpu.getAccZ() * 9.80665); Serial.print(",");
            Serial.print(mpu.getGyroX() * M_PI / 180.0); Serial.print(",");
            Serial.print(mpu.getGyroY() * M_PI / 180.0); Serial.print(",");
            Serial.println(mpu.getGyroZ() * M_PI / 180.0);
        }
    }
}
