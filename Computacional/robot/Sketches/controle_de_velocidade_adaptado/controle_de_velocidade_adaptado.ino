//Copiado de esp32_controle_remoto//

#include <WiFi.h>

// --- Configurações do Wi-Fi ---
const char* ssid     = "NOME_DA_SUA_REDE";
const char* password = "SENHA_DO_WIFI";

// Inicializa o servidor na porta 80
WiFiServer server(80);

// --- Configurações dos Pinos dos Motores ---
// Motor A
/*const int motorA_pin1 = 12;
const int motorA_pin2 = 13; 
const int enaPin      = 14; */// Pino de velocidade (PWM)

// Motor B
/*const int motorB_pin1 = 26;
const int motorB_pin2 = 27;
const int enbPin      = 25; */ // Pino de velocidade (PWM)

// Propriedades do PWM
const int freq = 30000;
const int pwmChannelA = 0;
const int pwmChannelB = 1;
const int resolution = 8; // Resolução de 8 bits (0 a 255)

// Variável para armazenar a velocidade atual (0 a 255)
int velocidade = 150; 

//Copiado de esp32_controle_remoto//


#define ENCODER_D_IN TODO //13
#define ENCODER_A_IN TODO //A0
#define MOTOR1_PWM_R 12 //A1
#define MOTOR1_PWM_L 13 //A2

void setup() {
  // put your setup code here, to run once:

//Copiado esp32_controle_remoto
  //Serial.begin(115200);

  /*// Configura os pinos como saída
  pinMode(motorA_pin1, OUTPUT);
  pinMode(motorA_pin2, OUTPUT);
  pinMode(motorB_pin1, OUTPUT);
  pinMode(motorB_pin2, OUTPUT); */

  // Configura o PWM no ESP32
  ledcSetup(pwmChannelA, freq, resolution);
  ledcSetup(pwmChannelB, freq, resolution);
  
  // Associa os pinos de Enable aos canais PWM
  /*ledcAttachPin(enaPin, pwmChannelA);
  ledcAttachPin(enbPin, pwmChannelB); */

  // Conecta ao Wi-Fi
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Mostra o IP gerado no Monitor Serial
  Serial.println("");
  Serial.println("Wi-Fi conectado!");
  Serial.print("Endereço IP do ESP32: http://");
  Serial.println(WiFi.localIP());
  
  server.begin();
//Copiado esp32_controle_remoto fim

  Serial.begin(115200);
  pinMode(ENCODER_D_IN, INPUT);
  pinMode(ENCODER_A_IN, INPUT);
  pinMode(MOTOR1_PWM_R, OUTPUT);
  pinMode(MOTOR1_PWM_L, OUTPUT);
}

// Variáveis leitura dos encoders
int count = 0;
unsigned long t0 = millis();
unsigned long intervalo = 50;
float intervalof = (float) intervalo;
bool leitura_anterior = digitalRead(ENCODER_D_IN);
bool leitura_d;
int leitura_a;
float vel_angular = 0;
//

