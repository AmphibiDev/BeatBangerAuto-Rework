#include "appcontroller.h"

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
    connect(m_scanner, &MemoryScanner::updateCheckStarted, this, &AppController::onUpdateCheckRequested);
    connect(m_updateManager, &UpdateManager::updateStatus, this, &AppController::onUpdateStatusChanged);
    connect(m_updateManager, &UpdateManager::configUpdated, this, &AppController::onConfigUpdateCompleted);
    connect(m_updateManager, &UpdateManager::configUpToDate, this, &AppController::onConfigUpToDate);
    connect(m_updateManager, &UpdateManager::useLocalConfig, this, &AppController::onUseLocalConfig);
}

void AppController::onUpdateCheckRequested()
{
    if (m_isUpdating) {
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
    handleUpdateFinished();
}

void AppController::onConfigUpToDate()
{
    handleUpdateFinished();
}

void AppController::onUseLocalConfig()
{
    handleUpdateFinished();
}

void AppController::handleUpdateFinished()
{
    m_isUpdating = false;
    m_scanner->onUpdateDone();
}
