#include "MainWindow.h"
#include "ChangelogDialog.h"
#include "../core/PlatformUtils.h"
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QDir>
#include <QPainter>
#include <QIcon>
#include <QColor>
#include <QFont>
#include <QSize>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
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
static const char *kEmbeddedSecretBase64 =
    "REPLACE_ME_WITH_YOUR_HMAC_SECRET_BASE64";

static const char *kServerBaseUrl = "http://localhost:8888/updater/";

// --- Palette façon Battle.net ---------------------------------------------
static const char *kColorBg          = "#1A1E29";
static const char *kColorSidebar     = "#12141C";
static const char *kColorPanelBox    = "#2A2A34";
static const char *kColorAccentBlue  = "#0070B2";
static const char *kColorTextWhite   = "#FFFFFF";
static const char *kColorTextMuted   = "#9AA0AE";

static const int kSidebarWidth = 72;
static const int kContentWidth = 800;
static const int kContentHeight = 653;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_verifier(kEmbeddedSecretBase64),
      m_catalog(&m_network, kServerBaseUrl),
      m_serverBaseUrl(kServerBaseUrl)
{
    buildUi();

    connect(&m_catalog, &Catalog::fetched, this, &MainWindow::onCatalogFetched);
    connect(m_retryButton, &QPushButton::clicked, this, &MainWindow::onRetryClicked);
    connect(m_launchButton, &QPushButton::clicked, this, &MainWindow::onLaunchClicked);
    connect(m_forumButton, &QPushButton::clicked, this, &MainWindow::onForumClicked);
    connect(m_patchNotesButton, &QPushButton::clicked, this, &MainWindow::onPatchNotesClicked);

    if (!m_verifier.isValid())
        qWarning("MainWindow: secret HMAC absent ou invalide - "
                 "les mises à jour seront refusées tant que kEmbeddedSecretBase64 "
                 "n'est pas remplacée.");

    refreshConnectivityAndFetch();
}

// --- Construction de l'UI ---------------------------------------------------

