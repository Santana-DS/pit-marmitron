#include "webserver_module.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Importa a nossa caixa de correio criada lá no main.cpp!
extern QueueHandle_t filaVelocidade;

// Cria o objeto do servidor na porta padrão HTTP (80)
static AsyncWebServer server(80);

// ==========================================
// INTERFACE WEB (HTML + CSS + JavaScript)
// ==========================================
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Controle do Robô</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; text-align: center; background-color: #1e1e1e; color: #ffffff; padding-top: 50px;}
    h1 { color: #00d2ff; }
    .slider { -webkit-appearance: none; width: 80%; height: 25px; border-radius: 12px; background: #333; outline: none; margin-top: 20px;}
    .slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 35px; height: 35px; border-radius: 50%; background: #00d2ff; cursor: pointer; }
    .valor { font-size: 2em; font-weight: bold; margin-top: 20px; color: #ffeb3b;}
  </style>
</head>
<body>
  <h1>Controle de Velocidade</h1>
  <p>Deslize para alterar a velocidade dos motores</p>
  
  <input type="range" min="-5.0" max="5.0" step="0.1" value="0.0" class="slider" id="sliderVel" oninput="enviarVelocidade(this.value)">
  
  <div class="valor">V: <span id="mostrador">0.0</span></div>

  <script>
    // Quando o usuário arrasta o dedo, essa função avisa o ESP32 em tempo real
    function enviarVelocidade(val) {
      document.getElementById('mostrador').innerText = val;
      // Faz uma requisição GET invisível para o robô (Ex: /set_speed?v=2.5)
      fetch('/set_speed?v=' + val);
    }
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

void webserver_init(const char* ssid, const char* password) {
  // 1. Conecta no Wi-Fi
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi conectado!");
  Serial.print("Abra o navegador no celular e digite este IP: ");
  Serial.println(WiFi.localIP());

  // 2. Rota Principal ("/"): O que acontece quando o usuário acessa o IP
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", html_page);
  });

  // 3. Rota de Controle ("/set_speed"): Onde o JavaScript bate para enviar dados
  server.on("/set_speed", HTTP_GET, [](AsyncWebServerRequest *request){
    
    // Verifica se a mensagem veio com o parâmetro "v" (velocidade)
    if(request->hasParam("v")) {
      String valor_string = request->getParam("v")->value();
      float nova_velocidade = valor_string.toFloat();

      // MÁGICA DO FREERTOS AQUI:
      // Se a fila existe, "joga" o número float lá dentro para a Task de Motores pegar!
      // O "(TickType_t)0" significa: Não perca tempo esperando, jogue na fila e fuja.
      if(filaVelocidade != NULL) {
        xQueueSend(filaVelocidade, &nova_velocidade, (TickType_t)0);
      }
    }
    // Responde ao navegador que deu tudo certo para ele não travar
    request->send(200, "text/plain", "OK"); 
  });

  // 4. Liga o servidor de vez
  server.begin();
  Serial.println("Webserver online rodando");
}