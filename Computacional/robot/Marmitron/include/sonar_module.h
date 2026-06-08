#pragma once

// Configura os pinos de Trigger e Echo
bool sonar_init();

// Dispara o pulso, aguarda o retorno com timeout de segurança e calcula a distância em metros
void sonar_update();

// Imprime a distância lida. Retorna -1.0 se estiver fora de alcance.
void sonar_print_ros_format();