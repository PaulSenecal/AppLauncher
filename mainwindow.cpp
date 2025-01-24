#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    networkManager(new QNetworkAccessManager(this)),
    //currentVersion("0.0.1"),

    applicationPath(QDir::homePath() + TARGET_PATH + "/" + EXE_NAME)  // Chemin complet
{
    ui->setupUi(this);
    verifyAndCreateDirforApp();  // Créer le dossier dès le démarrage
    verifyLocalCurrentVersionFile();
    ui->statusLabel->setText("Vérification des mises à jour...");
    ui->launchButton->setEnabled(false);
    checkForUpdates();
}
MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::verifyLocalCurrentVersionFile()
{
    QFile localCurrentVersionFile("currentVersion.txt");

    if (!localCurrentVersionFile.exists()) {
        if (localCurrentVersionFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&localCurrentVersionFile);
            stream << "0.0.0";
            localCurrentVersionFile.close();
        } else {
            qDebug() << "Erreur lors de la création du fichier currentVersion.txt";
            return;
        }
    }

    if (localCurrentVersionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&localCurrentVersionFile);
        currentVersion = stream.readAll();
        localCurrentVersionFile.close();
    } else {
        qDebug() << "Erreur lors de la lecture du fichier currentVersion.txt";
    }
}
void MainWindow::verifyAndCreateDirforApp()
{
    QString appDirPath = QDir::homePath() + "/Application";
    QDir appDir(appDirPath);
    if (!appDir.exists()) {
        if (appDir.mkpath(appDirPath)) {
            qDebug() << "Dossier Application créé avec succès à:" << appDirPath;
        } else {
            qDebug() << "Erreur lors de la création du dossier Application à:" << appDirPath;
        }
    } else {
        qDebug() << "Le dossier Application existe déjà à:" << appDirPath;
    }
}
void MainWindow::changerLocalCurrentVersionFile(QString lastestVersion)
{
    QFile localCurrentVersionFile("currentVersion.txt");
        if (localCurrentVersionFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&localCurrentVersionFile);
            stream << lastestVersion;
            localCurrentVersionFile.close();
        } else {
            qDebug() << "Erreur lors de la création du fichier currentVersion.txt";
            return;
        }
}
void MainWindow::checkForUpdates()
{
    QNetworkRequest request(QUrl(SERVER_URL + "version.txt"));
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUpdateCheck(reply);
    });
}

void MainWindow::handleUpdateCheck(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        latestVersion = QString(reply->readAll()).trimmed();
        if (QVersionNumber::fromString(latestVersion) > QVersionNumber::fromString(currentVersion)) {
            ui->statusLabel->setText("Mise à jour disponible !");
            downloadUpdate();
        } else {
            ui->progressBar->setValue(100);
            ui->versionLabel->setText("Version: " + currentVersion);
            ui->statusLabel->setText("Application à jour");
            ui->launchButton->setEnabled(true);
        }
    } else {
        ui->statusLabel->setText("Erreur de vérification");
        ui->launchButton->setEnabled(true);
    }
    reply->deleteLater();
}

void MainWindow::downloadUpdate()
{
    QNetworkRequest request(QUrl(SERVER_URL + "checksum.json"));
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            downloadExeAndVerify(doc.object()[EXE_NAME].toString());
        }
        reply->deleteLater();
    });
}

void MainWindow::downloadExeAndVerify(const QString &expectedHash)
{
    QNetworkRequest request(QUrl(SERVER_URL + EXE_NAME));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &MainWindow::handleDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, expectedHash]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray updateData = reply->readAll();
            if (verifyFileIntegrity(updateData, expectedHash)) {
                installUpdate(updateData);
            } else {
                ui->statusLabel->setText("Erreur : Fichier corrompu");
            }
        }
        reply->deleteLater();
    });
}

void MainWindow::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int progress = (bytesReceived * 100) / bytesTotal;
        ui->progressBar->setValue(progress);
        ui->statusLabel->setText(QString("Téléchargement: %1%").arg(progress));
    }
}

