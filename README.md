# Système de Mise à Jour Sécurisé OTA (ESP8266)

Projet réalisé par **Mohamed Aziz Aouachri**.
Ce dépôt contient le code source complet pour un système de mise à jour à distance sécurisé.

## Structure du Projet

- **Client_Arduino/** : Contient les firmwares pour l'ESP8266.
  - `V1.0` : Version de base (Prix standard).
  - `V2.0` : Version "Soldes" (Logique métier modifiée).
  et ainsi de suites de firmwares et de versions

- **Serveur_Python/** : Contient le back-end de gestion.
  - `ota_server.py` : Serveur Flask gérant la distribution et le calcul SHA-256.

## Fonctionnalités Clés
- Vérification d'intégrité via SHA-256.
- Persistance de version via EEPROM.
- Interface utilisateur simulée sur LCD I2C.

## Comment lancer le projet
1. Lancer le script Python : `python ota_server.py`
2. Alimenter l'ESP8266.
