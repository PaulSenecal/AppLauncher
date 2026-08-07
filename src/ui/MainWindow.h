#pragma once
#include <QMainWindow>
#include <QHash>
#include "../core/GameInfo.h"
#include "../core/GameManager.h"
#include "../core/NetworkClient.h"
#include "../core/ManifestAuthenticator.h"
#include "../core/Catalog.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class QProgressBar;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onCatalogFetched(QVector<GameInfo> games, const QString &error);
    void onGameSelected(QListWidgetItem *item);
    void onRetryClicked();
    void onLaunchClicked();

private:
    void buildUi();
    void refreshConnectivityAndFetch();
    GameManager *managerFor(const GameInfo &game);
    void startCheck(GameManager *mgr);
    void setBusy(bool busy, const QString &statusText);

    QListWidget *m_gameList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_launchButton = nullptr;
    QPushButton *m_retryButton = nullptr;

    NetworkClient m_network;
    ManifestAuthenticator m_verifier;
    Catalog m_catalog;
    QString m_serverBaseUrl;

    QHash<QString, GameManager *> m_managers; // clé = GameInfo::id
    QVector<GameInfo> m_catalogGames;
    GameManager *m_activeManager = nullptr;
};
