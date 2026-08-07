#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

// Fine couche au-dessus de QNetworkAccessManager qui centralise :
//  - le timeout (Qt ne timeout JAMAIS par défaut -> une requête peut rester
//    bloquée indéfiniment si le serveur ne répond simplement pas)
//  - la classification des erreurs (pas d'internet / serveur injoignable / 404 / autre)
//    pour pouvoir afficher un message pertinent et décider s'il faut proposer
//    un "Réessayer"
class NetworkClient : public QObject {
    Q_OBJECT
public:
    enum class ErrorKind {
        None,
        NoInternet,     // pas de connexion réseau du tout
        ServerUnreachable, // DNS résolu mais serveur ne répond pas / timeout
        NotFound,       // 404 : ressource absente côté serveur
        Other
    };

    explicit NetworkClient(QObject *parent = nullptr);

    // timeoutMs : au-delà, la requête est annulée et `finished` émet une erreur
    // TimeoutError. Sans ça, une simple coupure réseau en cours de requête peut
    // laisser l'UI bloquée indéfiniment sur "Vérification...".
    QNetworkReply *get(const QUrl &url, int timeoutMs = 8000);

    static ErrorKind classify(QNetworkReply *reply);
    static QString errorMessage(ErrorKind kind);

private:
    QNetworkAccessManager m_manager;
};
