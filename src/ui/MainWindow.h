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
class QToolButton;
class QButtonGroup;

// UI façon "launcher Battle.net" : rail d'icônes de jeux à gauche, gros panneau
// à droite pour le jeu sélectionné (visuel, changelog en encarts, PLAY/progress
// en bas à gauche du panneau). Toute la logique métier reste dans GameManager -
// cette classe ne fait qu'orchestrer l'affichage.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onCatalogFetched(QVector<GameInfo> games, const QString &error);
    void onGameButtonClicked(int index);
    void onRetryClicked();
    void onLaunchClicked();
    void onForumClicked();
    void onPatchNotesClicked();

private:
    void buildUi();
    void refreshConnectivityAndFetch();
    GameManager *managerFor(const GameInfo &game);
    void startCheck(GameManager *mgr);
    void setBusy(bool busy, const QString &statusText);
    void selectGame(int index);
    void updateNewsPanel(GameManager *mgr); // remplit les encarts avec le changelog
    static void loadPixmapOrHide(QLabel *label, const QString &resourcePath);
    QToolButton *makeSidebarButton(const GameInfo &game, int index);

    // --- rail de gauche ---
    QWidget *m_sidebar = nullptr;
    QButtonGroup *m_sidebarGroup = nullptr;

    // --- panneau principal (contenu du jeu sélectionné) ---
    QWidget *m_content = nullptr;
    QLabel *m_logoLabel = nullptr;
    QLabel *m_mainNewsLabel = nullptr;
    QLabel *m_gameTitleLabel = nullptr;
    QPushButton *m_forumButton = nullptr;
    QPushButton *m_patchNotesButton = nullptr;
    QLabel *m_newsThumb[3] = {nullptr, nullptr, nullptr};
    QLabel *m_newsText[3] = {nullptr, nullptr, nullptr};

    QLabel *m_versionLabel = nullptr;
    QPushButton *m_launchButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_retryButton = nullptr;

    NetworkClient m_network;
    ManifestAuthenticator m_verifier;
    Catalog m_catalog;
    QString m_serverBaseUrl;

    QHash<QString, GameManager *> m_managers; // clé = GameInfo::id
    QVector<GameInfo> m_catalogGames;
    GameManager *m_activeManager = nullptr;
};
