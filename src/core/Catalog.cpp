#include "Catalog.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

Catalog::Catalog(NetworkClient *network, const QString &serverBaseUrl, QObject *parent)
    : QObject(parent), m_network(network), m_serverBaseUrl(serverBaseUrl) {}

void Catalog::fetch()
{
    QNetworkReply *reply = m_network->get(QUrl(m_serverBaseUrl + "catalog.json"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const auto errKind = NetworkClient::classify(reply);
        if (errKind != NetworkClient::ErrorKind::None) {
            emit fetched({}, NetworkClient::errorMessage(errKind));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QVector<GameInfo> games;
        for (const QJsonValue &v : doc.object().value("games").toArray()) {
            const QJsonObject o = v.toObject();
            GameInfo g;
            g.id = o.value("id").toString();
            g.name = o.value("name").toString();
            g.serverPath = o.value("path").toString();
            g.exeBaseName = o.value("exe").toString();
            g.iconUrl = o.value("icon").toString();
            g.forumUrl = o.value("forumUrl").toString();
            g.patchNotesUrl = o.value("patchNotesUrl").toString();
            if (!g.id.isEmpty()) games << g;
        }
        emit fetched(games, QString());
    });
}
