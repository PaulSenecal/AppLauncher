#pragma once
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QHash>
#include <QJsonDocument>
#include <optional>

// Un fichier (ou pak) décrit dans le manifest d'un jeu.
struct ManifestFile {
    QString path;     // chemin relatif dans le dossier du jeu, ex: "assets/textures.pak"
    QString sha256;   // hash attendu, en hexadécimal minuscule
    qint64  size = 0; // taille en octets, pour l'affichage et le check d'espace disque
};

// Représente manifest.json d'UN jeu, tel que publié par le serveur (ou sauvegardé
// localement après une installation réussie).
//
// Format attendu côté serveur :
// {
//   "version": "1.3.0",
//   "exe": "stargate",                     // exeBaseName, sans extension
//   "changelog": ["Point 1", "Point 2"],
//   "files": [
//     {"path": "stargate.exe", "sha256": "...", "size": 8421000},
//     {"path": "assets/textures.pak", "sha256": "...", "size": 45000000}
//   ],
//   "signature": "base64(ed25519 signature of the canonical payload below)"
// }
//
// La signature couvre EXACTEMENT les octets de `canonicalPayload()` — jamais le
// JSON brut tel quel (l'ordre des clés / espaces ne doivent pas influer sur la
// vérification). Voir tools/sign_manifest.py pour la génération côté serveur.
class GameManifest {
public:
    QString version;
    QString exeBaseName;
    QStringList changelog;
    QVector<ManifestFile> files;
    QByteArray signatureB64; // tel que lu dans le JSON, encore encodé en base64

    static std::optional<GameManifest> fromJson(const QByteArray &json, QString *error);
    static std::optional<GameManifest> fromLocalFile(const QString &path);

    bool saveToLocalFile(const QString &path) const;

    // Octets exacts qui doivent être/ont été signés côté serveur : version + exe +
    // liste des fichiers triée par chemin, sérialisés de façon déterministe.
    // Le changelog N'EST PAS inclus dans la signature (il peut être édité a posteriori
    // sans invalider les fichiers déjà publiés).
    QByteArray canonicalPayload() const;

    QHash<QString, ManifestFile> filesByPath() const;
};

// Résultat de la comparaison entre le manifest local (déjà installé) et celui du serveur.
struct UpdateDiff {
    QVector<ManifestFile> toDownload; // fichiers nouveaux ou dont le hash a changé
    QStringList toDelete;             // fichiers présents localement mais plus dans le manifest distant
    qint64 totalBytes = 0;

    bool isEmpty() const { return toDownload.isEmpty() && toDelete.isEmpty(); }
};

UpdateDiff computeDiff(const GameManifest &local, const GameManifest &remote);
