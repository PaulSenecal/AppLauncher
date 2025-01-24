#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QTemporaryFile>
#include <QMessageBox>
#include <stdexcept>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <qfile.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void checkForUpdates();
    void handleUpdateCheck(QNetworkReply *reply);
    void downloadUpdate();
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void installUpdate(const QByteArray &updateData);
    void launchApplication();
    void on_launchButton_clicked();
private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QString currentVersion;
    QString latestVersion;
    QString applicationPath;

    bool verifyRunningApplication();
    QString calculateDataHash(const QByteArray &data);
    bool verifyFileIntegrity(const QByteArray &updateData, const QString &expectedHash);
    void downloadExeAndVerify(const QString &expectedHash);
    void backupExe();
    void restoreBackup();
    bool writeNewExe(const QByteArray &updateData);
    void verifyLocalCurrentVersionFile();
    void changerLocalCurrentVersionFile(QString);
    void verifyAndCreateDirforApp();
//C:/Programmation/test/
    static const inline QString SERVER_URL = "http://127.0.0.1/updater/serveur/";
    static const inline QString TARGET_PATH = "/Application";
    static const inline QString EXE_NAME = "sans_titre.exe";
};
#endif // MAINWINDOW_H
