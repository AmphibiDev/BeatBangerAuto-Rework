#include "configmanager.h"

bool ConfigManager::loadFromFile(const QString& configPath)
{
    m_versionConfigs.clear();
    m_lastError.clear();

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = "Couldn't open config";
        qDebug() << "[ERROR] Could not open config file";
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        m_lastError = "Invalid JSON format";
        qDebug() << "[ERROR] Invalid JSON format in config file";
        return false;
    }

    QJsonObject rootObj = doc.object();
    if (rootObj.contains("configurations")) {
        QJsonArray configurationsArray = rootObj["configurations"].toArray();
        for (const QJsonValue& configValue : configurationsArray) {
            QJsonObject configObj = configValue.toObject();
            VersionConfig config;

            if (!configObj.contains("md5_hashes")) {
                qDebug() << "[WARNING] Version numbers missing for configuration";
                continue;
            }
            QJsonArray hashArray = configObj["md5_hashes"].toArray();
            for (const QJsonValue& md5Hash : hashArray) {
                config.md5Hashes.append(md5Hash.toString());
            }

            if (!configObj.contains("display_name")) {
                qDebug() << "[WARNING] Display name missing for configuration";
                continue;
            }
            config.displayName = configObj["display_name"].toString();

            if (!configObj.contains("autoplay")) {
                qDebug() << "[WARNING] Autoplay pattern missing for configuration";
                continue;
            }
            QJsonArray autoplayArray = configObj["autoplay"].toArray();
            config.autoplayPattern = parseAutoplay(autoplayArray);
            if (config.autoplayPattern.empty()) {
                qDebug() << "[WARNING] Invalid autoplay pattern for configuration";
                continue;
            }

            if (!configObj.contains("is_playing_offset") || !configObj.contains("time_offset")) {
                qDebug() << "[WARNING] Missing offsets for configuration";
                continue;
            }
            config.isPlayingOffset = configObj["is_playing_offset"].toInt();
            config.timeOffset = configObj["time_offset"].toInt();

            if (!validateConfig(config)) {
                qDebug() << "[WARNING] Configuration failed validation:" << config.displayName;
                continue;
            }

            for (const QString& version : config.md5Hashes) {
                m_versionConfigs[version] = config;
            }
        }
    }

    if (m_versionConfigs.isEmpty()) {
        m_lastError = "Config corrupted";
        return false;
    }

    return true;
}

std::optional<VersionConfig> ConfigManager::getVersionConfig(const QString& md5Hash) const
{
    auto it = m_versionConfigs.find(md5Hash);
    if (it != m_versionConfigs.end())
        return it.value();
    return std::nullopt;
}

std::vector<int> ConfigManager::parseAutoplay(const QJsonArray& array)
{
    std::vector<int> pattern;
    pattern.reserve(array.size());
    for (const QJsonValue& v : array) {
        if (v.isDouble()) {
            int val = v.toInt();
            pattern.push_back(val);
        }
    }
    return pattern;
}

bool ConfigManager::validateConfig(const VersionConfig& config)
{
    if (config.autoplayPattern.empty())
        return false;
    
    for (int byte : config.autoplayPattern) {
        if (byte != -1 && (byte < 0 || byte > 255))
            return false;
    }

    if (config.isPlayingOffset == 0 || config.timeOffset == 0)
        return false;

    if (std::abs(config.isPlayingOffset) > Constants::MAX_REASONABLE_OFFSET ||
        std::abs(config.timeOffset) > Constants::MAX_REASONABLE_OFFSET)
        return false;

    return true;
}