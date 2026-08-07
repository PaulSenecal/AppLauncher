#!/usr/bin/env python3
"""
Authentifie un manifest.json avec le secret HMAC généré par keygen.py.
Aucune dépendance externe (juste la stdlib - hmac/hashlib).

Usage:
    python sign_manifest.py <manifest.json> <secret.b64>

Réécrit le manifest.json en place avec le champ "signature" rempli (le nom
du champ reste "signature" pour la compatibilité du format, même si c'est
techniquement un tag HMAC ici, pas une signature asymétrique).

IMPORTANT : la charge authentifiée doit être construite EXACTEMENT comme
GameManifest::canonicalPayload() côté C++ (src/core/GameManifest.cpp), sinon
la vérification échouera côté launcher. Si tu modifies l'un des deux,
modifie l'autre en miroir.
"""
import argparse
import base64
import hashlib
import hmac
import json
from pathlib import Path

def canonical_payload(manifest: dict) -> bytes:
    lines = [manifest["version"], manifest["exe"]]
    for f in sorted(manifest["files"], key=lambda f: f["path"]):
        lines.append(f'{f["path"]}|{f["sha256"]}|{f["size"]}')
    return ("\n".join(lines) + "\n").encode("utf-8")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest_path")
    parser.add_argument("secret_path")
    args = parser.parse_args()

    manifest_path = Path(args.manifest_path)
    manifest = json.loads(manifest_path.read_text())

    secret = base64.b64decode(Path(args.secret_path).read_text().strip())

    payload = canonical_payload(manifest)
    tag = hmac.new(secret, payload, hashlib.sha256).digest()
    manifest["signature"] = base64.b64encode(tag).decode()

    manifest_path.write_text(json.dumps(manifest, indent=2))
    print(f"Manifest authentifié (HMAC-SHA256): {manifest_path}")

if __name__ == "__main__":
    main()
