#pragma once

// ==========================================
// REDE WI-FI
// ==========================================
const char* const WIFI_SSID = "Marmitron";
const char* const WIFI_PASS = "12345678";

// ==========================================
// FREQUÊNCIA DE ATUALIZAÇÃO
// ==========================================
constexpr int FREQ_MOTORES_HZ = 50; 

// ==========================================
// SINTONIA DO PID (RODAS)
// ==========================================
constexpr float KPesq = 1.1; //já usei 0.3
constexpr float KIesq = 0.3;
constexpr float KDesq = 0.0;
constexpr float KPdir = 1.1; //já usei 0.3
constexpr float KIdir = 0.3;
constexpr float KDdir = 0.0;