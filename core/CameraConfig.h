#pragma once

#include <QList>
#include <QString>

struct CameraConfig
{
    QString id;
    QString name;
    QString rtspUrl;
};

QList<CameraConfig> loadCameraConfigs();
