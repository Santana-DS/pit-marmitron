#include "webserver_module.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Importa a nossa caixa de correio criada lá no main.cpp!
extern QueueHandle_t filaVelocidadeEsq;
extern QueueHandle_t filaVelocidadeDir;

// Cria o objeto do servidor na porta padrão HTTP (80)
static AsyncWebServer server(80);

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Controle do Robô</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; text-align: center; background-color: #222; color: white; margin: 0; padding: 20px; }
    h2 { margin-bottom: 30px; color: #f1c40f; }
    
    /* Grid e estilo dos botões de controle de direção */
    .control-container { display: grid; grid-template-columns: repeat(3, 100px); grid-gap: 15px; justify-content: center; margin: 0 auto; }
    .btn { width: 100px; height: 100px; font-size: 18px; border: none; border-radius: 50%; color: white; cursor: pointer; font-weight: bold; transition: 0.2s; }
    .btn-dir { background-color: #3498db; }
    .btn-dir:active { background-color: #2980b9; }
    .btn-stop { background-color: #e74c3c; width: 100%; grid-column: span 3; border-radius: 10px; height: 60px; margin-top: 15px; }
    .btn-stop:active { background-color: #c0392b; }
    .empty { width: 100px; height: 100px; }
  </style>
</head>
<body>

  <h2>Controle Remoto do Carrinho</h2>
  <p>Toque nos botões para direcionar o robô</p>
  
  <div class='control-container'>
    <div class='empty'></div>
    <button class='btn btn-dir' onclick="enviarComando('1.0')">▲<br>Frente</button>
    <div class='empty'></div>
    
    <button class='btn btn-dir' onclick="enviarComando('2.0')">◀<br>Esq</button>
    <div class='empty'></div>
    <button class='btn btn-dir' onclick="enviarComando('3.0')">▶<br>Dir</button>
    
    <div class='empty'></div>
    <button class='btn btn-dir' onclick="enviarComando('-1.0')">▼<br>Trás</button>
    <div class='empty'></div>
    
    <button class='btn btn-stop' onclick="enviarComando('0.0')">🛑 PARAR</button>
  </div>

  <script>
    // Envia o comando via requisição HTTP assíncrona (Background)
    function enviarComando(valor) {
      fetch('/set_speed?v=' + valor);
    }
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

void webserver_init(const char* ssid, const char* password) {
  // 1. Configura a ESP32 como Access Point (Rede própria), igual ao código 1
  Serial.print("Criando rede Wi-Fi: ");
  Serial.println(ssid);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Abra o navegador no celular e digite este IP: ");
  Serial.println(IP);

  // 2. Rota Principal ("/"): Serve a interface com os botões
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", html_page);
  });

  // 3. Rota de Controle ("/set_speed"): Captura o código da direção e joga na Fila
  server.on("/set_speed", HTTP_GET, [](AsyncWebServerRequest *request){
    
    if(request->hasParam("v")) {
      String valor_string = request->getParam("v")->value();
      float comando_direcao = valor_string.toFloat();

      // Envia o float para a Task de Motores processar de forma assíncrona
      if(filaVelocidadeEsq != NULL) {
        xQueueSend(filaVelocidadeEsq, &comando_direcao, (TickType_t)0);
      }
    }
    request->send(200, "text/plain", "OK"); 
  });

  // 4. Inicia o servidor
  server.begin();
  Serial.println("Webserver assíncrono online!");
}