#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QDebug>
#include <QString>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <vector>
#include <optional>

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

    std::optional<VersionConfig> getVersionConfig(const QString& versionNumber) const;

    QString getLastError() const { return m_lastError; }

private:
    static std::vector<int> convertDecimalArrayToPattern(const QJsonArray& decimalArray);

    static bool validateVersionConfig(const VersionConfig& config);

    QHash<QString, VersionConfig> m_versionConfigs;
    QString m_lastError;
};

#endif // CONFIGMANAGER_H
