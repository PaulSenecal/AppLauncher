#include "GameManifest.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <algorithm>

std::optional<GameManifest> GameManifest::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = "JSON invalide: " + parseErr.errorString();
        return std::nullopt;
    }

    const QJsonObject obj = doc.object();
    if (!obj.contains("version") || !obj.contains("files") || !obj.contains("exe")) {
        if (error) *error = "Champs requis manquants (version/exe/files)";
        return std::nullopt;
    }

    GameManifest m;
    m.version = obj.value("version").toString();
    m.exeBaseName = obj.value("exe").toString();
    m.signatureB64 = obj.value("signature").toString().toUtf8();

    for (const QJsonValue &v : obj.value("changelog").toArray())
        m.changelog << v.toString();

    for (const QJsonValue &v : obj.value("files").toArray()) {
        const QJsonObject fo = v.toObject();
        ManifestFile f;
        f.path = fo.value("path").toString();
        f.sha256 = fo.value("sha256").toString().toLower();
        f.size = static_cast<qint64>(fo.value("size").toDouble());
        if (f.path.isEmpty() || f.sha256.isEmpty()) {
            if (error) *error = "Entrée de fichier invalide dans le manifest";
            return std::nullopt;
        }
        m.files << f;
    }

    // Ordre canonique : indispensable pour que canonicalPayload() soit stable
    std::sort(m.files.begin(), m.files.end(),
              [](const ManifestFile &a, const ManifestFile &b) { return a.path < b.path; });

    return m;
}

std::optional<GameManifest> GameManifest::fromLocalFile(const QString &path)
{
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return std::nullopt;
    QString err;
    auto m = fromJson(f.readAll(), &err);
    f.close();
    return m;
}

bool GameManifest::saveToLocalFile(const QString &path) const
{
    QJsonObject obj;
    obj["version"] = version;
    obj["exe"] = exeBaseName;
    obj["signature"] = QString::fromUtf8(signatureB64);

    QJsonArray changelogArr;
    for (const QString &l : changelog) changelogArr.append(l);
    obj["changelog"] = changelogArr;

    QJsonArray filesArr;
    for (const ManifestFile &f : files) {
        QJsonObject fo;
        fo["path"] = f.path;
        fo["sha256"] = f.sha256;
        fo["size"] = static_cast<double>(f.size);
        filesArr.append(fo);
    }
    obj["files"] = filesArr;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    out.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    out.close();
    return true;
}

QByteArray GameManifest::canonicalPayload() const
{
    // Sérialisation déterministe et minimale : uniquement ce qui doit être garanti
    // authentique. Format texte simple plutôt que JSON pour éviter toute ambiguïté
    // de sérialisation entre le script Python (signature) et Qt (vérification).
    //
    //   version\n
    //   exe\n
    //   path1|sha1|size1\n
    //   path2|sha2|size2\n
    //   ...
    QByteArray out;
    out += version.toUtf8() + '\n';
    out += exeBaseName.toUtf8() + '\n';
    for (const ManifestFile &f : files) {
        out += f.path.toUtf8() + '|' + f.sha256.toUtf8() + '|'
             + QByteArray::number(f.size) + '\n';
    }
    return out;
}

QHash<QString, ManifestFile> GameManifest::filesByPath() const
{
    QHash<QString, ManifestFile> h;
    h.reserve(files.size());
    for (const ManifestFile &f : files) h.insert(f.path, f);
    return h;
}

UpdateDiff computeDiff(const GameManifest &local, const GameManifest &remote)
{
    UpdateDiff diff;
    const auto localFiles = local.filesByPath();
    const auto remoteFiles = remote.filesByPath();

    for (const ManifestFile &rf : remote.files) {
        const auto it = localFiles.find(rf.path);
        if (it == localFiles.end() || it->sha256 != rf.sha256) {
            diff.toDownload << rf;
            diff.totalBytes += rf.size;
        }
    }
    for (const QString &localPath : localFiles.keys()) {
        if (!remoteFiles.contains(localPath))
            diff.toDelete << localPath;
    }
    return diff;
}
