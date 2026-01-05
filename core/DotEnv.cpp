#include "DotEnv.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

static QString trimQuotes(const QString& v)
{
    if (v.size() >= 2) {
        if ((v.startsWith('"') && v.endsWith('"')) || (v.startsWith('\'') && v.endsWith('\''))) {
            return v.mid(1, v.size() - 2);
        }
    }
    return v;
}

static bool loadExactFile(const QString& fullPath)
{
    QFile f(fullPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        if (line.startsWith("export "))
            line = line.mid(7).trimmed();

        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;

        QString key = line.left(eq).trimmed();
        QString val = trimQuotes(line.mid(eq + 1).trimmed());

        qputenv(key.toUtf8().constData(), val.toUtf8());
        qDebug() << "[DotEnv] Loaded:" << key;
    }

    qDebug() << "[DotEnv] Using file:" << fullPath;
    return true;
}

bool DotEnv::loadFile(const QString& path)
{
    // Info de diagnóstico (solo paths, no secretos)
    qDebug() << "[DotEnv] currentPath =" << QDir::currentPath();
    qDebug() << "[DotEnv] appDir      =" << QCoreApplication::applicationDirPath();

    // 1) path tal cual (por si ya es absoluto o relativo correcto)
    if (loadExactFile(path))
        return true;

    // 2) relativo a currentPath
    {
        const QString p = QDir(QDir::currentPath()).filePath(path);
        if (loadExactFile(p))
            return true;
    }

    // 3) relativo a la carpeta del exe
    {
        const QString p = QDir(QCoreApplication::applicationDirPath()).filePath(path);
        if (loadExactFile(p))
            return true;
    }

    // 4) subir padres desde appDir (muy típico con CMake/VS: out/build/.../Debug)
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        const QString candidate = d.filePath(path);
        if (loadExactFile(candidate))
            return true;
        if (!d.cdUp())
            break;
    }

    qDebug() << "[DotEnv] No .env found. Tried path =" << path;
    return false;
}