#pragma once

// ==========================================
// REDE WI-FI
// ==========================================
// Usamos 'const char*' para textos
const char* const WIFI_SSID = "NOME_DA_SUA_REDE";
const char* const WIFI_PASS = "SUA_SENHA_AQUI";

// ==========================================
// SINTONIA DO PID (RODAS)
// ==========================================
// Usamos 'constexpr float' para números, pois é mais seguro e rápido que o #define
// constexpr float PID_KP = 2.0;
// constexpr float PID_KI = 0.5;
// constexpr float PID_KD = 0.1;

// ==========================================
// PARÂMETROS FÍSICOS DO ROBÔ (Exemplos úteis)
// ==========================================
// constexpr float RAIO_DA_RODA_METROS = 0.033; 
// constexpr float DISTANCIA_ENTRE_RODAS = 0.15; // Para a odometria
// constexpr int FUROS_DO_ENCODER = 40;