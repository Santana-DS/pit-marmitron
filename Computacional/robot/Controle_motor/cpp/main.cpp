#include <fstream>
#include <iostream>

#include "pid.h"
#include "motor.h"
#include "encoder_sim.h"
#include "cinematica.h"
#include <cstdlib>

#define DT 0.01

int main() {

    PID pid_left;

    pid_init(&pid_left, 20, 0, 0, -255, 255);

    Encoder enc_left = {0, 0};

    float v_real = 0;
    float tempo = 0;

    std::ofstream logFile("../data/log.csv");

    logFile << "tempo,referencia,velocidade,pwm,erro\n";

    while (tempo < 10.0) {

        float v_ref;

        if (tempo < 2.0)
            v_ref = 0;
        else
            v_ref = 1.0;

        encoder_update(&enc_left, v_real, DT);

        float v_meas = encoder_get_velocity(&enc_left, DT);

        // Ruído do encoder
        float ruido = ((rand() % 100) / 1000.0f) - 0.05f;

        v_meas += ruido;

        static float v_filtrado = 0.0f;

        v_filtrado = 0.9f * v_filtrado + 0.1f * v_meas;

        v_meas = v_filtrado;

        float erro = v_ref - v_meas;

        float pwm = pid_update(&pid_left, erro, DT);

        v_real = motor_update(pwm, v_real, DT);

        logFile << tempo << ","
                << v_ref << ","
                << v_meas << ","
                << pwm << ","
                << erro << "\n";

        tempo += DT;
    }

    logFile.close();

    std::cout << "Simulação concluída.\n";

    return 0;
}