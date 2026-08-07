#include "ChangelogDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextEdit>

ChangelogDialog::ChangelogDialog(const QString &gameName, const QString &newVersion,
                                 const QStringList &changelog, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mise à jour disponible");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(QString("<b>%1</b> — version %2").arg(gameName, newVersion), this);
    layout->addWidget(title);

    auto *text = new QTextEdit(this);
    text->setReadOnly(true);
    if (changelog.isEmpty()) {
        text->setPlainText("Aucune note de version fournie.");
    } else {
        QString html = "<ul>";
        for (const QString &line : changelog)
            html += "<li>" + line.toHtmlEscaped() + "</li>";
        html += "</ul>";
        text->setHtml(html);
    }
    layout->addWidget(text);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *updateBtn = buttons->addButton("Mettre à jour", QDialogButtonBox::AcceptRole);
    buttons->addButton("Plus tard", QDialogButtonBox::RejectRole);
    updateBtn->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}