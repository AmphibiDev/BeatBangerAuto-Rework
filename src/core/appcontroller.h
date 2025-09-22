#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QString>
#include <QDebug>

class MemoryScanner;
class UpdateManager;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(MemoryScanner* scanner, UpdateManager* updateManager, QObject* parent = nullptr);
    ~AppController() = default;

private slots:
    // MemoryScanner event handlers
    void onUpdateCheckRequested();

    // UpdateManager event handlers
    void onUpdateStatusChanged(const QString& status);
    void onConfigUpdateCompleted();
    void onConfigUpToDate();

private:
    // Managed components
    MemoryScanner* m_scanner;
    UpdateManager* m_updateManager;

    // State tracking
    bool m_isUpdating;

    // Internal methods
    void setupConnections();
    void handleUpdateFinished();
};

#endif // APPCONTROLLER_H
