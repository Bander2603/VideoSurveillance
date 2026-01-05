#pragma once
#include <QString>

namespace DotEnv {
    // Carga KEY=VALUE desde un archivo .env (ruta relativa a working dir)
    // Devuelve true si pudo abrirlo y procesarlo.
    bool loadFile(const QString& path = ".env");
}