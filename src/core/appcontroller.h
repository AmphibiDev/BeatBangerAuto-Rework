#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

// Qt includes
#include <QObject>
#include <QString>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

// Project includes
#include "memoryscanner.h"
#include "../utils/updatemanager.h"

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(MemoryScanner* scanner, UpdateManager* updateManager, QObject* parent = nullptr);
    ~AppController() = default;

private slots:
    void onUpdateCheckRequested();
    void onUpdateStatusChanged(const QString& status);
    void onConfigUpdateCompleted();
    void onConfigUpToDate();

private:
    void setupConnections();
    void handleUpdateFinished();

    MemoryScanner* m_scanner;
    UpdateManager* m_updateManager;
    bool m_isUpdating;
};

#endif // APPCONTROLLER_H
