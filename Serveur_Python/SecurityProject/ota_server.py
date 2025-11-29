# ota_server.py

from flask import Flask, jsonify, request, send_file
import os
import hashlib
import json

app = Flask(__name__)

# --- CONFIGURATION (À VÉRIFIER) ---
API_KEY_REQUIRED = "OTA_KEY_2025"
FIRMWARE_FOLDER = "firmwares"

# PARAMÈTRES RÉSEAU
SERVER_IP = "192.168.1.14"  # <<< VOTRE ADRESSE IP
SERVER_PORT = 3000  # <<< CHANGÉ: Port HTTP (3000)
PROTOCOL = "http"  # <<< AJOUTÉ: Pour les URLs

# CHEMINS DES CERTIFICATS (Inutiles en HTTP)
CERT_PATH = "server.crt"
KEY_PATH = "server.key"

# --- SIMULATION DE VERSIONS DISPONIBLES (inchangé) ---
AVAILABLE_FIRMWARES = {
    "aouachri-1.0": {"description": "Version de base 100 DT", "file": "v1.0.bin"},
    "aouachri-2.0": {"description": "Version Aouachri PROJET V2.0", "file": "v2.0.bin"},
    "aouachri-3.0": {"description": "Version Promo Finale V3.0", "file": "v3.0.bin"}  # AJOUTÉ

}
LATEST_VERSION = "aouachri-1.0"


# -------------------------------------------------------------------------------------

def calculate_sha256(filepath):
    """Calcule le SHA256 d'un fichier binaire."""
    hash_sha256 = hashlib.sha256()
    try:
        with open(filepath, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b''):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()
    except Exception as e:
        print(f"Erreur lors du calcul du SHA256 pour {filepath}: {e}")
        return ""


def get_firmware_info(version_key):
    """Récupère les infos d'une version spécifique et calcule le hash."""
    if version_key not in AVAILABLE_FIRMWARES:
        return None

    file_name = AVAILABLE_FIRMWARES[version_key]["file"]
    file_path = os.path.join(FIRMWARE_FOLDER, file_name)

    if not os.path.exists(file_path):
        return None

    sha256_hash = calculate_sha256(file_path)

    # URL de téléchargement (HTTP)
    download_url = f"{PROTOCOL}://{SERVER_IP}:{SERVER_PORT}/firmware/download/{file_name}"

    return {
        "version": version_key,
        "url": download_url,
        "sha256": sha256_hash,
        "description": AVAILABLE_FIRMWARES[version_key]["description"]
    }


def check_api_key():
    """Vérifie l'API Key dans l'en-tête (check/latest) ou dans les arguments URL (download)."""
    client_api_key = request.headers.get('x-api-key')
    if client_api_key == API_KEY_REQUIRED:
        return True

    client_api_key = request.args.get('api_key')
    if client_api_key == API_KEY_REQUIRED:
        return True

    print(f"Tentative non autorisée avec la clé: {client_api_key}")
    return False


@app.route('/firmware/latest', methods=['GET'])
def check_for_update():
    """Endpoint appelé par l'ESP8266 pour vérifier la version disponible."""

    if not check_api_key():
        return jsonify({"error": "Unauthorized"}), 401

    print("Clé API valide. Vérification de la dernière version...")

    info = get_firmware_info(LATEST_VERSION)

    if info:
        del info['description']
        print(f"Dernière version: {info['version']}, URL: {info['url']}")
        return jsonify(info), 200
    else:
        print(f"Erreur: Fichier binaire non trouvé pour la dernière version ({LATEST_VERSION}).")
        return jsonify({"error": "Firmware not available"}), 500


@app.route('/firmware/download/<filename>', methods=['GET'])
def download_firmware(filename):
    """Endpoint pour le téléchargement du fichier binaire."""

    if not check_api_key():
        return jsonify({"error": "Unauthorized"}), 401

    file_path = os.path.join(FIRMWARE_FOLDER, filename)
    if not os.path.exists(file_path):
        print(f"Fichier non trouvé: {filename}")
        return jsonify({"error": "File not found"}), 404

    print(f"Téléchargement du fichier {filename}...")

    return send_file(file_path, as_attachment=True, download_name=filename, mimetype='application/octet-stream')


if __name__ == '__main__':
    if not os.path.exists(FIRMWARE_FOLDER):
        os.makedirs(FIRMWARE_FOLDER)
        print(f"Dossier '{FIRMWARE_FOLDER}' créé. Placez-y vos .bin.")

    print(f"\nServeur OTA démarré en HTTP sur http://{SERVER_IP}:{SERVER_PORT}")
    print(f"Dernière version proposée: {LATEST_VERSION}")

    try:
        # Lancer le serveur Flask en mode HTTP (sans ssl_context)
        app.run(
            host=SERVER_IP,
            port=SERVER_PORT,
            debug=True
        )
    except Exception as e:
        print("\n!!! ERREUR DE DÉMARRAGE DU SERVEUR !!!")
        print(
            f"Assurez-vous que l'adresse IP {SERVER_IP} est correcte et que le port {SERVER_PORT} est libre et ouvert dans votre pare-feu.")
        print(f"Détails de l'erreur: {e}")