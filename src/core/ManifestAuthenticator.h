#pragma once
#include <QByteArray>
#include <QString>

// Authentifie un manifest via HMAC-SHA256 (clé secrète partagée), avec les
// seules briques fournies par QtCore (QMessageAuthenticationCode) - aucune
// dépendance externe à installer/configurer.
//
// IMPORTANT - à bien comprendre avant d'utiliser ça en prod :
// Contrairement à une signature asymétrique (Ed25519/RSA), la clé embarquée
// ici dans le launcher EST le secret lui-même, pas une clé publique. Quelqu'un
// qui décompile le binaire peut potentiellement l'extraire et forger de faux
// manifests. Ça protège contre "un tiers qui compromet le serveur mais n'a pas
// accès à ton binaire", pas contre "un attaquant qui a pris le temps de
// reverse-engineer le launcher". Si tu veux la garantie forte, il faut une
// lib de crypto asymétrique externe (libsodium, OpenSSL...).
class ManifestAuthenticator {
public:
    // secretBase64 : clé partagée générée une fois par tools/keygen.py,
    // encodée en base64, embarquée en dur dans le launcher (voir MainWindow.cpp)
    // ET gardée par le script tools/sign_manifest.py côté build.
    explicit ManifestAuthenticator(const QString &secretBase64);

    bool isValid() const { return m_keyLoaded; }

    // message : GameManifest::canonicalPayload()
    // tagB64  : GameManifest::signatureB64 (le nom du champ JSON reste
    //           "signature" pour ne pas casser le format, même si c'est
    //           techniquement un tag HMAC ici)
    bool verify(const QByteArray &message, const QByteArray &tagB64) const;

private:
    QByteArray m_secret;
    bool m_keyLoaded = false;
};
