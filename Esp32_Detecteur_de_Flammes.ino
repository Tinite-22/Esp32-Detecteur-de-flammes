#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// =========================================================================
// 1. CONFIGURATION
// =========================================================================
const char* ssid = "Votre_SSID_Wifi";
const char* mot_de_passe = "Votre_mot_de_passe_Wifi";

// Remplacez ceci par le Token donné par @BotFather sur Telegram
const String TOKEN_BOT = "Votre_token_bot";

// Ajoutez votre ID de Chat ici (gardez les guillemets)
const String ID_CHAT_AUTORISE = "Votre_ID_chat";

// =========================================================================
// 2. MATÉRIEL
// =========================================================================
const int brocheIR     = 27;
const int brocheBuzzer = 26;

// =========================================================================
// 3. ÉTAT PARTAGÉ ENTRE LES DEUX CŒURS (volatile + Mutex)
// =========================================================================
volatile bool systemeArme      = true;
volatile bool alarmeActive     = false;
volatile bool armementEnAttente = false;
volatile unsigned long heureArmementPrevue = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // Verrou pour l'accès simultané (concurrence)

// =========================================================================
// 4. COMMUNICATION
// =========================================================================
WiFiClientSecure clientSecurise;
UniversalTelegramBot bot(TOKEN_BOT, clientSecurise);

// =========================================================================
// 5. GESTIONNAIRE DE MESSAGES TELEGRAM
// =========================================================================
void gererNouveauxMessages(int nbNouveauxMessages) {
  for (int i = 0; i < nbNouveauxMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String texte   = bot.messages[i].text;

    if (chat_id != ID_CHAT_AUTORISE) {
      bot.sendMessage(chat_id, "⛔ Accès refusé.", "");
      continue;
    }

    if (texte == "Statut") {
      String statut = "Statut du Système :\n";
      statut += systemeArme  ? "🛡️ SYSTÈME ARMÉ\n"    : "💤 SYSTÈME EN MODE VEILLE\n";
      statut += alarmeActive ? "🚨 ALARME EN COURS !\n" : "✅ PAS DE FLAMME.\n";
      bot.sendMessage(chat_id, statut, "");
    }

    else if (texte == "Désactiver l'alarme") {
      portENTER_CRITICAL(&mux);
        alarmeActive = false;
        systemeArme  = false;
      portEXIT_CRITICAL(&mux);
      digitalWrite(brocheBuzzer, LOW);
      bot.sendMessage(chat_id, "🔥 Alarme désactivée. Système en mode veille.", "");
    }

    else if (texte == "Activer l'alarme") {
      // Envoie le message AVANT d'armer, sans utiliser de delay() bloquant
      bot.sendMessage(chat_id, "🛡️ Activation dans 15s.......", "");
      
      // Minuterie non bloquante : enregistre l'heure d'armement future
      // Le cœur de détection vérifiera cette valeur
      heureArmementPrevue = millis() + 15000;
      armementEnAttente = true;
    }
  }
}

