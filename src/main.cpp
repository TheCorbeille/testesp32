#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Permet de choisir le pin pour utilser, dependant des differents exigences
#define PWM_MOTEUR 25
// Vitesse maximale du moteur, trouvee en ligne dans les consignes du moteur
#define VITESSE_MINIMALE 1050
// Vitesse minmale du moteur, trouvee en ligne dans les consignes du moteur
#define VITESSE_MAXIMALE 1950
// Frequence pour utiliser le Servo. Une constante
#define FREQ 50
// Vitesse maximale du moteur en RPM
#define VITESSE_MAXIMALE_RPM 360

// Structure pour implémenter une Map simple
#define TAILLE_MAX_CARTE 30

typedef struct {
    float cle;
    void* valeur;
    bool estOccupe;
} EntreeCarte;

typedef struct {
    EntreeCarte entrees[TAILLE_MAX_CARTE];
    int taille;
    int capacite;
} Carte;

// Structure pour stocker une carte de distances->vitessesRPM
typedef struct {
    Carte distancesVitesses;
} DonneesAngle;

// Carte globale des angles -> données
Carte referencesDistances;

// Fonctions pour gérer les cartes
void initialiserCarte(Carte* carte) {
    carte->taille = 0;
    carte->capacite = TAILLE_MAX_CARTE;
    
    for (int i = 0; i < TAILLE_MAX_CARTE; i++) {
        carte->entrees[i].estOccupe = false;
        carte->entrees[i].valeur = NULL;
    }
}

int trouverIndexCle(Carte* carte, float cle) {
    for (int i = 0; i < carte->capacite; i++) {
        if (carte->entrees[i].estOccupe && carte->entrees[i].cle == cle) {
            return i;
        }
    }
    return -1;
}

void* obtenirValeur(Carte* carte, float cle) {
    int index = trouverIndexCle(carte, cle);
    if (index >= 0) {
        return carte->entrees[index].valeur;
    }
    return NULL;
}

bool definirValeur(Carte* carte, float cle, void* valeur) {
    // Vérifier si la clé existe déjà
    int index = trouverIndexCle(carte, cle);
    if (index >= 0) {
        carte->entrees[index].valeur = valeur;
        return true;
    }
    
    // Trouver un emplacement libre
    if (carte->taille >= carte->capacite) {
        Serial.println("Erreur: Carte pleine");
        return false;
    }
    
    // Trouver la première position libre
    for (int i = 0; i < carte->capacite; i++) {
        if (!carte->entrees[i].estOccupe) {
            carte->entrees[i].cle = cle;
            carte->entrees[i].valeur = valeur;
            carte->entrees[i].estOccupe = true;
            carte->taille++;
            return true;
        }
    }
    
    return false;
}

// Fonction pour trouver la valeur la plus proche dans une carte
float trouverValeurPlusProche(float valeur, Carte* carte) {
    if (carte->taille <= 0) {
        Serial.println("Erreur: Carte vide");
        return 0.0;
    }
    
    float cleAValeurMinimal = 0.0;
    float deltaValeurMinimal = INFINITY;
    bool premierElement = true;
    
    for (int i = 0; i < carte->capacite; i++) {
        if (carte->entrees[i].estOccupe) {
            float cle = carte->entrees[i].cle;
            float deltaValeur = fabs(cle - valeur);
            
            if (premierElement || deltaValeur < deltaValeurMinimal) {
                cleAValeurMinimal = cle;
                deltaValeurMinimal = deltaValeur;
                premierElement = false;
            }
        }
    }
    
    return cleAValeurMinimal;
}

