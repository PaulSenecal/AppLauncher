#pragma once
#include <QObject>
#include <QVector>
#include "GameInfo.h"
#include "NetworkClient.h"

// Récupère la liste des jeux disponibles depuis catalog.json à la racine du
// serveur. Cette liste n'a pas besoin d'être signée (elle ne fait qu'annoncer
// des chemins ; c'est le manifest.json de CHAQUE jeu qui est signé et fait foi
// pour l'intégrité des fichiers réellement installés).
//
// Format attendu :
// { "games": [
//     {"id": "stargate", "name": "Stargate", "path": "games/stargate/", "exe": "stargate", "icon": "..."},
//     {"id": "avalon",   "name": "Avalon",   "path": "games/avalon/",   "exe": "avalon"}
// ]}
class Catalog : public QObject {
    Q_OBJECT
public:
    explicit Catalog(NetworkClient *network, const QString &serverBaseUrl, QObject *parent = nullptr);
    void fetch();

signals:
    void fetched(QVector<GameInfo> games, const QString &error);

private:
    NetworkClient *m_network;
    QString m_serverBaseUrl;
};
