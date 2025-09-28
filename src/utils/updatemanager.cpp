#include "updatemanager.h"

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_githubConfigUrl(Constants::GITHUB_CONFIG_URL)
    , m_updateDialogShown(false)
{
    m_localConfigPath = QDir(QCoreApplication::applicationDirPath()).filePath(Constants::CONFIG_FILENAME);
    m_currentVersion = readLocalVersion();
}

void UpdateManager::checkForUpdates()
{
    abortNetworkRequest();

    QUrl configUrl(m_githubConfigUrl);
    QNetworkRequest request(configUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "BeatBangerAuto");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateManager::onConfigDownloadFinished);
}

bool UpdateManager::localConfigExists() const
{
    return QFile::exists(m_localConfigPath);
}

QString UpdateManager::localConfigPath() const
{
    return m_localConfigPath;
}

void UpdateManager::onConfigDownloadFinished()
{
    if (!m_currentReply) return;

    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[LOG] Network error:" << reply->errorString();
        emit updateStatus("No internet connection");

        if (localConfigExists() && isLocalConfigValid()) {
            qDebug() << "[LOG] Using local config due to no internet";
            emit useLocalConfig();
        } else {
            qDebug() << "[LOG] No valid local config available";
            emit configUpToDate();
        }
        return;
    }

    QByteArray configData = reply->readAll();
    if (configData.isEmpty()) {
        qDebug() << "[LOG] Downloaded config is empty";
        emit updateStatus("Downloaded config is empty");
        emit configUpToDate();
        return;
    }

    QJsonDocument remoteDoc = QJsonDocument::fromJson(configData);
    if (!remoteDoc.isObject()) {
        qDebug() << "[LOG] Invalid JSON format";
        emit updateStatus("Invalid config format");
        emit configUpToDate();
        return;
    }

    QString remoteAppVersion = remoteDoc.object().value("app_version").toString("");
    if (remoteAppVersion.isEmpty()) {
        qDebug() << "[LOG] Missing app_version in config";
        emit updateStatus("Invalid config structure");
        emit configUpToDate();
        return;
    }

    if (remoteAppVersion != Constants::APP_VERSION) {
        qDebug() << "[LOG] App version mismatch. Local:" << QString(Constants::APP_VERSION) << "| Remote:" << remoteAppVersion;

        if (!m_updateDialogShown) {
            m_updateDialogShown = true;
            showUpdateDialog();
        }

        if (localConfigExists() && isLocalConfigValid()) {
            qDebug() << "[LOG] Using existing local config despite version mismatch";
            emit configUpToDate();
        } else {
            qDebug() << "[LOG] No valid local config available";
            emit updateStatus("No config available");
            emit configUpToDate();
        }
        return;
    }

    QString remoteConfigVersion = remoteDoc.object().value("config_version").toString("0");
    QString currentConfigVersion = readLocalVersion();

    qDebug() << "[LOG] Local config version:" << currentConfigVersion << "| Github config version:" << remoteConfigVersion;

    bool shouldUpdate = !localConfigExists() || !isLocalConfigValid() || (currentConfigVersion != remoteConfigVersion);

    if (shouldUpdate) {
        if (saveConfig(configData)) {
            emit updateStatus("Config is up to date.");
            emit configUpdated();
        } else {
            qDebug() << "[LOG] Failed to save config";
            emit updateStatus("Failed to save config");
            emit configUpdated();
        }
    } else {
        emit updateStatus("Config is up to date.");
        emit configUpToDate();
    }
}

QString UpdateManager::readLocalVersion()
{
    if (!localConfigExists()) {
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

    QString version = doc.object().value("config_version").toString("0");
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
    if (!localConfigExists()) {
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

    if (!rootObj.contains("app_version")) {
        qDebug() << "[LOG] Local config missing app_version field";
        return false;
    }

    if (!rootObj.contains("config_version")) {
        qDebug() << "[LOG] Local config missing config_version field";
        return false;
    }

    QJsonArray configurations = rootObj["configurations"].toArray();
    if (configurations.isEmpty()) {
        qDebug() << "[LOG] Local config has empty configurations";
        return false;
    }

    return true;
}

void UpdateManager::finalizeWithoutUpdate()
{
    emit configUpToDate();
}

void UpdateManager::abortNetworkRequest()
{
    if (m_currentReply) {
        m_currentReply->disconnect(this);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void UpdateManager::showUpdateDialog()
{
    QMessageBox msgBox(
        QMessageBox::Warning,
        "New app version is available!",
        "This program requires an update to function properly<br>"
        "Would you like to visit GitHub releases tab to download update?",
        QMessageBox::Yes | QMessageBox::No
    );

    msgBox.setWindowFlags(msgBox.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl(Constants::GITHUB_RELEASES_URL));
    }

    emit updateAvailable();
}
