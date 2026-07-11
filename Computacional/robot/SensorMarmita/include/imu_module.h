#pragma once

// Inicializa o barramento SPI, o sensor MPU9250 e faz a calibração.
// Retorna 'true' se tudo der certo, 'false' se houver falha de hardware.
bool imu_init();

// Lê os dados brutos dos sensores e faz a conversão matemática para os padrões ROS (m/s², rad/s, Teslas)
void accgyr_update();
void mag_update();

// Imprime os últimos dados lidos na porta Serial em formato CSV
void accgyr_print_ros_format();
void mag_print_ros_format();