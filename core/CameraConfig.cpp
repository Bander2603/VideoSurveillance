#include "CameraConfig.h"

#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>

namespace {

bool isDisabledValue(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == "0"
        || normalized == "false"
        || normalized == "no"
        || normalized == "off";
}

QString cleanName(const QString& value, const QString& fallback)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

} // namespace

QList<CameraConfig> loadCameraConfigs()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QRegularExpression dynamicUrlPattern("^VS_CAMERA_(\\d+)_URL$");

    QList<int> indexes;
    const QStringList keys = env.keys();
    for (const QString& key : keys) {
        const QRegularExpressionMatch match = dynamicUrlPattern.match(key);
        if (!match.hasMatch()) {
            continue;
        }

        indexes.push_back(match.captured(1).toInt());
    }

    std::sort(indexes.begin(), indexes.end());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

    QList<CameraConfig> configs;
    for (const int index : indexes) {
        const QString prefix = QString("VS_CAMERA_%1").arg(index);
        const QString url = env.value(prefix + "_URL").trimmed();
        if (url.isEmpty()) {
            continue;
        }

        if (isDisabledValue(env.value(prefix + "_ENABLED"))) {
            continue;
        }

        CameraConfig config;
        config.id = cleanName(env.value(prefix + "_ID"), QString("camera_%1").arg(index));
        config.name = cleanName(env.value(prefix + "_NAME"), QString("Camera %1").arg(index));
        config.rtspUrl = url;
        configs.push_back(config);
    }

    if (!configs.isEmpty()) {
        return configs;
    }

    const struct LegacyCamera {
        const char* key;
        const char* name;
    } legacyCameras[] = {
        { "VS_RTSP_TC72", "TC72" },
        { "VS_RTSP_C210", "C210" },
        { "VS_RTSP_TC70", "TC70" },
    };

    for (const LegacyCamera& legacy : legacyCameras) {
        const QString url = env.value(legacy.key).trimmed();
        if (url.isEmpty()) {
            continue;
        }

        CameraConfig config;
        config.id = QString::fromUtf8(legacy.name).toLower();
        config.name = QString::fromUtf8(legacy.name);
        config.rtspUrl = url;
        configs.push_back(config);
    }

    return configs;
}
