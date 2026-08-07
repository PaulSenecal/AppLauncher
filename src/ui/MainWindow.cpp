#include "MainWindow.h"
#include "ChangelogDialog.h"
#include "../core/PlatformUtils.h"
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QDir>
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
#include <QNetworkInformation>
#endif

// ATTENTION : remplace cette valeur par TON propre secret généré avec
// tools/keygen.py avant de builder en production. Celui-ci n'est qu'un
// exemple et ne doit jamais être utilisé tel quel.
//
// Contrairement à une clé publique, CE SECRET NE DOIT PAS FUITER - c'est lui
// qui prouve qu'un manifest vient bien de toi (voir ManifestAuthenticator.h
// pour le compromis de sécurité que ça implique).
//static const char *kEmbeddedSecretBase64 =
    //"REPLACE_ME_WITH_YOUR_HMAC_SECRET_BASE64";

//static const char *kServerBaseUrl = "https://updates.example.com/";

static const char *kEmbeddedSecretBase64 = "w6Iq5aFILxj0xcdNTIItFEg4aaGePi18HZmy5EJJFzw=";
static const char *kServerBaseUrl = "http://localhost:8888/updater/";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_verifier(kEmbeddedSecretBase64),
      m_catalog(&m_network, kServerBaseUrl),
      m_serverBaseUrl(kServerBaseUrl)
{
    buildUi();

    connect(&m_catalog, &Catalog::fetched, this, &MainWindow::onCatalogFetched);
    connect(m_gameList, &QListWidget::itemClicked, this, &MainWindow::onGameSelected);
    connect(m_retryButton, &QPushButton::clicked, this, &MainWindow::onRetryClicked);
    connect(m_launchButton, &QPushButton::clicked, this, &MainWindow::onLaunchClicked);

    if (!m_verifier.isValid())
        qWarning("MainWindow: secret HMAC absent ou invalide - "
                 "les mises à jour seront refusées tant que kEmbeddedSecretBase64 "
                 "n'est pas remplacée.");

    refreshConnectivityAndFetch();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(central);

    m_gameList = new QListWidget(central);
    m_gameList->setMinimumWidth(220);
    mainLayout->addWidget(m_gameList);

    auto *rightPanel = new QVBoxLayout();
    m_statusLabel = new QLabel("Chargement du catalogue...", central);
    m_versionLabel = new QLabel(QString(), central);
    m_progressBar = new QProgressBar(central);
    m_progressBar->setRange(0, 0); // indéterminé tant qu'on n'a pas de vrai pourcentage

    auto *buttonsRow = new QHBoxLayout();
    m_retryButton = new QPushButton("Réessayer", central);
    m_retryButton->setVisible(false);
    m_launchButton = new QPushButton("Lancer", central);
    m_launchButton->setEnabled(false);
    buttonsRow->addWidget(m_retryButton);
    buttonsRow->addWidget(m_launchButton);

    rightPanel->addWidget(m_statusLabel);
    rightPanel->addWidget(m_versionLabel);
    rightPanel->addWidget(m_progressBar);
    rightPanel->addStretch();
    rightPanel->addLayout(buttonsRow);

    mainLayout->addLayout(rightPanel);
    setCentralWidget(central);
    resize(640, 380);
    setWindowTitle("Launcher");
}

void MainWindow::refreshConnectivityAndFetch()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        const auto reach = QNetworkInformation::instance()->reachability();
        if (reach != QNetworkInformation::Reachability::Online
            && reach != QNetworkInformation::Reachability::Unknown) {
            setBusy(false, "Pas de connexion internet - jeux déjà installés disponibles hors-ligne");
            m_retryButton->setVisible(true);
            // On peuple quand même la liste avec ce qui est déjà installé localement.
            QVector<GameInfo> offline;
            for (const QString &dir : QDir(PlatformUtils::gamesRootDir())
                                           .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                GameInfo g;
                g.id = dir;
                g.name = dir;
                offline << g;
            }
            onCatalogFetched(offline, QString());
            return;
        }
    }
#endif
    setBusy(true, "Chargement du catalogue...");
    m_retryButton->setVisible(false);
    m_catalog.fetch();
}

void MainWindow::onRetryClicked()
{
    refreshConnectivityAndFetch();
}

