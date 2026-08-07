#include "ui/MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("TonStudio");
    QApplication::setApplicationName("Launcher");

    MainWindow window;
    window.show();
    return app.exec();
}
