#include "ctrl_velocidade.h"
#include <Arduino.h>

#define ENCODER_D_IN 27
#define MOTOR1_R_PWM 32
#define MOTOR1_L_PWM 33
#define MOTOR1_R_EN 25
#define MOTOR1_L_EN 26

// Variáveis leitura dos encoders
float vel_medida;
float vel_filtrada = 0; //rpm
#define ALPHA  0.7 //alpha = tau/(tau+Ts), tau cte de tempo do filtro
volatile unsigned long t_encoder, t_anterior_encoder, periodo_quarta_volta; //micros
volatile int conta_encoder = 0;
const unsigned long periodo_amostragem_vel = 100; //10 Hz
unsigned long t_amostragem_anterior = millis();


// Variáveis controle PID
unsigned long t_anterior = micros();
unsigned long t_controle; //micros
float deltat; //segundos
#define KP 0.7
#define KI 0.1
#define KD 0.0
float integral = 0;
float derivada = 0;
float erro = 0;
//float vel_ref = 50; //rpm
int sentido = 1;
float erro_anterior = 0;
float deltae = 0;
float controle = 0;
int pwm = 0;

unsigned long t0 = millis();
unsigned long t;
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


void ControleDeVelocidade_setup(motor* p_motor1) {
  p_motor1->acionaMotor(40);
  pinMode(ENCODER_D_IN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_D_IN), lerEncoderISR, CHANGE);

}


void ControleDeVelocidade_loop(motor* p_motor1, float vel_ref) {

  t = millis();

  // ===== Leitura de velocidade ====================================================================================
  //=================================================================================================================
  
  if (t-t_amostragem_anterior >= periodo_amostragem_vel) {
    noInterrupts();
    if (periodo_quarta_volta != 0.0) {
      vel_medida = (float) 60.0*1000000.0/(4.0*periodo_quarta_volta);
    }
    t_amostragem_anterior = millis();
    interrupts();
  }
  Serial.print(">vel_medida:"); Serial.print(vel_medida);
  vel_filtrada = ALPHA*vel_filtrada + (1.0-ALPHA)*vel_medida; //Filtro passa baixas
  Serial.print(",vel_filtrada: "); Serial.print(vel_filtrada);  Serial.print(",");
  Serial.print(",periodo_quarta_volta"); Serial.print(periodo_quarta_volta);
  Serial.print(",conta_encoder:"); Serial.print(conta_encoder);
  

  // ==== Controle ==================================================================================================
  //=================================================================================================================

  if (t-t0>500){
    vel_ref = 50;
    //pwm = 30;
  }
  if (t-t0>10000){
    vel_ref = 100;
    //pwm = 50;
  }
  if (t-t0>25000){
    vel_ref = 300;
    //pwm = 200;
  }
  if (t-t0>33000){
    vel_ref = 200;
    //pwm = 100;
  }
  
  Serial.print("vel_ref:"); Serial.print(vel_ref); Serial.print(",");
  t_controle = micros(); // Tempo atual em microsegundos
  deltat = (float) (t_controle - t_anterior)/1.0e6; //segundos
  //Serial.print("delta_t:"); Serial.print(deltat); Serial.print(",");
  t_anterior = t_controle;
  
  erro = vel_ref - vel_filtrada; //Cálculo do sinal de erro
  erro = zonaMorta(erro, 0.001);
  //Serial.print("erro:"); Serial.print(erro); Serial.print(",");
  deltae = erro - erro_anterior;
  //Serial.print("delta_erro:"); Serial.print(deltae);
  erro_anterior = erro;

  integral += erro*deltat; //Calcula a integral como uma soma de Riemann
  derivada = deltae/deltat; //Calcula derivada como variação média sobre período pequeno
  Serial.print("integral:"); Serial.print(integral); Serial.print(",");
  //Serial.print("derivada:"); Serial.print(derivada); Serial.print(",");

  controle = KP*erro + KI*integral + KD*derivada; //Sinal de controle PID
  //Serial.print(",saida_PID:"); Serial.print(controle);

  pwm = (int) constrain(controle, 0, 255); //Projeta controle como inteiro sobre pwm
  Serial.print(",pwm:"); Serial.println(pwm);
  p_motor1->acionaMotor(sentido*pwm);

}