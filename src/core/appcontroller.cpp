#include "AppController.h"
#include "../core/MemoryScanner.h"
#include "../utils/UpdateManager.h"

AppController::AppController(MemoryScanner* scanner, UpdateManager* updateManager, QObject* parent)
    : QObject(parent)
    , m_scanner(scanner)
    , m_updateManager(updateManager)
    , m_isUpdating(false)
{
    setupConnections();
}

void AppController::setupConnections()
{
    connect(m_scanner, &MemoryScanner::updateCheckStarted,
            this, &AppController::onUpdateCheckRequested);
    connect(m_updateManager, &UpdateManager::updateStatus,
            this, &AppController::onUpdateStatusChanged);
    connect(m_updateManager, &UpdateManager::configUpdated,
            this, &AppController::onConfigUpdateCompleted);
    connect(m_updateManager, &UpdateManager::configUpToDate,
            this, &AppController::onConfigUpToDate);
}

void AppController::onUpdateCheckRequested()
{
    qDebug() << "[LOG] Update check requested";

    if (m_isUpdating) {
        qDebug() << "[LOG] Update already in progress, ignoring request";
        return;
    }

    m_isUpdating = true;
    m_updateManager->checkForUpdates();
}

void AppController::onUpdateStatusChanged(const QString& status)
{
    m_scanner->updateConnectionStatus(status);
}

void AppController::onConfigUpdateCompleted()
{
    qDebug() << "[LOG] Configuration update completed";
    handleUpdateFinished();
}

void AppController::onConfigUpToDate()
{
    qDebug() << "[LOG] Configuration is up to date";
    handleUpdateFinished();
}

void AppController::handleUpdateFinished()
{
    m_isUpdating = false;

    m_scanner->onUpdateCompleted();
}
