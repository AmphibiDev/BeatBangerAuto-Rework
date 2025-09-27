#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

// Qt includes
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// STL includes
#include <cmath>
#include <optional>
#include <vector>

// Project includes
#include "../utils/constants.h"

struct VersionConfig {
    std::vector<int> autoplayPattern;
    int isPlayingOffset;
    int timeOffset;
    QString displayName;
    QStringList md5Hashes;

    bool isValid() const {
        return !autoplayPattern.empty() && isPlayingOffset != 0 && timeOffset != 0;
    }
};

class ConfigManager
{
public:
    bool loadFromFile(const QString& configPath);

    std::optional<VersionConfig> getVersionConfig(const QString& md5Hash) const;

    QString getLastError() const { return m_lastError; }

private:
    static std::vector<int> parseAutoplay(const QJsonArray& array);
    static bool validateConfig(const VersionConfig& config);

    QHash<QString, VersionConfig> m_versionConfigs;
    QString m_lastError;
};

#endif // CONFIGMANAGER_H