// Fonction pour initialiser les données à partir d'un fichier CSV stocké dans SPIFFS
void initialiserDonneesCSV(const char* cheminFichier) {
    // Initialiser la carte principale
    initialiserCarte(&referencesDistances);
    
    // Ouvrir le fichier depuis SPIFFS
    File fichier = SPIFFS.open(cheminFichier, "r");
    if (!fichier) {
        Serial.println("Erreur: Impossible d'ouvrir le fichier");
        return;
    }
    
    char ligne[128]; // Ligne de taille maximum
    int indexLigne = 0;
    
    // Lire chaque ligne du fichier
    while (fichier.available()) {
        int caractere = fichier.read();
        if (caractere == '\n' || indexLigne >= 127) {
            // Terminer la ligne
            ligne[indexLigne] = '\0';
            
            // Traiter la ligne si elle n'est pas vide
            if (indexLigne > 0) {
                // Variables pour stocker les valeurs parsées
                float angle = 0.0;
                float vitesseRPM = 0.0;
                float distanceFinale = 0.0;
                
                // Analyser la ligne CSV
                char* ptr = ligne;
                
                // Lire l'angle
                angle = strtof(ptr, &ptr);
                if (*ptr == ',') ptr++;
                
                // Lire la vitesse RPM
                vitesseRPM = strtof(ptr, &ptr);
                if (*ptr == ',') ptr++;
                
                // Lire la distance finale
                distanceFinale = strtof(ptr, NULL);
                
                // Vérifier si l'angle existe déjà dans la carte
                DonneesAngle* donneesAngle = (DonneesAngle*)obtenirValeur(&referencesDistances, angle);
                
                if (donneesAngle == NULL) {
                    // Créer une nouvelle entrée pour cet angle
                    donneesAngle = (DonneesAngle*)malloc(sizeof(DonneesAngle));
                    if (donneesAngle == NULL) {
                        Serial.println("Erreur: Allocation mémoire échouée");
                        continue;
                    }
                    
                    initialiserCarte(&donneesAngle->distancesVitesses);
                    definirValeur(&referencesDistances, angle, donneesAngle);
                }
                
                // Ajouter la vitesse RPM pour cette distance
                float* nouvelleVitesse = (float*)malloc(sizeof(float));
                if (nouvelleVitesse == NULL) {
                    Serial.println("Erreur: Allocation mémoire échouée");
                    continue;
                }
                
                *nouvelleVitesse = vitesseRPM;
                definirValeur(&donneesAngle->distancesVitesses, distanceFinale, nouvelleVitesse);
            }
            
            // Réinitialiser l'index pour la prochaine ligne
            indexLigne = 0;
        } else if (caractere != '\r') {
            // Ajouter le caractère à la ligne courante
            ligne[indexLigne++] = (char)caractere;
        }
    }
    
    fichier.close();
    Serial.println("Données CSV chargées avec succès");
}

// Fonction pour obtenir la vitesse RPM pour un angle et une distance donnés
float obtenirVitesseRPM(float angle, float distance) {
    // Trouver l'angle le plus proche
    float angleClef = trouverValeurPlusProche(angle, &referencesDistances);
    
    // Obtenir les données pour cet angle
    DonneesAngle* donneesAngle = (DonneesAngle*)obtenirValeur(&referencesDistances, angleClef);
    if (donneesAngle == NULL) {
        Serial.println("Erreur: Aucune donnée disponible pour l'angle");
        return 0.0;
    }
    
    // Trouver la distance la plus proche
    float distanceClef = trouverValeurPlusProche(distance, &donneesAngle->distancesVitesses);
    
    // Obtenir la vitesse RPM pour cette distance
    float* vitesseRPM = (float*)obtenirValeur(&donneesAngle->distancesVitesses, distanceClef);
    if (vitesseRPM == NULL) {
        Serial.println("Erreur: Aucune vitesse RPM disponible pour la distance");
        return 0.0;
    }
    
    return *vitesseRPM;
}

Servo moteur;
float angleActuel = 0;     // Pour stocker l'angle courant
float vitesseConsigne = 0; // Pour stocker la vitesse demandée
float vitesseActuelle = 0; // Pour stocker la vitesse mesurée

