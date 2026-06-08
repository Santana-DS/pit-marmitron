#include "encoder_sim.h"
#include "config.h"

void encoder_update(Encoder *enc, float velocidade, float dt) {
    enc->posicao += velocidade * dt;

    enc->pulsos = (int)(enc->posicao * PPR);
}

float encoder_get_velocity(Encoder *enc, float dt) {
    return (float)enc->pulsos / (PPR * dt);
}