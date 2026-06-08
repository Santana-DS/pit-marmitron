#define ENCODER_D_IN 13
#define ENCODER_A_IN A0
#define MOTOR1_PWM_R A1
#define MOTOR1_PWM_L A2

void setup() {
  // put your setup code here, to run once:
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
  elif (controle < -255) {
    controle = -255;
  }
  pwm = (int) controle; //Projeta controle como inteiro sobre pwm
  acionarMotor(pwm);
}

void acionarMotor(int pwm) {
  if (pwm>0) {
    analogWrite(MOTOR1_PWM_R, pwm);
  }
  else {
    analogWrite(MOTOR1_PWM_L, -1*pwm);
  }
}



















