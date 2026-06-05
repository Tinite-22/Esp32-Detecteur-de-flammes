# Esp32-Detecteur-de-flammes
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge&logo=espressif)
![Framework: FreeRTOS](https://img.shields.io/badge/Framework-FreeRTOS-violet?style=for-the-badge)
![API: Telegram](https://img.shields.io/badge/API-Telegram-2CA5E0?style=for-the-badge&logo=telegram)
![Language: C++](https://img.shields.io/badge/Language-C++-green?style=for-the-badge&logo=c%2B%2B)

Un système d'alarme intelligent et ultra-réactif basé sur un ESP32, conçu pour détecter des flammes (ou des obstacles) à l'aide d'un capteur Infrarouge (IR). Il vous alerte instantanément sur votre smartphone via Telegram.

L'innovation de ce projet réside dans son **architecture logicielle multicœur basée sur FreeRTOS**. La lecture du capteur (temps réel) et la communication réseau (bloquante par nature) sont isolées sur les deux cœurs distincts du processeur, garantissant qu'une latence Wi-Fi n'empêchera jamais l'alarme de se déclencher.

---

## Fonctionnalités Principales

- **Traitement Multicœur (FreeRTOS) :** - **Cœur 0 (Priorité 2) :** Dédié à la sécurité matérielle en temps réel (lecture IR, anti-rebond, sirène).
  - **Cœur 1 (Priorité 1) :** Dédié à la connectivité Wi-Fi et à la messagerie Telegram.
- **Contrôle à distance Telegram :** Activez, désactivez ou consultez le statut de l'alarme n'importe où dans le monde.
- **Notifications Push :** Recevez un message d'alerte instantané en cas de détection.
- **Accès Sécurisé :** Les commandes sont filtrées par un `ID_CHAT` unique. Les inconnus ne peuvent pas interagir avec votre bot.
- **Multitâche Non-Bloquant :** Utilisation de `millis()` pour les temporisations (ex: délai d'armement de 15s, clignotement du buzzer) sans jamais geler le système avec `delay()`.
- **Thread-Safety (Mutex) :** Protection des variables partagées entre les cœurs via des blocs de section critique (`portENTER_CRITICAL`).

---

## Configuration Matérielle

| Composant | Broche ESP32 (GPIO) | Description |
| :--- | :--- | :--- |
| **Capteur IR (Flamme/Obstacle)** | `GPIO 27` | Entrée (État LOW = Détection) |
| **Buzzer Actif** | `GPIO 26` | Sortie (Sirène sonore non-bloquante) |

---

## Configuration du Bot Telegram

Avant de téléverser le code, vous devez configurer votre bot Telegram :

1. **Créer le Bot :** Ouvrez Telegram, cherchez **@BotFather**, envoyez `/newbot` et suivez les instructions. Il vous donnera un **Token** (ex: `123456789:ABCDEF...`).
2. **Obtenir votre ID :** Cherchez **@userinfobot** ou **@RawDataBot** sur Telegram, envoyez `/start` et notez votre `Id` (ex: `987654321`).
3. **Initier le chat :** Cherchez votre nouveau bot sur Telegram et envoyez-lui `/start` (indispensable pour qu'il puisse vous écrire en premier).

---

## Installation & Déploiement

1. Clonez ce dépôt.
2. Ouvrez le fichier dans **Arduino IDE** (ou PlatformIO).
3. Installez les bibliothèques requises via le gestionnaire :
   - `UniversalTelegramBot` par Brian Lough
   - `ArduinoJson` par Benoit Blanchon (Version 6.x recommandée)
4. Dans le code source, modifiez la section **Configuration** :
   ```cpp
   const char* ssid = "VOTRE_WIFI";
   const char* mot_de_passe = "VOTRE_MOT_DE_PASSE";
   const String TOKEN_BOT = "VOTRE_TOKEN_BOT";
   const String ID_CHAT_AUTORISE = "VOTRE_ID_CHAT";


5. Sélectionnez votre carte ESP32 et téléversez le code à **115200 bauds**.

---

## Commandes Telegram Disponibles

Une fois le système connecté, envoyez ces commandes à votre bot :

* `Statut` : Affiche l'état actuel (Armé / En Veille) et s'il y a une alerte en cours.
* `Activer l'alarme` : Déclenche un délai de 15 secondes pour vous permettre de quitter la pièce avant l'armement.
* `Désactiver l'alarme` : Coupe la sirène immédiatement et remet le système en veille.

---

## Plongée dans l'Architecture (Pour les développeurs)

Ce code utilise des pratiques d'ingénierie embarquée avancées :

* **Anti-Rebond Logiciel (Debounce) :** Les capteurs IR peuvent être instables lors d'une transition. Le code exige que le signal soit constant pendant 50ms (`DELAI_ANTI_REBOND_MS`) avant de valider une flamme.
* **Verrous Mutex (`portMUX_TYPE`) :** Lorsqu'un cœur lit une variable (comme `alarmeActive`) et que l'autre cœur la modifie (lorsque vous envoyez "Désactiver" sur Telegram), il y a un risque de corruption mémoire. Les instructions `portENTER_CRITICAL(&mux)` verrouillent l'accès le temps de la lecture/écriture.
* **Délai Asynchrone :** L'activation différée (15s) n'utilise pas `delay(15000)` qui bloquerait le processeur. Le système enregistre `heureArmementPrevue = millis() + 15000` et continue de vérifier les autres capteurs à chaque cycle.

---

## 📄 Licence

Ce projet est distribué sous la licence MIT. Sentez-vous libre de le cloner, l'adapter et l'améliorer pour vos propres besoins domotiques !
