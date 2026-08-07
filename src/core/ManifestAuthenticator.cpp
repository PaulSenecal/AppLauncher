#include "ManifestAuthenticator.h"
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QDebug>

ManifestAuthenticator::ManifestAuthenticator(const QString &secretBase64)
{
    m_secret = QByteArray::fromBase64(secretBase64.toUtf8());
    if (m_secret.size() < 32) { // en dessous de 32 octets, le secret est trop faible
        qWarning() << "ManifestAuthenticator: clé absente ou trop courte"
                   << "(" << m_secret.size() << "octets, 32 minimum attendus)";
        return;
    }
    m_keyLoaded = true;
}

bool ManifestAuthenticator::verify(const QByteArray &message, const QByteArray &tagB64) const
{
    if (!m_keyLoaded) return false;

    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(m_secret);
    mac.addData(message);
    const QByteArray expected = mac.result();
    const QByteArray provided = QByteArray::fromBase64(tagB64);

    if (provided.size() != expected.size()) return false;

    // Comparaison en temps constant : évite qu'une différence de timing entre
    // "le premier octet ne correspond pas" et "les 31 premiers correspondent"
    // ne fuite d'information exploitable sur le tag attendu.
    unsigned char diff = 0;
    for (int i = 0; i < expected.size(); ++i)
        diff |= static_cast<unsigned char>(expected[i]) ^ static_cast<unsigned char>(provided[i]);
    return diff == 0;
}
