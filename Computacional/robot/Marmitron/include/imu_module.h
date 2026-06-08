#pragma once

// Inicializa o barramento SPI, o sensor MPU9250 e faz a calibração.
// Retorna 'true' se tudo der certo, 'false' se houver falha de hardware.
bool imu_init();

// Lê os dados brutos dos sensores e faz a conversão matemática para os padrões ROS (m/s², rad/s, Teslas)
void imu_update();

// Imprime os últimos dados lidos na porta Serial em formato CSV
void imu_print_ros_format();