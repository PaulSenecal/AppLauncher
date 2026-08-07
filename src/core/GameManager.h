#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QLockFile>
#include <QQueue>
#include "GameInfo.h"
#include "GameManifest.h"
#include "NetworkClient.h"
#include "ManifestAuthenticator.h"

// Orchestre tout le cycle de vie d'UN jeu : vérification de version, calcul du
// diff, téléchargement (parallélisé, en staging), vérification hash + signature,
// bascule atomique, et lancement.
//
// Une instance de GameManager par jeu sélectionnable dans le launcher ; elles
// partagent le même NetworkClient / ManifestAuthenticator (passés par référence)
// pour éviter de dupliquer les connexions et le parsing de clé.
class GameManager : public QObject {
    Q_OBJECT
public:
    GameManager(GameInfo game, NetworkClient *network,
                ManifestAuthenticator *verifier, const QString &serverBaseUrl,
                QObject *parent = nullptr);

    const GameInfo &game() const { return m_game; }
    const GameManifest &remoteManifest() const { return m_remoteManifest; }

    // 1) Récupère manifest.json distant, vérifie sa signature, compare à la
    //    version locale. Émet checkFinished().
    void checkForUpdate();

    // 2) Télécharge et applique le diff calculé lors du check. Émet progressChanged
    //    en continu puis updateFinished() à la fin (succès ou échec).
    void applyUpdate();

    void cancelUpdate();

    // Détecte si le jeu est en cours d'exécution (verrou posé par launch()).
    bool isRunning() const;

    // Lance l'exécutable local. Retourne false si le jeu n'est pas installé.
    bool launch();

signals:
    void checkFinished(bool updateAvailable, const QString &error);
    void progressChanged(qint64 bytesReceived, qint64 bytesTotal, const QString &currentFile);
    void updateFinished(bool success, const QString &error);
    void gameRunningWarning();

private:
    void downloadNextFile();
    void onFileDownloaded(const QByteArray &data, const ManifestFile &expected);
    bool applyStagedFiles();
    QString stagingDir() const;
    QString lockFilePath() const;

    GameInfo m_game;
    NetworkClient *m_network;          // non possédé
    ManifestAuthenticator *m_verifier;  // non possédé
    QString m_serverBaseUrl;           // ex: "https://updates.example.com/"

    GameManifest m_remoteManifest;
    GameManifest m_localManifest;
    UpdateDiff m_diff;

    QQueue<ManifestFile> m_downloadQueue;
    int m_activeDownloads = 0;
    static constexpr int kMaxConcurrentDownloads = 3;
    qint64 m_bytesReceivedTotal = 0;
    bool m_updateFailed = false;
    bool m_cancelled = false;

    QScopedPointer<QLockFile> m_runLock;
    QPointer<QProcess> m_runningProcess;
};
