#pragma once
#include <Arduino.h>

class EncoderISR {
  private:
    uint8_t pinoA;
    float alpha;

    // Variáveis voláteis (modificadas pela interrupção)
    volatile unsigned long t_encoder;
    volatile unsigned long t_anterior_encoder;
    volatile unsigned long periodo_quarta_volta;
    volatile int conta_encoder;

    // Variáveis de memória do objeto
    float vel_filtrada;
    portMUX_TYPE mux; // Fechadura exclusiva para proteger os dados desta roda

    // 1. A função que faz a matemática real (Privada)
    void IRAM_ATTR rotinaInterrupcao();

    // 2. A "Ponte" estática que o hardware do ESP32 exige
    static void IRAM_ATTR isrWrapper(void* arg);

  public:
    // Construtor
    EncoderISR(uint8_t pinA, float alpha_filtro = 0.7);

    // Liga o pino e a interrupção
    void init();

    // Retorna o RPM já calculado, filtrado e com proteção contra travamento (Timeout)
    float lerVelocidadeRPM();
};