// =========================================================================
// 6. TÂCHE DU CŒUR 0 — Détection IR + Buzzer (Temps réel)
// =========================================================================
void tacheDetection(void* param) {
  unsigned long dernierBip      = 0;
  bool          etatBuzzer      = false;
  bool          dernierEtatIR   = HIGH;  // Pour l'anti-rebond
  unsigned long tempsAntiRebond = 0;
  const int     DELAI_ANTI_REBOND_MS = 50; // Ignore les transitions < 50ms

  while (true) {
    bool lectureIR = digitalRead(brocheIR);
    unsigned long maintenant = millis();

    // --- Anti-rebond du capteur IR ---
    if (lectureIR != dernierEtatIR) {
      tempsAntiRebond = maintenant; // Réinitialise le chrono à chaque changement
    }
    if ((maintenant - tempsAntiRebond) > DELAI_ANTI_REBOND_MS) {
      // Signal stable pendant DELAI_ANTI_REBOND_MS -> on le traite
      if (lectureIR == LOW) {  // Obstacle / Flamme détecté(e)
        portENTER_CRITICAL(&mux);
        bool estArme = systemeArme;
        bool alarmeDejaActive = alarmeActive;
        portEXIT_CRITICAL(&mux);

        if (estArme && !alarmeDejaActive) {
          portENTER_CRITICAL(&mux);
            alarmeActive = true;
          portEXIT_CRITICAL(&mux);
          Serial.println(">>> FLAMME DÉTECTÉE <<<");
          // Note : appeler sendMessage ici serait dangereux (le Wi-Fi tourne sur le Cœur 1)
          // On lève juste le drapeau (flag), Telegram le détectera sur le Cœur 1
        }
      }
    }
    dernierEtatIR = lectureIR;

    // --- Armement Différé (Remplace delay(15000)) ---
    if (armementEnAttente && maintenant >= heureArmementPrevue) {
      portENTER_CRITICAL(&mux);
        systemeArme        = true;
        alarmeActive       = false;
        armementEnAttente  = false;
      portEXIT_CRITICAL(&mux);
      Serial.println("Système ARMÉ.");
    }

    // --- Buzzer Non-bloquant ---
    portENTER_CRITICAL(&mux);
    bool statutAlarmeActuel = alarmeActive;
    portEXIT_CRITICAL(&mux);

    if (statutAlarmeActuel) {
      if (maintenant - dernierBip > 300) {
        etatBuzzer = !etatBuzzer;
        digitalWrite(brocheBuzzer, etatBuzzer ? HIGH : LOW);
        dernierBip = maintenant;
      }
    } else {
      // S'assure que le buzzer est ÉTEINT si l'alarme est arrêtée
      if (etatBuzzer) {
        etatBuzzer = false;
        digitalWrite(brocheBuzzer, LOW);
      }
    }

    vTaskDelay(1); // Cède 1ms au planificateur FreeRTOS (évite le déclenchement du chien de garde / watchdog)
  }
}

// =========================================================================
// 7. TÂCHE DU CŒUR 1 — Telegram + Notification d'Alarme
// =========================================================================
void tacheTelegram(void* param) {
  unsigned long derniereVerification = 0;
  bool          alerteEnvoyee        = false; // Empêche le spam de messages
  bool          confirmArmementEnvoyee = false;

  while (true) {
    unsigned long maintenant = millis();

    // --- Notification d'Alarme (Envoi unique) ---
    if (alarmeActive && !alerteEnvoyee) {
      bot.sendMessage(ID_CHAT_AUTORISE, "🚨 Alerte : Flamme détectée par le capteur ! 🚨", "");
      alerteEnvoyee = true;
    }
    if (!alarmeActive) alerteEnvoyee = false; // Réinitialise si l'alarme est arrêtée

    // --- Notification de Confirmation d'Armement ---
    if (!armementEnAttente && confirmArmementEnvoyee == false && systemeArme) {
      // Logique pour une confirmation unique après l'armement
    }

    // --- Interrogation de Telegram (Toutes les 2s, moins agressif) ---
    if (maintenant - derniereVerification > 2000) {
      int nbNouveauxMessages = bot.getUpdates(bot.last_message_received + 1);
      while (nbNouveauxMessages) {
        gererNouveauxMessages(nbNouveauxMessages);
        nbNouveauxMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      derniereVerification = maintenant;
    }

    vTaskDelay(10);
  }
}

// =========================================================================
// 8. INITIALISATION (SETUP)
// =========================================================================
void setup() {
  Serial.begin(115200);
  pinMode(brocheIR, INPUT);
  pinMode(brocheBuzzer, OUTPUT);
  digitalWrite(brocheBuzzer, LOW);

  WiFi.begin(ssid, mot_de_passe);
  Serial.print("Connexion au Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnecté !");

  clientSecurise.setInsecure();
  bot.sendMessage(ID_CHAT_AUTORISE, "🔌 Système en ligne et Armé.", "");

  // Lancement des deux tâches sur leurs cœurs respectifs
  xTaskCreatePinnedToCore(tacheDetection, "Detection", 4096, NULL, 2, NULL, 0); // Cœur 0, Priorité 2
  xTaskCreatePinnedToCore(tacheTelegram,  "Telegram",  8192, NULL, 1, NULL, 1); // Cœur 1, Priorité 1
}

// =========================================================================
// 9. BOUCLE (Vide — Les tâches FreeRTOS gèrent tout)
// =========================================================================
void loop() {
  vTaskDelay(1000);
}