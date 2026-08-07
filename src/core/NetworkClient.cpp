#include "NetworkClient.h"
#include <QNetworkRequest>

NetworkClient::NetworkClient(QObject *parent) : QObject(parent) {}

QNetworkReply *NetworkClient::get(const QUrl &url, int timeoutMs)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(timeoutMs);
#endif
    return m_manager.get(request);
}

NetworkClient::ErrorKind NetworkClient::classify(QNetworkReply *reply)
{
    switch (reply->error()) {
    case QNetworkReply::NoError:
        return ErrorKind::None;
    case QNetworkReply::ContentNotFoundError:
        return ErrorKind::NotFound;
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
        return ErrorKind::NoInternet;
    case QNetworkReply::TimeoutError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
        return ErrorKind::ServerUnreachable;
    default:
        return ErrorKind::Other;
    }
}

QString NetworkClient::errorMessage(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::None:              return QString();
    case ErrorKind::NoInternet:        return "Pas de connexion internet";
    case ErrorKind::ServerUnreachable: return "Serveur injoignable";
    case ErrorKind::NotFound:          return "Ressource introuvable sur le serveur";
    case ErrorKind::Other:             return "Erreur réseau inattendue";
    }
    return "Erreur inconnue";
}
