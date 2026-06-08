#ifndef ENCODER_SIM_H
#define ENCODER_SIM_H

typedef struct {
    float posicao;
    int pulsos;
} Encoder;

void encoder_update(Encoder *enc, float velocidade, float dt);
float encoder_get_velocity(Encoder *enc, float dt);

#endif