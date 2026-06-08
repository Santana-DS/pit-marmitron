#include "gps_module.h"
#include "pinout.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>

// ==========================================
// VARIÁVEIS INTERNAS DO MÓDULO
// ==========================================
static TinyGPSPlus gps;

// Instancia a porta Serial2 nativa do hardware do ESP32
static HardwareSerial gpsSerial(2);

// Variáveis para guardar os dados decodificados
static double latitude = 0.0;
static double longitude = 0.0;
static double altitude = 0.0;
static uint32_t num_satellites = 0;
static bool has_fix = false;

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

bool gps_init() {
  // Inicializa a UART2. O Baud rate padrão de fábrica do Neo-6M é 9600 bps.
  // Parâmetros: Baud Rate, Protocolo Padrão, Pino RX, Pino TX
  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  
  Serial.println("GPS UART2 inicializada com sucesso.");
  return true; // A UART sempre inicia. O sinal do satélite avaliaremos no update().
}

void gps_update() {
  // Enquanto o GPS estiver mandando letras pela porta serial...
  while (gpsSerial.available() > 0) {
    // Pegamos a letra e entregamos para a biblioteca montar a frase
    gps.encode(gpsSerial.read());
  }

  // Se a biblioteca conseguiu extrair uma coordenada válida, atualizamos os dados
  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    has_fix = true;
  } else {
    // Se o robô estiver dentro de casa, o GPS não terá "Fix" (sinal com os satélites)
    has_fix = false;
  }

  if (gps.altitude.isValid()) {
    altitude = gps.altitude.meters();
  }

  if (gps.satellites.isValid()) {
    num_satellites = gps.satellites.value();
  }
}

void gps_print_ros_format() {
  // Formato da linha: Latitude,Longitude,Altitude,Satélites,StatusFix(0 ou 1)
  
  // Usamos 6 casas decimais para Lat/Lon, o que garante precisão de centímetros!
  Serial.print(latitude, 6); Serial.print(",");
  Serial.print(longitude, 6); Serial.print(",");
  Serial.print(altitude, 2); Serial.print(",");
  Serial.print(num_satellites); Serial.print(",");
  
  // Imprime "1" se tem sinal de satélite, "0" se está sem sinal
  Serial.print(has_fix ? "1" : "0"); Serial.print(",");
}