#pragma once

// Inicializa a porta Serial2 (UART) para se comunicar com o Ublox Neo-6M
bool gps_init();

// Lê continuamente os dados da porta serial e decodifica as coordenadas
void gps_update();

// Imprime Latitude, Longitude, Altitude, Satélites e Status do Fix em formato CSV
void gps_print_ros_format();