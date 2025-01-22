#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
//binarycreator.exe -c installer/config/config.xml -p installer/packages sans_titre.exe
//windeployqt.exe C:\Programmation\C++\Launcher\MonLauncher\build\Desktop_Qt_6_8_1_MinGW_64_bit-release\release\MonLauncher.exe
