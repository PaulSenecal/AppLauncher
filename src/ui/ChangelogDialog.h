#pragma once
#include <QDialog>

class QLabel;

// Petite fenêtre "Quoi de neuf" affichée avant d'appliquer une mise à jour.
// L'utilisateur peut différer l'update (le jeu reste jouable dans son ancienne
// version) ou l'appliquer immédiatement.
class ChangelogDialog : public QDialog {
    Q_OBJECT
public:
    ChangelogDialog(const QString &gameName, const QString &newVersion,
                     const QStringList &changelog, QWidget *parent = nullptr);
};
