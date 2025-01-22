#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QTemporaryFile>
#include <QMessageBox>
#include <stdexcept>

#include <QTemporaryDir>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    networkManager(new QNetworkAccessManager(this)),
    currentVersion("0.1.1"),
    applicationPath("C:/Programmation/test/sans_titre.exe")  // path to the app
{
    ui->setupUi(this);
    ui->centralWidget->setStyleSheet(
        "QWidget#centralWidget {"
        "   border: none;"
        "}"
        );
    ui->statusLabel->setStyleSheet("QLabel { color: white; }");
    ui->progressBar->setStyleSheet("QProgressBar { color: white; }");
    ui->launchButton->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(0, 0, 0, 100);"
        "   color: white;"
        "   border-radius: 15px;"
        "   padding: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(255, 255, 255, 50);"
        "}"
        );
    QPixmap currentPixmap = ui->wallpapperLabel->pixmap();
    QPixmap newPixmap = currentPixmap.scaled(300, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->wallpapperLabel->setPixmap(newPixmap);

    // simple user interface configuration
    ui->statusLabel->setText("Vérification des mises à jour...");
    ui->launchButton->setEnabled(false);

    // check if update is availlable
    checkForUpdates();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::checkForUpdates()
{
    QNetworkRequest request(QUrl("http://127.0.0.1/updater/serveur/version.txt")); // Server Version URL
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUpdateCheck(reply);
    });
}

void MainWindow::handleUpdateCheck(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        latestVersion = QString(reply->readAll()).trimmed();

        if (isUpdateAvailable() ) {
            ui->statusLabel->setText("Mise à jour disponible !");
            downloadUpdate();
        } else {
            ui->progressBar->setValue(100);
            ui->statusLabel->setText("Application à jour");
            ui->launchButton->setEnabled(true);
        }
    } else {
        ui->statusLabel->setText("Erreur de vérification des mises à jour");
        ui->launchButton->setEnabled(true);
    }

    reply->deleteLater();
}

bool MainWindow::isUpdateAvailable()
{
    // simple compar -need to upgrade
    return latestVersion != currentVersion;
}

void MainWindow::downloadUpdate()
{
    QNetworkRequest request(QUrl("http://127.0.0.1/updater/serveur/sans_titre.exe"));
    QNetworkReply *reply = networkManager->get(request);

    // Connect signal for progression
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            int progress = (bytesReceived * 100) / bytesTotal;
            ui->progressBar->setValue(progress);
            ui->statusLabel->setText(QString("Téléchargement: %1%").arg(progress));
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleDownloadFinished(reply);
    });
}

void MainWindow::handleDownloadFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray updateData = reply->readAll();
        ui->progressBar->setValue(0);
        ui->statusLabel->setText("Installation en cours...");
        installUpdate(updateData);
        currentVersion = latestVersion;
        ui->progressBar->setValue(100);
        ui->statusLabel->setText("Mise à jour installée");
    } else {
        ui->progressBar->setValue(0);
        ui->statusLabel->setText("Erreur de téléchargement");
    }

    ui->launchButton->setEnabled(true);
    reply->deleteLater();
}
void MainWindow::installUpdate(const QByteArray &updateData)
{
    QString targetPath = "C:/Programmation/test/";
    QString targetExe = targetPath + "sans_titre.exe";

    ui->progressBar->setValue(25); // 25% - begin insatall process

    // verifi if app is start or not
    QProcess process;
    process.start("tasklist");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    if (output.contains("sans_titre.exe")) {
        ui->progressBar->setValue(0);
        QMessageBox::warning(this, "Attention", "Veuillez fermer l'application avant la mise à jour");
        return;
    }

    ui->progressBar->setValue(50); // 50% - end verification

    try {
        // Save old exe
        if (QFile::exists(targetExe)) {
            if (!QFile::rename(targetExe, targetExe + ".backup")) {
                throw std::runtime_error("Impossible de sauvegarder l'ancien exe");
            }
        }

        ui->progressBar->setValue(75); // 75% - save

        // install new exe
        QFile newExe(targetExe);
        if (!newExe.open(QIODevice::WriteOnly)) {
            throw std::runtime_error("Impossible d'écrire le nouvel exe");
        }
        newExe.write(updateData);
        newExe.close();

        // if it correctli install erase old exe
        if (QFile::exists(targetExe + ".backup")) {
            QFile::remove(targetExe + ".backup");
        }

        ui->progressBar->setValue(100); // 100% - install end
        ui->statusLabel->setText("Mise à jour installée avec succès");
    }
    catch (const std::exception &e) {
        // if problem, restor save
        ui->progressBar->setValue(0);
        if (QFile::exists(targetExe + ".backup")) {
            QFile::remove(targetExe);
            QFile::rename(targetExe + ".backup", targetExe);
        }
        ui->statusLabel->setText("Erreur lors de l'installation : " + QString(e.what()));
    }
}

void MainWindow::launchApplication()
{
    // Démarrer le processus de manière détachée
    bool success = QProcess::startDetached(applicationPath, QStringList());

    if (success) {
        // Attendre un petit moment pour s'assurer que l'application démarre
        QThread::msleep(500);
        this->close();
    } else {
        QMessageBox::critical(this, "Erreur", "Impossible de lancer l'application");
    }
}

void MainWindow::on_launchButton_clicked()
{
    launchApplication();

}

