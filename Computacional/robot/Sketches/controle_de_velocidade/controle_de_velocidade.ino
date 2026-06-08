#include <Arduino.h>

#define ENCODER_D_IN 27
#define MOTOR1_R_PWM 32
#define MOTOR1_L_PWM 33
#define MOTOR1_R_EN 25
#define MOTOR1_L_EN 26

// Variáveis leitura dos encoders
float vel_medida = 0.0;
float vel_filtrada = 0.0;
#define ALPHA  0.25 //alpha = tau/(tau+Ts), tau cte de tempo do filtro
volatile unsigned long t_encoder, t_anterior_encoder, periodo_quarta_volta;
unsigned long t_tol_encoder = 1;
const unsigned long periodo_amostragem_vel = 100;
unsigned long t_amostragem_anterior = millis();
volatile int conta_encoder = 0;

// Variáveis controle PID
unsigned long t_anterior = micros();
unsigned long t_controle;
float deltat;
#define KP 0.5
#define KI 0.1
#define KD 0.0
float integral = 0;
float derivada = 0;
float erro = 0;
float vel_ref = 200;
int sentido = 1;
float erro_anterior = 0;
float deltae = 0;
float controle = 0;
int pwm = 0;

unsigned long t_espera = 5000;
unsigned long t0 = millis();
unsigned long t;
bool controle_liberado = 0;
//

void IRAM_ATTR lerEncoderISR(){
  conta_encoder ++;
  if (conta_encoder>=10){
    t_encoder = micros();
    periodo_quarta_volta = t_encoder - t_anterior_encoder; 
    t_anterior_encoder = t_encoder;
    conta_encoder = 0;
  }
}

float zonaMorta(float sinal, float zona_morta){
    if (sinal<=zona_morta) return 0.0;
    else return sinal;
}

void ControleDeVelocidade_setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(ENCODER_D_IN, INPUT);
  pinMode(MOTOR1_R_PWM, OUTPUT);
  pinMode(MOTOR1_L_PWM, OUTPUT);
  pinMode(MOTOR1_R_EN, OUTPUT);
  pinMode(MOTOR1_L_EN, OUTPUT);
  digitalWrite(MOTOR1_R_EN, HIGH);
  digitalWrite(MOTOR1_L_EN, HIGH);
  Serial.println("Escreva velocidade de referência (rpm)");

  attachInterrupt(digitalPinToInterrupt(ENCODER_D_IN), lerEncoderISR, CHANGE);
}

void ControleDeVelocidade_loop() {
  t = millis();
  /* ===== Leitura de velocidade ====================================================================================
  =================================================================================================================== */
  Serial.print("conta_encoder:"); Serial.print(conta_encoder);
  Serial.print(",periodo_quarta_volta:"); Serial.print(periodo_quarta_volta);
  Serial.print(",vel_ref:"); Serial.print(vel_ref);

  if (t-t_amostragem_anterior >= periodo_amostragem_vel) {
    noInterrupts();
    if (periodo_quarta_volta != 0.0) {
      vel_medida = (float) 60.0*1000000.0/(4*periodo_quarta_volta); //rpm
    }
    //else {
      //vel_medida = 0.0;
    //}
    
    t_amostragem_anterior = millis();
    interrupts();
  }

  Serial.print(",vel_medida:"); Serial.print(vel_medida);  
  vel_filtrada = ALPHA*vel_filtrada + (1.0-ALPHA)*vel_medida; //Filtro passa baixas
  Serial.print(",vel_filtrada:"); Serial.print(vel_filtrada);  
  

  /* ==== Controle ==================================================================================================
  =================================================================================================================== */
  if (Serial.available()>2) {
    int incoming = Serial.read();
    vel_ref = (float) incoming;
    if (vel_ref >= 0) {sentido = 1;}
    else if (vel_ref < 0) {
      sentido = -1;
      vel_ref = -1*vel_ref;
      }
  }
  vel_ref = 0.0;
  pwm = 0;
  if (t-t0>500){
    //vel_ref = 50;
    pwm = 30;
  }
  if (t-t0>6000){
    //vel_ref = 100;
    pwm = 50;
  }
  
  if (controle_liberado || t-t0>t_espera) { //Espera a leitura inicial estabilizar
    controle_liberado = 1;
    
    t_controle = micros(); // Tempo atual em microsegundos
    deltat = (float) (t_controle - t_anterior)/1.0e6; // Diferença de tempo entre último loop e agora, em segundos.
    //Serial.print("Delta t: "); Serial.println(deltat);
    t_anterior = t_controle;
    
    erro = vel_ref - vel_filtrada; //Cálculo do sinal de erro
    erro = zonaMorta(erro, 0.0001);
    deltae = erro - erro_anterior;
    //Serial.print("Delta Erro: "); Serial.println(deltae);
    erro_anterior = erro;

    integral += erro*deltat; //Calcula a integral como uma soma de Riemann
    derivada = deltae/deltat; //Calcula derivada como variação média sobre período pequeno
    //Serial.print("Integral: "); Serial.println(integral);
    //Serial.print("Derivada: "); Serial.println(derivada);

    controle = KP*erro + KI*integral + KD*derivada; //Sinal de controle PID
    //Serial.print("Saída PID: "); Serial.println(controle);

    //pwm = (int) constrain(controle, 0, 255); //Projeta controle como inteiro sobre pwm
    Serial.print(",pwm:"); Serial.println(pwm);
    acionarMotor(sentido*pwm);
  }
}

void acionarMotor(int pwm) {
  if (pwm>0) {
    digitalWrite(MOTOR1_R_EN, HIGH);
    digitalWrite(MOTOR1_L_EN, HIGH);
    analogWrite(MOTOR1_L_PWM, 0);
    analogWrite(MOTOR1_R_PWM, pwm);
  }
  else {
    digitalWrite(MOTOR1_R_EN, HIGH);
    digitalWrite(MOTOR1_L_EN, HIGH);
    analogWrite(MOTOR1_R_PWM, 0);
    analogWrite(MOTOR1_L_PWM, -1*pwm);
  }
}

void setup(){
  ControleDeVelocidade_setup();
};

void loop(){
  ControleDeVelocidade_loop();
  delay(5);
};