void MainWindow::onCatalogFetched(QVector<GameInfo> games, const QString &error)
{
    setBusy(false, QString());
    if (!error.isEmpty() && games.isEmpty()) {
        m_statusLabel->setText("Impossible de charger le catalogue: " + error);
        m_retryButton->setVisible(true);
        return;
    }

    m_catalogGames = games;
    m_gameList->clear();
    for (const GameInfo &g : games) {
        auto *item = new QListWidgetItem(g.name.isEmpty() ? g.id : g.name, m_gameList);
        item->setData(Qt::UserRole, g.id);
    }
    m_statusLabel->setText(games.isEmpty() ? "Aucun jeu disponible" : "Sélectionne un jeu");
}

GameManager *MainWindow::managerFor(const GameInfo &game)
{
    if (auto *existing = m_managers.value(game.id, nullptr))
        return existing;

    auto *mgr = new GameManager(game, &m_network, &m_verifier, m_serverBaseUrl, this);
    connect(mgr, &GameManager::checkFinished, this, [this, mgr](bool updateAvailable, const QString &error) {
        if (mgr != m_activeManager) return;
        setBusy(false, QString());

        if (!error.isEmpty()) {
            const bool canLaunchOffline = mgr->game().isInstalled();
            m_statusLabel->setText(error + (canLaunchOffline ? " - version locale disponible" : ""));
            m_retryButton->setVisible(true);
            m_launchButton->setEnabled(canLaunchOffline);
            return;
        }

        m_versionLabel->setText("Version installée: "
            + (mgr->game().isInstalled() ? mgr->game().localVersion : "aucune"));

        if (!updateAvailable) {
            m_statusLabel->setText("À jour");
            m_launchButton->setEnabled(mgr->game().isInstalled());
            return;
        }

        ChangelogDialog dlg(mgr->game().name, mgr->remoteManifest().version,
                             mgr->remoteManifest().changelog, this);
        if (dlg.exec() == QDialog::Accepted) {
            setBusy(true, "Téléchargement...");
            m_progressBar->setRange(0, 100);
            mgr->applyUpdate();
        } else {
            m_statusLabel->setText("Mise à jour différée");
            m_launchButton->setEnabled(mgr->game().isInstalled());
        }
    });

    connect(mgr, &GameManager::progressChanged, this, [this, mgr](qint64 received, qint64 total, const QString &file) {
        if (mgr != m_activeManager || total <= 0) return;
        const int pct = static_cast<int>((received * 100) / total);
        m_progressBar->setValue(pct);
        m_statusLabel->setText(QString("Téléchargement de %1 - %2 / %3 Mo (%4%)")
            .arg(file)
            .arg(received / 1'000'000.0, 0, 'f', 1)
            .arg(total / 1'000'000.0, 0, 'f', 1)
            .arg(pct));
    });

    connect(mgr, &GameManager::updateFinished, this, [this, mgr](bool success, const QString &error) {
        if (mgr != m_activeManager) return;
        m_progressBar->setValue(success ? 100 : 0);
        if (success) {
            m_statusLabel->setText("Mise à jour installée");
            m_versionLabel->setText("Version installée: " + mgr->game().localVersion);
        } else {
            m_statusLabel->setText("Échec de la mise à jour: " + error);
            m_retryButton->setVisible(true);
        }
        m_launchButton->setEnabled(mgr->game().isInstalled());
    });

    connect(mgr, &GameManager::gameRunningWarning, this, [this]() {
        QMessageBox::warning(this, "Jeu en cours", "Ferme le jeu avant de continuer.");
    });

    m_managers.insert(game.id, mgr);
    return mgr;
}

void MainWindow::onGameSelected(QListWidgetItem *item)
{
    const QString id = item->data(Qt::UserRole).toString();
    const auto it = std::find_if(m_catalogGames.begin(), m_catalogGames.end(),
                                  [&](const GameInfo &g) { return g.id == id; });
    if (it == m_catalogGames.end()) return;

    m_activeManager = managerFor(*it);
    startCheck(m_activeManager);
}

void MainWindow::startCheck(GameManager *mgr)
{
    setBusy(true, "Vérification des mises à jour...");
    m_launchButton->setEnabled(false);
    m_retryButton->setVisible(false);
    mgr->checkForUpdate();
}

void MainWindow::onLaunchClicked()
{
    if (!m_activeManager) return;
    if (!m_activeManager->launch())
        QMessageBox::critical(this, "Erreur", "Lancement impossible.");
}

void MainWindow::setBusy(bool busy, const QString &statusText)
{
    if (!statusText.isEmpty()) m_statusLabel->setText(statusText);
    m_progressBar->setRange(0, busy ? 0 : 100); // indéterminé pendant l'attente réseau
}