void MainWindow::buildUi()
{
    setWindowTitle("Launcher");
    setFixedSize(kSidebarWidth + kContentWidth, kContentHeight);
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(kColorBg));

    auto *central = new QWidget(this);
    setCentralWidget(central);

    // --- Rail de gauche : un bouton par jeu, rempli dynamiquement dans
    //     onCatalogFetched() -----------------------------------------------
    m_sidebar = new QWidget(central);
    m_sidebar->setGeometry(0, 0, kSidebarWidth, kContentHeight);
    m_sidebar->setStyleSheet(QString("background-color: %1;").arg(kColorSidebar));
    m_sidebarGroup = new QButtonGroup(this);
    m_sidebarGroup->setExclusive(true);

    // --- Panneau principal : reproduit la disposition de l'ancienne UI
    //     (logo, gros visuel à droite, 3 encarts d'actu, PLAY en bas à
    //     gauche), décalée de kSidebarWidth vers la droite -----------------
    m_content = new QWidget(central);
    m_content->setGeometry(kSidebarWidth, 0, kContentWidth, kContentHeight);

    m_logoLabel = new QLabel(m_content);
    m_logoLabel->setGeometry(10, 20, 211, 81);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    loadPixmapOrHide(m_logoLabel, ":/news/resources/Rcs/logoVideoGames.png");

    m_forumButton = new QPushButton("Forum", m_content);
    m_forumButton->setGeometry(30, 120, 91, 20);
    m_forumButton->setFlat(true);
    m_forumButton->setCursor(Qt::PointingHandCursor);
    m_forumButton->setStyleSheet(QString(
        "QPushButton { color: %1; font-weight: bold; text-align: left; border: none; }"
        "QPushButton:disabled { color: #555; }").arg(kColorTextWhite));

    m_patchNotesButton = new QPushButton("Patch Notes", m_content);
    m_patchNotesButton->setGeometry(30, 160, 121, 20);
    m_patchNotesButton->setFlat(true);
    m_patchNotesButton->setCursor(Qt::PointingHandCursor);
    m_patchNotesButton->setStyleSheet(m_forumButton->styleSheet());

    m_mainNewsLabel = new QLabel(m_content);
    m_mainNewsLabel->setGeometry(260, 20, 381, 231);
    loadPixmapOrHide(m_mainNewsLabel, ":/news/resources/Rcs/mainNews.png");

    m_gameTitleLabel = new QLabel(m_content);
    m_gameTitleLabel->setGeometry(640, 30, 151, 211);
    m_gameTitleLabel->setWordWrap(true);
    m_gameTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_gameTitleLabel->setStyleSheet(QString(
        "border-radius: 5px; background-color: %1; color: %2; font-weight: bold; padding: 6px;")
        .arg(kColorPanelBox, kColorTextWhite));
    m_gameTitleLabel->setText("Sélectionne un jeu\ndans la liste à\ngauche");

    const int thumbX[3] = {260, 440, 620};
    const char *placeholders[3] = {
        ":/news/resources/Rcs/new1.png",
        ":/news/resources/Rcs/new2.png",
        ":/news/resources/Rcs/new3.png"
    };
    for (int i = 0; i < 3; ++i) {
        m_newsThumb[i] = new QLabel(m_content);
        m_newsThumb[i]->setGeometry(thumbX[i], 360, 151, 131);
        loadPixmapOrHide(m_newsThumb[i], placeholders[i]);

        m_newsText[i] = new QLabel(m_content);
        m_newsText[i]->setGeometry(thumbX[i], 490, 151, 81);
        m_newsText[i]->setWordWrap(true);
        m_newsText[i]->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_newsText[i]->setStyleSheet(QString(
            "border-radius: 5px; background-color: %1; color: %2; padding: 6px;")
            .arg(kColorPanelBox, kColorTextWhite));
        m_newsText[i]->setText(QString());
    }

    // --- Bas gauche : version / PLAY / progression / statut ---------------
    m_versionLabel = new QLabel(m_content);
    m_versionLabel->setGeometry(30, 520, 174, 20);
    m_versionLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(kColorTextMuted));
    m_versionLabel->setText("Version : -");

    m_launchButton = new QPushButton("JOUER", m_content);
    m_launchButton->setGeometry(30, 550, 174, 48);
    m_launchButton->setEnabled(false);
    m_launchButton->setCursor(Qt::PointingHandCursor);
    QFont playFont = m_launchButton->font();
    playFont.setPointSize(16);
    playFont.setBold(true);
    m_launchButton->setFont(playFont);
    m_launchButton->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 4px; }"
        "QPushButton:disabled { background-color: #3A3F4B; color: #888; }"
        "QPushButton:hover:!disabled { background-color: #0086D6; }")
        .arg(kColorAccentBlue));

    m_progressBar = new QProgressBar(m_content);
    m_progressBar->setGeometry(30, 604, 174, 20);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(QString(
        "QProgressBar { background-color: #12141C; border-radius: 3px; }"
        "QProgressBar::chunk { background-color: %1; border-radius: 3px; }")
        .arg(kColorAccentBlue));

    m_statusLabel = new QLabel(m_content);
    m_statusLabel->setGeometry(30, 630, 300, 20);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(kColorTextMuted));
    m_statusLabel->setText("Chargement du catalogue...");

    m_retryButton = new QPushButton("Réessayer", m_content);
    m_retryButton->setGeometry(210, 550, 90, 30);
    m_retryButton->setVisible(false);
    m_retryButton->setStyleSheet(
        "QPushButton { background-color: #3A3F4B; color: white; border-radius: 4px; }"
        "QPushButton:hover { background-color: #4A505E; }");
}

// Charge un pixmap embarqué (qrc) sans planter si la ressource est absente -
// utile tant que tu n'as pas encore recopié tes visuels dans resources/Rcs/.
void MainWindow::loadPixmapOrHide(QLabel *label, const QString &resourcePath)
{
    const QPixmap pix(resourcePath);
    if (pix.isNull()) {
        label->hide();
        return;
    }
    label->setPixmap(pix);
    label->setScaledContents(true);
}

QToolButton *MainWindow::makeSidebarButton(const GameInfo &game, int index)
{
    auto *btn = new QToolButton(m_sidebar);
    btn->setGeometry(12, 20 + index * 60, 48, 48);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);

    QPixmap pix(game.iconUrl);
    if (pix.isNull()) {
        // Pas d'icône dispo -> avatar généré à partir des initiales, pour ne
        // jamais afficher un bouton vide.
        pix = QPixmap(48, 48);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(kColorAccentBlue));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(pix.rect(), 8, 8);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSize(16);
        p.setFont(f);
        const QString initials = game.name.isEmpty()
            ? "?" : game.name.left(2).toUpper();
        p.drawText(pix.rect(), Qt::AlignCenter, initials);
    }
    btn->setIcon(QIcon(pix));
    btn->setIconSize(QSize(48, 48));
    btn->setStyleSheet(
        "QToolButton { border: none; border-radius: 8px; }"
        "QToolButton:checked { border: 2px solid white; }");
    btn->setToolTip(game.name);

    m_sidebarGroup->addButton(btn, index);
    return btn;
}

