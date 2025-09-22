#include "updatemanager.h"
#include "../utils/constants.h"

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_githubConfigUrl("https://raw.githubusercontent.com/AmphibiDev/BeatBangerAuto-Rework/main/versions.json")
{
    m_localConfigPath = QDir(QCoreApplication::applicationDirPath()).filePath(Constants::CONFIG_FILENAME);
    m_currentVersion = readLocalVersion();

    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &UpdateManager::onNetworkTimeout);
}

// QML Methods
void UpdateManager::checkForUpdates()
{
    cleanupNetworkRequest();

    QUrl configUrl(m_githubConfigUrl);
    QNetworkRequest request(configUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "BeatBangerAuto");

    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateManager::onConfigDownloadFinished);

    m_timeoutTimer->start(Constants::NETWORK_REQUEST_TIMEOUT);
}

bool UpdateManager::hasLocalConfig() const
{
    return QFile::exists(m_localConfigPath);
}

QString UpdateManager::getLocalConfigPath() const
{
    return m_localConfigPath;
}

// Network Response Handling
void UpdateManager::onConfigDownloadFinished()
{
    m_timeoutTimer->stop();

    if (!m_currentReply) return;

    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[LOG] Network error:" << reply->errorString();

        if (reply->error() == QNetworkReply::HostNotFoundError ||
            reply->error() == QNetworkReply::TimeoutError ||
            reply->error() == QNetworkReply::NetworkSessionFailedError) {
            qDebug() << "[LOG] No internet connection";
        } else {
            qDebug() << "[LOG] Failed to download config";
        }

        proceedWithoutUpdate();
        return;
    }

    QByteArray configData = reply->readAll();
    if (configData.isEmpty()) {
        qDebug() << "[LOG] Downloaded config is empty";
        proceedWithoutUpdate();
        return;
    }

    QJsonDocument remoteDoc = QJsonDocument::fromJson(configData);
    if (!remoteDoc.isObject()) {
        qDebug() << "[LOG] Invalid JSON format";
        proceedWithoutUpdate();
        return;
    }

    QString remoteVersion = remoteDoc.object().value("version").toString("0");
    QString currentLocalVersion = readLocalVersion();

    qDebug() << "[LOG] Local version:" << currentLocalVersion << "| Github version:" << remoteVersion;

    bool shouldUpdate = !hasLocalConfig() || !isLocalConfigValid() || (currentLocalVersion != remoteVersion);

    if (shouldUpdate) {
        if (saveConfig(configData)) {
            emit configUpdated();
        } else {
            qDebug() << "[LOG] Failed to save config";
            emit configUpdated();
        }
    } else {
        emit updateStatus("Config is up to date.");
        emit configUpToDate();
    }
}

void UpdateManager::onNetworkTimeout()
{
    qDebug() << "[LOG] Network request timed out";

    cleanupNetworkRequest();

    proceedWithoutUpdate();
}

// Helper Methods
QString UpdateManager::readLocalVersion()
{
    if (!hasLocalConfig()) {
        qDebug() << "[LOG] Local config file does not exist";
        return "0";
    }

    QFile file(m_localConfigPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[LOG] Cannot open local config file";
        return "0";
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "[LOG] Local config is not valid JSON";
        return "0";
    }

    QString version = doc.object().value("version").toString("0");
    return version;
}

bool UpdateManager::saveConfig(const QByteArray& data)
{
    QFile file(m_localConfigPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[LOG] Cannot open config file for writing";
        return false;
    }

    qint64 written = file.write(data);
    file.close();

    if (written == data.size()) {
        m_currentVersion = readLocalVersion();
        qDebug() << "[LOG] Configuration saved successfully";
        return true;
    }

    qDebug() << "[LOG] Failed to write complete configuration";
    return false;
}

bool UpdateManager::isLocalConfigValid()
{
    if (!hasLocalConfig()) {
        return false;
    }

    QFile file(m_localConfigPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[LOG] Cannot open local config file for validation";
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        qDebug() << "[LOG] Local config file is empty";
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "[LOG] Local config is not valid JSON";
        return false;
    }

    QJsonObject rootObj = doc.object();

    if (!rootObj.contains("version")) {
        qDebug() << "[LOG] Local config missing version field";
        return false;
    }

    if (!rootObj.contains("configurations")) {
        qDebug() << "[LOG] Local config missing configurations field";
        return false;
    }

    QJsonArray configurations = rootObj["configurations"].toArray();
    if (configurations.isEmpty()) {
        qDebug() << "[LOG] Local config has empty configurations";
        return false;
    }

    qDebug() << "[LOG] Local config is valid";
    return true;
}

void UpdateManager::proceedWithoutUpdate()
{
    if (isLocalConfigValid()) {
        emit configUpToDate();
    }
}

void UpdateManager::cleanupNetworkRequest()
{
    if (m_currentReply) {
        m_currentReply->disconnect(this);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_timeoutTimer->stop();
}
