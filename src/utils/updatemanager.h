#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QUrl>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "../utils/constants.h"

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);

    // QML Methods
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool hasLocalConfig() const;
    Q_INVOKABLE QString getLocalConfigPath() const;

signals:
    // Сигналы для AppController
    void updateStatus(const QString& status);
    void configUpdated();
    void configUpToDate();

private slots:
    void onConfigDownloadFinished();
    void onNetworkTimeout();

private:
    // Network Management
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_timeoutTimer;

    // Configuration
    QString m_githubConfigUrl;
    QString m_localConfigPath;
    QString m_currentVersion;

    // Helper Methods
    QString readLocalVersion();
    bool saveConfig(const QByteArray& data);
    bool isLocalConfigValid();
    void proceedWithoutUpdate();
    void cleanupNetworkRequest();
};

#endif // UPDATEMANAGER_H
