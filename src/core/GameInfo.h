#pragma once
#include <QString>

// Décrit un jeu géré par le launcher : identité + chemins locaux.
// Une instance est créée pour chaque entrée renvoyée par le catalogue serveur (catalog.json).
struct GameInfo {
    QString id;              // identifiant unique stable, ex: "stargate"
    QString name;             // nom affiché, ex: "Stargate"
    QString serverPath;       // chemin relatif sur le serveur, ex: "games/stargate/"
    QString iconUrl;          // icône à afficher dans la liste (optionnel)
    QString forumUrl;         // lien "Forum" (optionnel)
    QString patchNotesUrl;    // lien "Patch Notes" externe (optionnel, sinon on montre le changelog du manifest)

    // Nom de l'exécutable SANS extension - l'extension est résolue par plateforme
    // (voir PlatformUtils::executableName)
    QString exeBaseName;

    // Résolu au runtime, jamais lu depuis le JSON :
    QString installDir;       // dossier local d'installation de CE jeu
    QString localVersion;     // version actuellement installée ("" si non installé)

    bool isInstalled() const { return !localVersion.isEmpty(); }
};
