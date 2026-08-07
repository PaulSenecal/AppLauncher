#!/usr/bin/env python3
"""
Scanne le dossier de build d'UN jeu et génère son manifest.json (sans
signature - c'est sign_manifest.py qui l'ajoute ensuite).

Usage:
    python generate_manifest.py <dossier_build> <version> <exe_base_name> [--changelog "ligne1" "ligne2" ...]

Exemple:
    python generate_manifest.py ./build/stargate 1.3.0 stargate \
        --changelog "Correction du crash au chargement" "Ajout du mode sombre"

Produit <dossier_build>/manifest.json, prêt à être signé puis déposé sur le
serveur dans games/<id>/ aux côtés des fichiers eux-mêmes.
"""
import argparse
import hashlib
import json
from pathlib import Path

def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("version")
    parser.add_argument("exe_base_name")
    parser.add_argument("--changelog", nargs="*", default=[])
    args = parser.parse_args()

    build_dir = Path(args.build_dir).resolve()
    files = []
    for path in sorted(build_dir.rglob("*")):
        if path.is_file() and path.name != "manifest.json":
            rel = path.relative_to(build_dir).as_posix()
            files.append({
                "path": rel,
                "sha256": sha256_of(path),
                "size": path.stat().st_size,
            })

    manifest = {
        "version": args.version,
        "exe": args.exe_base_name,
        "changelog": args.changelog,
        "files": files,
        "signature": "",  # rempli par sign_manifest.py
    }

    out_path = build_dir / "manifest.json"
    out_path.write_text(json.dumps(manifest, indent=2))
    print(f"{len(files)} fichiers -> {out_path}")
    print("N'oublie pas de lancer sign_manifest.py avant de publier.")

if __name__ == "__main__":
    main()
