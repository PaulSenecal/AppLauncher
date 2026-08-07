#pragma once
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

// Regroupe tout ce qui diffère d'un OS à l'autre, pour ne JAMAIS avoir
// de #ifdef ou de chemin/commande spécifique dispersés dans le reste du code.
namespace PlatformUtils {

// Racine locale où le launcher installe tous les jeux :
// - Windows : C:/Users/<user>/AppData/Local/<Org>/Launcher/Games
// - Linux   : ~/.local/share/<Org>/Launcher/Games
// - macOS   : ~/Library/Application Support/<Org>/Launcher/Games
inline QString gamesRootDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base + "/Games");
    if (!dir.exists()) dir.mkpath(".");
    return dir.absolutePath();
}

// Ajoute l'extension exécutable propre à l'OS courant.
inline QString executableName(const QString &baseName)
{
#if defined(Q_OS_WIN)
    return baseName + ".exe";
#else
    return baseName; // Linux/macOS : pas d'extension (macOS .app géré à part si besoin)
#endif
}

// Rend un fichier exécutable après écriture (no-op sous Windows).
inline bool ensureExecutable(const QString &filePath)
{
#if defined(Q_OS_WIN)
    Q_UNUSED(filePath);
    return true;
#else
    QFile f(filePath);
    return f.setPermissions(f.permissions()
                             | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif
}

} // namespace PlatformUtils
