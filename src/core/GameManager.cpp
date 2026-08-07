#include "GameManager.h"
#include "PlatformUtils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStorageInfo>
#include <QNetworkReply>
#include <QDebug>
#include <utility>

GameManager::GameManager(GameInfo game, NetworkClient *network,
                         ManifestAuthenticator *verifier, const QString &serverBaseUrl,
                         QObject *parent)
    : QObject(parent), m_game(std::move(game)), m_network(network),
    m_verifier(verifier), m_serverBaseUrl(serverBaseUrl)
{
    m_game.installDir = PlatformUtils::gamesRootDir() + "/" + m_game.id;
    QDir().mkpath(m_game.installDir);

    const QString localManifestPath = m_game.installDir + "/manifest.json";
    if (auto local = GameManifest::fromLocalFile(localManifestPath)) {
        m_localManifest = *local;
        m_game.localVersion = m_localManifest.version;
    }
}

QString GameManager::stagingDir() const { return m_game.installDir + "/.staging"; }
QString GameManager::lockFilePath() const { return m_game.installDir + "/.running.lock"; }

// --- 1) Vérification de version -------------------------------------------------

void GameManager::checkForUpdate()
{
    const QUrl url(m_serverBaseUrl + m_game.serverPath + "manifest.json");
    QNetworkReply *reply = m_network->get(url);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const auto errKind = NetworkClient::classify(reply);
        if (errKind != NetworkClient::ErrorKind::None) {
            emit checkFinished(false, NetworkClient::errorMessage(errKind));
            return;
        }

        QString parseError;
        auto manifest = GameManifest::fromJson(reply->readAll(), &parseError);
        if (!manifest) {
            emit checkFinished(false, "Manifest distant invalide: " + parseError);
            return;
        }

        // Vérification de signature AVANT toute autre décision : un manifest non
        // signé (ou signé par une mauvaise clé) est traité comme une erreur réseau,
        // jamais comme "pas de mise à jour".
        if (!m_verifier->isValid()
            || !m_verifier->verify(manifest->canonicalPayload(), manifest->signatureB64)) {
            emit checkFinished(false, "Signature du manifest invalide - mise à jour ignorée par sécurité");
            return;
        }

        m_remoteManifest = *manifest;
        m_diff = computeDiff(m_localManifest, m_remoteManifest);

        const bool updateAvailable = !m_diff.isEmpty()
                                     && m_remoteManifest.version != m_game.localVersion;
        emit checkFinished(updateAvailable, QString());
    });
}

// --- 2) Application de la mise à jour -------------------------------------------

void GameManager::applyUpdate()
{
    if (isRunning()) {
        emit gameRunningWarning();
        return;
    }

    // Vérifie l'espace disque disponible avant de commencer (fichiers modifiés
    // à télécharger + on garde temporairement les anciens en place jusqu'au swap).
    QStorageInfo storage(m_game.installDir);
    if (storage.isValid() && storage.bytesAvailable() < m_diff.totalBytes * 2) {
        emit updateFinished(false, "Espace disque insuffisant");
        return;
    }

    QDir().mkpath(stagingDir());
    m_bytesReceivedTotal = 0;
    m_updateFailed = false;
    m_cancelled = false;
    m_downloadQueue.clear();
    for (const ManifestFile &f : std::as_const(m_diff.toDownload))
        m_downloadQueue.enqueue(f);
    m_activeDownloads = 0;

    if (m_downloadQueue.isEmpty()) {
        applyStagedFiles(); // uniquement des suppressions, pas de téléchargement
        return;
    }

    for (int i = 0; i < kMaxConcurrentDownloads && !m_downloadQueue.isEmpty(); ++i)
        downloadNextFile();
}

void GameManager::cancelUpdate()
{
    m_cancelled = true;
    QDir(stagingDir()).removeRecursively();
}

