#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>

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
    void handleDownloadFinished(QNetworkReply *reply);
    void launchApplication();

    void on_launchButton_clicked();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QString currentVersion;
    QString latestVersion;
    QString applicationPath;

    bool isUpdateAvailable();
    void installUpdate(const QByteArray &updateData);
};
#endif // MAINWINDOW_H