// Variáveis controle PID
unsigned long t_anterior = micros();
unsigned long t_controle;
float deltat;
float Kp = 10;
float Ki = 10;
float Kd = 1;
float integral = 0;
float derivada = 0;
float erro = 0;
float vel_ref = 0;
float erro_anterior = 0;
float deltae = 0;
float controle = 0;
int pwm;
//
void loop() {

  //Copiado de esp32_controle_remoto
  WiFiClient client = server.available(); // Verifica se há clientes conectados

  if (client) {
    Serial.println("Novo cliente conectado.");
    String currentLine = "";                // String para armazenar os dados vindos do cliente
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        
        if (c == '\n') {
          // Se a linha atual estiver vazia, significa o fim da requisição HTTP do cliente
          if (currentLine.length() == 0) {
            // Envia o cabeçalho HTTP padrão
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Página Web HTML + JavaScript para os comandos
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>body{text-align:center; font-family:Arial;} .btn{padding:15px 25px; font-size:18px; margin:10px; text-decoration:none; color:white; background:#2196F3; border-radius:5px;} .stop{background:#f44336;}</style></head>");
            client.println("<body><h1>Controle ESP32</h1>");
            
            // Exibe a velocidade atual
            client.print("<p>Velocidade Atual: <strong>");
            client.print(velocidade);
            client.println("</strong> / 255</p>");
            
            // Botões de comando
            client.println("<p><a href=\"/F\" class=\"btn\">Frente</a></p>");
            client.println("<p><a href=\"/E\" class=\"btn\">Esquerda</a> <a href=\"/S\" class=\"btn stop\">PARAR</a> <a href=\"/D\" class=\"btn\">Direita</a></p>");
            client.println("<p><a href=\"/T\" class=\"btn\">Trás</a></p>");
            
            // Controle de velocidade (Mudar para valores fixos ou usar sliders)
            client.println("<br><h3>Ajustar Velocidade:</h3>");
            client.println("<a href=\"/V100\" class=\"btn\" style=\"background:#4CAF50;\">Min (100)</a>");
            client.println("<a href=\"/V180\" class=\"btn\" style=\"background:#4CAF50;\">Média (180)</a>");
            client.println("<a href=\"/V255\" class=\"btn\" style=\"background:#4CAF50;\">Máxima (255)</a>");
            
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c; // Adiciona o caractere à linha atual
        }

        // --- Processamento dos Comandos de Movimento ---
        if (currentLine.endsWith("GET /F")) moverFrente();
        if (currentLine.endsWith("GET /T")) moverTras();
        if (currentLine.endsWith("GET /E")) virarEsquerda();
        if (currentLine.endsWith("GET /D")) virarDireita();
        if (currentLine.endsWith("GET /S")) pararMotores();

        // --- Processamento dos Comandos de Velocidade ---
        if (currentLine.endsWith("GET /V100")) { velocidade = 100; vel_ref = 7.0;; }
        if (currentLine.endsWith("GET /V180")) { velocidade = 180; vel_ref = 15.0; }
        if (currentLine.endsWith("GET /V255")) { velocidade = 255; vel_ref = 31.0; }
      }
    }
    client.stop();
    Serial.println("Cliente desconectado.");
  }
  //Copiado de esp32_controle_remoto fim
  
  /* ===== Leitura de velocidade ====================================================================================
  =================================================================================================================== */

  // Quando o sensor é coberto, leitura digital é 1. Quando está livre, leitura digital é 0. Leitura analógica parece crescer com a cobertura do sensor.
  unsigned long t = millis();
  leitura_d = digitalRead(ENCODER_D_IN);
  leitura_a = analogRead(ENCODER_A_IN);
  if (leitura_d != leitura_anterior){  //Lê contando as mudancas de leitura. Uma volta completa, numa roda de 20 perfurações, tem 40 mudanças de leitura.
    count+=1;  
  }
  leitura_anterior = leitura_d;
  if (t-t0 > intervalo){
  Serial.print("Leitura digital: "); Serial.println(leitura_d);
  vel_angular = count/(40.0*1000.0*intervalof); //vel_angular em rad/s
  t0 = millis();
  }
  Serial.print("Velocidade angular"); Serial.print(vel_angular); Serial.println(" rad/s");

  /* ==== Controle ==================================================================================================
  =================================================================================================================== */
  
  t_controle = micros(); // Tempo atual em microsegundos
  deltat = (t_controle - t_anterior)/1.0e6; // Diferença de tempo entre último loop e agora, em segundos.
  t_anterior = t_controle;
  
  erro = vel_ref - vel_angular; //Cálculo do sinal de erro
  deltae = erro - erro_anterior;
  erro_anterior = erro;

  integral += erro*deltat; //Calcula a integral como uma soma de Riemann
  derivada = deltae/deltat; //Calcula derivada como variação média sobre período pequeno

  controle = Kp*erro + Ki*integral + Kd*derivada; //Sinal de controle PID
  if (controle > 255) { //Limita o sinal de controle ao limite do sinal pwm
    controle = 255;
  }
  else if (controle < -255) {
    controle = -255;
  }
  pwm = (int) controle; //Projeta controle como inteiro sobre pwm
  acionarMotor1(pwm);
}

void acionarMotor1(int pwm) {
  if (pwm>0) {
    analogWrite(MOTOR1_PWM_R, pwm);
  }
  else {
    analogWrite(MOTOR1_PWM_L, -1*pwm);
  }
}



















