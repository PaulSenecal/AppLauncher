QT       += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/core/GameManifest.cpp \
    src/core/ManifestAuthenticator.cpp \
    src/core/NetworkClient.cpp \
    src/core/GameManager.cpp \
    src/core/Catalog.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ChangelogDialog.cpp

HEADERS += \
    src/core/GameInfo.h \
    src/core/PlatformUtils.h \
    src/core/GameManifest.h \
    src/core/ManifestAuthenticator.h \
    src/core/NetworkClient.h \
    src/core/GameManager.h \
    src/core/Catalog.h \
    src/ui/MainWindow.h \
    src/ui/ChangelogDialog.h

# Pas de mainwindow.ui : l'UI de MainWindow est construite en code dans
# MainWindow::buildUi() (voir src/ui/MainWindow.cpp) plutôt que via Designer,
# pour rester lisible/diffable sans dépendre d'un fichier .ui. Si tu préfères
# repartir d'un .ui, remets une entrée FORMS += et adapte buildUi() en
# conséquence - le reste du code ne dépend pas de ce choix.
# FORMS += \
#     src/ui/mainwindow.ui

INCLUDEPATH += src

# --- Default rules for deployment. -----------------------------------------
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    rcs.qrc