void GameManager::downloadNextFile()
{
    if (m_downloadQueue.isEmpty() || m_cancelled) return;

    const ManifestFile file = m_downloadQueue.dequeue();
    ++m_activeDownloads;

    const QUrl url(m_serverBaseUrl + m_game.serverPath + file.path);
    QNetworkReply *reply = m_network->get(url, 30000); // fichiers volumineux -> timeout plus large

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, file](qint64 received, qint64 total) {
                Q_UNUSED(total);
                emit progressChanged(m_bytesReceivedTotal + received, m_diff.totalBytes, file.path);
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply, file]() {
        reply->deleteLater();
        --m_activeDownloads;

        if (m_cancelled) return;

        const auto errKind = NetworkClient::classify(reply);
        if (errKind != NetworkClient::ErrorKind::None) {
            m_updateFailed = true;
            emit updateFinished(false, NetworkClient::errorMessage(errKind) + " (" + file.path + ")");
            return;
        }

        onFileDownloaded(reply->readAll(), file);

        if (m_updateFailed || m_cancelled) return;

        if (!m_downloadQueue.isEmpty()) {
            downloadNextFile();
        } else if (m_activeDownloads == 0) {
            applyStagedFiles();
        }
    });
}

void GameManager::onFileDownloaded(const QByteArray &data, const ManifestFile &expected)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(data);
    if (hash.result().toHex() != expected.sha256.toUtf8()) {
        m_updateFailed = true;
        emit updateFinished(false, "Fichier corrompu après téléchargement: " + expected.path);
        return;
    }

    const QString stagedPath = stagingDir() + "/" + expected.path;
    QDir().mkpath(QFileInfo(stagedPath).absolutePath());

    QFile out(stagedPath);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
        m_updateFailed = true;
        emit updateFinished(false, "Écriture impossible: " + expected.path);
        return;
    }
    out.close();
    m_bytesReceivedTotal += data.size();
}

bool GameManager::applyStagedFiles()
{
    // Bascule atomique : on déplace fichier par fichier depuis le staging vers
    // l'emplacement final (le rename est atomique sur un même volume), puis on
    // supprime ce qui n'existe plus côté serveur, et enfin on écrit le nouveau
    // manifest local. Si une étape échoue au milieu, le pire cas est un fichier
    // à jour + un vieux fichier non supprimé - jamais un exe à moitié écrit.
    for (const ManifestFile &f : m_diff.toDownload) {
        const QString from = stagingDir() + "/" + f.path;
        const QString to = m_game.installDir + "/" + f.path;
        QDir().mkpath(QFileInfo(to).absolutePath());
        QFile::remove(to); // QFile::rename échoue si la destination existe déjà
        if (!QFile::rename(from, to)) {
            emit updateFinished(false, "Impossible de finaliser: " + f.path);
            return false;
        }
        PlatformUtils::ensureExecutable(to);
    }

    for (const QString &staleFile : m_diff.toDelete)
        QFile::remove(m_game.installDir + "/" + staleFile);

    QDir(stagingDir()).removeRecursively();

    m_remoteManifest.saveToLocalFile(m_game.installDir + "/manifest.json");
    m_localManifest = m_remoteManifest;
    m_game.localVersion = m_remoteManifest.version;
    m_game.exeBaseName = m_remoteManifest.exeBaseName;

    emit updateFinished(true, QString());
    return true;
}

// --- Détection "jeu en cours" ----------------------------------------------------

bool GameManager::isRunning() const
{
    QLockFile probe(lockFilePath());
    probe.setStaleLockTime(0); // on gère la péremption nous-mêmes si besoin
    if (probe.tryLock(0)) {
        probe.unlock();
        return false; // on a réussi à prendre le verrou -> personne ne le tenait
    }
    return true;
}

bool GameManager::launch()
{
    if (!m_game.isInstalled()) return false;

    const QString exePath = m_game.installDir + "/"
                            + PlatformUtils::executableName(m_game.exeBaseName);
    if (!QFile::exists(exePath)) return false;

    m_runLock.reset(new QLockFile(lockFilePath()));
    if (!m_runLock->tryLock(0)) {
        emit gameRunningWarning();
        return false;
    }

    m_runningProcess = new QProcess(this);
    m_runningProcess->setProgram(exePath);
    m_runningProcess->setWorkingDirectory(m_game.installDir);

    connect(m_runningProcess, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (m_runLock) m_runLock->unlock();
    });

    m_runningProcess->start();
    return m_runningProcess->waitForStarted(3000);
}