void setVitesseMoteur(float x) {
  float entreeMoteur = 1.25 * x + 1500;
  moteur.writeMicroseconds(entreeMoteur);
}

// Fonction pour envoyer proprement un message JSON
void sendJsonResponse(const char* status, float vitesse, float angle) {
  // Buffer pour le message JSON - suffisamment grand pour éviter les débordements
  StaticJsonDocument<256> doc;
  
  doc["status"] = status;
  doc["vitesse"] = vitesse;
  doc["angle"] = angle;
  
  // Sérialiser et envoyer en une seule opération
  String jsonString;
  serializeJson(doc, jsonString);
  
  // S'assurer que tout est envoyé d'un coup pour éviter la fragmentation
  Serial.println(jsonString);
  Serial.flush(); // Attendre que tous les octets soient transmis
}

void setup() {
  // Initialiser la communication série AVANT tout
  Serial.begin(115200);
  delay(1000); // Attendre que la communication série s'établisse
  
  // Message de démarrage
  sendJsonResponse("starting", 0, 0);
  
  // Initialiser SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Erreur d'initialisation SPIFFS");
    return;
  }
  
  // Charger les données CSV depuis SPIFFS
  initialiserDonneesCSV("/donnees.csv");
  
  pinMode(2, OUTPUT);
  pinMode(PWM_MOTEUR, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  moteur.setPeriodHertz(FREQ);
  moteur.attach(PWM_MOTEUR, VITESSE_MINIMALE, VITESSE_MAXIMALE);
  
  // Message après initialisation complète
  sendJsonResponse("ready", 0, 0);
}

void loop() {
  if (Serial.available() > 0) {
    String inputString = "";
    
    // Lire la ligne complète
    while(Serial.available()) {
      char inChar = (char)Serial.read();
      inputString += inChar;
      delay(2); // Petit délai pour permettre l'arrivée de caractères supplémentaires
      if (inChar == '\n') break;
    }
    
    inputString.trim(); // Enlever les espaces et retours à la ligne
    
    // Message de débogage pour vérifier ce qui est reçu
    StaticJsonDocument<256> debugDoc;
    debugDoc["debug"] = "received";
    debugDoc["raw"] = inputString;
    String debugJson;
    serializeJson(debugDoc, debugJson);
    Serial.println(debugJson);
    Serial.flush();
    
    // Parser le JSON reçu
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, inputString);

    if (error) {
      // En cas d'erreur de parsing
      StaticJsonDocument<256> errorDoc;
      errorDoc["status"] = "error";
      errorDoc["message"] = error.c_str();
      String errorJson;
      serializeJson(errorDoc, errorJson);
      Serial.println(errorJson);
      Serial.flush();
      return;
    }

    // Traitement des valeurs
    float vitesse = 0;
    float angle = 0;
    
    // Vérifier si les clés existent
    if (doc.containsKey("vitesse")) {
      vitesse = doc["vitesse"];
    }
    
    if (doc.containsKey("angle")) {
      angle = doc["angle"];
      angleActuel = angle; // Mettre à jour l'angle actuel
    }
    
    // Si on a une distance, calculer la vitesse RPM correspondante
    if (doc.containsKey("distance-entree")) {
      float distance = doc["distance"];
      // Utiliser l'angle actuel et la distance pour déterminer la vitesse RPM
      vitesse = obtenirVitesseRPM(angleActuel, distance);
    }

    // Appliquer la vitesse au moteur
    setVitesseMoteur(vitesse);
    vitesseActuelle = vitesse; // Mettre à jour la vitesse actuelle

    // Allumer/éteindre la LED selon la vitesse
    digitalWrite(2, vitesse != 0);

    // Envoyer la réponse avec la fonction dédiée
    sendJsonResponse("ok", vitesse, angleActuel);
  }
  
  // Petit délai pour éviter de surcharger le CPU
  delay(10);
}