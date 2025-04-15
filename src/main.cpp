#include <ESP32Servo.h>

#define PULSE_ENCODEUR 13
#define PWM_MOTEUR 12
#define VITESSE_MINIMALE 1050 
#define VITESSE_MAXIMALE 1950
#define FREQ 50

const float pulseParRotation = 14;
const float multiplicateurBoiteAVitesse = (1 + (46 / 17)) * (1 + (46 / 17)) * (1 + (46 / 17));


const int longeurMaxVitessesMoyennesMoteur = 16;
float vitessesMoyennesMoteur[longeurMaxVitessesMoyennesMoteur] = {}; 
int indexVitessesMoyennesMoteur = 0;
int longueurVitessesMoyennesMoteur = 0;
 
Servo moteur;

void setVitesseMoteur(float vitesse) {
  float entreeMoteur = vitesse * (VITESSE_MAXIMALE - VITESSE_MINIMALE) + VITESSE_MINIMALE;
  moteur.write(entreeMoteur);
}

void setup() {
  Serial.begin(115200);
  pinMode(PULSE_ENCODEUR, INPUT);
  pinMode(PWM_MOTEUR, OUTPUT);
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  moteur.setPeriodHertz(FREQ);
  moteur.attach(PWM_MOTEUR, VITESSE_MINIMALE, VITESSE_MAXIMALE);
}

float lireVitesseMoteur() {
  float encodeurMicros = pulseIn(PULSE_ENCODEUR, HIGH, 300000);
  if (encodeurMicros == 0) {
    return 0;
  } 

  float vitesseRPS = ((1000000.0 / encodeurMicros) / pulseParRotation) / multiplicateurBoiteAVitesse;
  float entreeRPM = vitesseRPS * 60;
  vitessesMoyennesMoteur[indexVitessesMoyennesMoteur++] = entreeRPM;
  indexVitessesMoyennesMoteur %= longeurMaxVitessesMoyennesMoteur;
  longueurVitessesMoyennesMoteur = min(longueurVitessesMoyennesMoteur + 1, longeurMaxVitessesMoyennesMoteur);

  float vitesseRPM = 0;

  for (int i = 0; i < longueurVitessesMoyennesMoteur; i++) {
    vitesseRPM += vitessesMoyennesMoteur[i];
  }

  return vitesseRPM / longueurVitessesMoyennesMoteur;
}

void loop() {
  Serial.println("Min speed");
  setVitesseMoteur(0.0);
  delay(2000);
  
  Serial.println("Mid speed");
  setVitesseMoteur(0.5);
  delay(2000);
  
  Serial.println("Max speed");
  setVitesseMoteur(1.0);
  delay(2000);
}