// --- Réseau / cycle de vie ---------------------------------------------------

void MainWindow::refreshConnectivityAndFetch()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        const auto reach = QNetworkInformation::instance()->reachability();
        if (reach != QNetworkInformation::Reachability::Online
            && reach != QNetworkInformation::Reachability::Unknown) {
            setBusy(false, "Pas de connexion internet - jeux déjà installés disponibles hors-ligne");
            m_retryButton->setVisible(true);
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

    // Reconstruit le rail de jeux à chaque fetch (simple et suffisant - le
    // catalogue ne change pas en cours de session dans l'usage normal).
    for (QAbstractButton *b : m_sidebarGroup->buttons()) {
        m_sidebarGroup->removeButton(b);
        b->deleteLater();
    }
    for (int i = 0; i < games.size(); ++i)
        makeSidebarButton(games[i], i)->show();

    connect(m_sidebarGroup, &QButtonGroup::idClicked, this, &MainWindow::onGameButtonClicked);

    m_statusLabel->setText(games.isEmpty() ? "Aucun jeu disponible" : "Sélectionne un jeu");

    if (!games.isEmpty()) {
        m_sidebarGroup->button(0)->setChecked(true);
        selectGame(0);
    }
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

        m_versionLabel->setText("Version : "
            + (mgr->game().isInstalled() ? mgr->game().localVersion : "aucune"));

        if (!updateAvailable) {
            m_statusLabel->setText("À jour");
            m_launchButton->setEnabled(mgr->game().isInstalled());
            updateNewsPanel(mgr);
            return;
        }

        updateNewsPanel(mgr);
        ChangelogDialog dlg(mgr->game().name, mgr->remoteManifest().version,
                             mgr->remoteManifest().changelog, this);
        if (dlg.exec() == QDialog::Accepted) {
            setBusy(true, "Téléchargement...");
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
        m_statusLabel->setText(QString("%1 - %2 / %3 Mo (%4%)")
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
            m_versionLabel->setText("Version : " + mgr->game().localVersion);
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

void MainWindow::onGameButtonClicked(int index)
{
    selectGame(index);
}

void MainWindow::selectGame(int index)
{
    if (index < 0 || index >= m_catalogGames.size()) return;

    const GameInfo &game = m_catalogGames[index];
    m_activeManager = managerFor(game);

    m_gameTitleLabel->setText(game.name);
    m_forumButton->setEnabled(!game.forumUrl.isEmpty());
    m_patchNotesButton->setEnabled(!game.patchNotesUrl.isEmpty());
    for (int i = 0; i < 3; ++i) m_newsText[i]->setText(QString());

    startCheck(m_activeManager);
}

void MainWindow::updateNewsPanel(GameManager *mgr)
{
    // Réutilise les 3 encarts d'actu de l'ancienne UI pour afficher le
    // changelog du manifest distant, plutôt que du contenu statique.
    const QStringList &changelog = mgr->remoteManifest().changelog;
    for (int i = 0; i < 3; ++i)
        m_newsText[i]->setText(i < changelog.size() ? changelog[i] : QString());
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

void MainWindow::onForumClicked()
{
    if (m_activeManager && !m_activeManager->game().forumUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_activeManager->game().forumUrl));
}

void MainWindow::onPatchNotesClicked()
{
    if (!m_activeManager) return;
    const GameInfo &game = m_activeManager->game();
    if (!game.patchNotesUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(game.patchNotesUrl));
    } else {
        ChangelogDialog dlg(game.name, m_activeManager->remoteManifest().version,
                             m_activeManager->remoteManifest().changelog, this);
        dlg.exec();
    }
}

void MainWindow::setBusy(bool busy, const QString &statusText)
{
    if (!statusText.isEmpty()) m_statusLabel->setText(statusText);
    m_progressBar->setRange(0, busy ? 0 : 100); // indéterminé pendant l'attente réseau
}
