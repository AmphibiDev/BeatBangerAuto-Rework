#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

// Qt includes
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// Project includes
#include "../utils/constants.h"

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool localConfigExists() const;
    Q_INVOKABLE QString localConfigPath() const;

signals:
    void updateStatus(const QString& status);
    void configUpdated();
    void configUpToDate();
    void updateAvailable();
    void useLocalConfig();

private slots:
    void onConfigDownloadFinished();

private:
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;

    QString m_githubConfigUrl;
    QString m_localConfigPath;
    QString m_currentVersion;
    bool m_updateDialogShown;

    QString readLocalVersion();
    bool saveConfig(const QByteArray& data);
    bool isLocalConfigValid();
    void finalizeWithoutUpdate();
    void abortNetworkRequest();
    void showUpdateDialog();
};

#endif // UPDATEMANAGER_H