QString MainWindow::calculateDataHash(const QByteArray &data)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(data);
    return hash.result().toHex();
}

bool MainWindow::verifyFileIntegrity(const QByteArray &updateData, const QString &expectedHash)
{
    // Si l'application n'existe pas, on accepte la première installation
    if (!QFile::exists(applicationPath)) {
        return true;
    }
    return calculateDataHash(updateData) == expectedHash;
}

bool MainWindow::verifyRunningApplication()
{
    QProcess process;
    process.start("tasklist");
    process.waitForFinished();
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    return output.contains(EXE_NAME);
}

void MainWindow::backupExe()
{
    // Ne faire la sauvegarde que si le fichier existe
    if (QFile::exists(applicationPath)) {
        if (!QFile::rename(applicationPath, applicationPath + ".backup")) {
            throw std::runtime_error("Impossible de sauvegarder l'ancien exe");
        }
    }
}

void MainWindow::restoreBackup()
{
    if (QFile::exists(applicationPath + ".backup")) {
        QFile::remove(applicationPath);
        QFile::rename(applicationPath + ".backup", applicationPath);
    }
}

bool MainWindow::writeNewExe(const QByteArray &updateData)
{
    QString dirPath = QDir::homePath() + TARGET_PATH;
    QDir dir(dirPath);

    // Vérifier/créer le dossier
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Erreur création dossier:" << dirPath;
            return false;
        }
    }

    // Vérifier les permissions
    QFileInfo dirInfo(dirPath);
    if (!dirInfo.isWritable()) {
        qDebug() << "Dossier non accessible en écriture:" << dirPath;
        return false;
    }

    QFile newExe(applicationPath);
    if (!newExe.open(QIODevice::WriteOnly)) {
        qDebug() << "Erreur ouverture fichier:" << newExe.errorString();
        return false;
    }

    if (newExe.write(updateData) == -1) {
        qDebug() << "Erreur écriture fichier:" << newExe.errorString();
        newExe.close();
        return false;
    }

    newExe.close();
    return true;
}

void MainWindow::installUpdate(const QByteArray &updateData)
{
    if (updateData.isEmpty()) {
        ui->statusLabel->setText("Erreur: données vides");
        return;
    }

    ui->progressBar->setValue(25);

    // Vérifier l'application en cours d'exécution seulement si elle existe
    if (QFile::exists(applicationPath) && verifyRunningApplication()) {
        ui->progressBar->setValue(0);
        QMessageBox::warning(this, "Attention", "Veuillez fermer l'application");
        return;
    }

    try {
        ui->progressBar->setValue(50);
        backupExe();  // La sauvegarde ne se fera que si le fichier existe

        ui->progressBar->setValue(75);
        if (!writeNewExe(updateData)) {
            throw std::runtime_error("Erreur d'écriture");
        }

        // Supprimer la sauvegarde seulement si elle existe
        if (QFile::exists(applicationPath + ".backup")) {
            QFile::remove(applicationPath + ".backup");
        }

        ui->progressBar->setValue(100);
        ui->statusLabel->setText(QFile::exists(applicationPath) ? "Mise à jour installée" : "Installation terminée");
        ui->launchButton->setEnabled(true);
        ui->versionLabel->setText("Version: " + latestVersion);
        currentVersion = latestVersion;
        changerLocalCurrentVersionFile(currentVersion);
    }
    catch (const std::exception &e) {
        ui->progressBar->setValue(0);
        restoreBackup();
        ui->statusLabel->setText("Erreur: " + QString(e.what()));
    }
}

void MainWindow::launchApplication()
{
    if (QProcess::startDetached(applicationPath)) {
        QThread::msleep(500);
        close();
    } else {
        QMessageBox::critical(this, "Erreur", "Lancement impossible");
    }
}

void MainWindow::on_launchButton_clicked()
{
    launchApplication();
}
