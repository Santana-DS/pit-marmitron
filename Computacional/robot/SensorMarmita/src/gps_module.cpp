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

portMUX_TYPE gpsMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t timestamp_gps;
uint32_t gps_seq;

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
  portENTER_CRITICAL(&gpsMux);
  timestamp_gps = micros();
  portEXIT_CRITICAL(&gpsMux);

  // Se a biblioteca conseguiu extrair uma coordenada válida, atualizamos os dados
  if (gps.location.isValid()) {
    portENTER_CRITICAL(&gpsMux);
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    has_fix = true;
    gps_seq += 1;
    portEXIT_CRITICAL(&gpsMux);
  } else {
    // Se o robô estiver dentro de casa, o GPS não terá "Fix" (sinal com os satélites)
    portENTER_CRITICAL(&gpsMux);
    has_fix = false;
    portEXIT_CRITICAL(&gpsMux);
  }

  if (gps.altitude.isValid()) {
    portENTER_CRITICAL(&gpsMux);
    altitude = gps.altitude.meters();
    portEXIT_CRITICAL(&gpsMux);
  }

  if (gps.satellites.isValid()) {
    portENTER_CRITICAL(&gpsMux);
    num_satellites = gps.satellites.value();
    portEXIT_CRITICAL(&gpsMux);
  }
}

void gps_print_ros_format() {
  // Formato da linha: Latitude,Longitude,Altitude,Satélites,StatusFix(0 ou 1)
  uint32_t timestamp;
  double lat, longi, alt;
  int num_sat, fix;
  uint32_t seq;

  portENTER_CRITICAL(&gpsMux);
  lat = latitude;
  longi = longitude;
  alt = altitude;
  num_sat = num_satellites;
  fix = has_fix;
  timestamp = timestamp_gps;
  seq = gps_seq;
  portEXIT_CRITICAL(&gpsMux);

  Serial.print(seq); Serial.print(",");
  Serial.print(timestamp); Serial.print(",");
  Serial.print(lat, 6); Serial.print(",");
  Serial.print(longi, 6); Serial.print(",");
  Serial.print(alt, 2); Serial.print(",");
  Serial.print(num_sat); Serial.print(",");
  // Imprime "1" se tem sinal de satélite, "0" se está sem sinal
  Serial.print(fix ? "1" : "0"); //Serial.print(",");
}