#!/usr/bin/env python3
"""
Génère UNE fois pour toutes le secret HMAC partagé utilisé pour authentifier
les manifests de tous tes jeux. Aucune dépendance externe (juste la stdlib).

Usage:
    python keygen.py

Sortie :
    secret.b64  -> à garder STRICTEMENT hors du dépôt git public, hors du
                   serveur de distribution. Sert à sign_manifest.py ET doit
                   être copié tel quel dans src/ui/MainWindow.cpp
                   (kEmbeddedSecretBase64).

ATTENTION : contrairement à une paire de clés asymétrique, ce secret N'EST
PAS un secret "serveur uniquement" - il finit obligatoirement embarqué dans
le launcher distribué aux joueurs, donc potentiellement extractible par
rétro-ingénierie du binaire. Voir ManifestAuthenticator.h pour le détail du
compromis. Si tu veux une garantie plus forte, il faut une vraie signature
asymétrique (libsodium/OpenSSL) - hors scope ici pour rester 100% Qt natif.
"""
import base64
import secrets

def main():
    secret = secrets.token_bytes(32)  # 256 bits
    secret_b64 = base64.b64encode(secret).decode()

    with open("secret.b64", "w") as f:
        f.write(secret_b64)

    print("Secret écrit dans secret.b64 -- NE JAMAIS PARTAGER CE FICHIER PUBLIQUEMENT")
    print()
    print("À coller dans src/ui/MainWindow.cpp :")
    print(f'    static const char *kEmbeddedSecretBase64 = "{secret_b64}";')

if __name__ == "__main__":
    main()
