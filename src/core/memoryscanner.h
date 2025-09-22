#ifndef MEMORYSCANNER_H
#define MEMORYSCANNER_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <windows.h>
#include <vector>
#include <array>
#include <memory>

#include "../utils/configmanager.h"

class UpdateManager;

class MemoryScanner : public QObject
{
    Q_OBJECT

    // QML Properties
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool inAutoplay READ inAutoplay NOTIFY inAutoplayChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString gameVersion READ gameVersion NOTIFY gameVersionChanged)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)

public:
    explicit MemoryScanner(QObject *parent = nullptr);
    ~MemoryScanner();

    // QML Methods
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void reloadConfiguration();
    Q_INVOKABLE void onUpdateCompleted();

    // Property Getters
    bool isScanning() const;
    bool inAutoplay() const;
    QString statusText() const;
    QString gameVersion() const;
    QString connectionStatus() const;

    // Thread Worker Methods
    void performAddressScanning();
    void performAutoplayLoop();
    void updateConnectionStatus(const QString& status);
    bool shouldStopScanning() const;

signals:
    void scanningChanged(bool scanning);
    void inAutoplayChanged(bool active);
    void statusTextChanged(const QString& text);
    void connectionStatusChanged(const QString& text);
    void gameVersionChanged(const QString& version);
    void updateCheckStarted();

private:
    // Enums
    enum class State {
        Idle,
        Scanning,
        Autoplay
    };

    struct PatternSearchResult {
        int threadId;
        uintptr_t address;
        bool found;
    };

    // Nested Classes
    class MemoryRegionSearchThread : public QThread {
    public:
        MemoryRegionSearchThread(MemoryScanner* scanner, HANDLE process,
                                 const VersionConfig& config, uint8_t* startAddr,
                                 uint8_t* endAddr, int threadId);

        PatternSearchResult getResult() const { return m_result; }
        int getThreadId() const { return m_threadId; }

    protected:
        void run() override;

    private:
        MemoryScanner* m_scanner;
        HANDLE m_process;
        VersionConfig m_config;
        uint8_t* m_startAddr;
        uint8_t* m_endAddr;
        int m_threadId;
        PatternSearchResult m_result;
    };

    class WorkerThread : public QThread {
    public:
        WorkerThread(MemoryScanner* scanner, bool isAutoplay = false)
            : m_scanner(scanner), m_isAutoplay(isAutoplay) {}

    protected:
        void run() override;

    private:
        MemoryScanner* m_scanner;
        bool m_isAutoplay;
    };

    // Core Methods
    void setState(State newState);
    void updateStatus(const QString& status);
    void updateGameVersion(const QString& version);
    void startScanning();
    void startAutoplay();
    void stop();
    void cleanupThread();
    void loadConfiguration();

    // Process Methods
    HANDLE openGameProcess() const;

    // Scanning Methods
    void performParallelPatternSearch(const VersionConfig& config);
    void onMemoryRegionSearchCompleted(MemoryRegionSearchThread* completedThread,
                                       const VersionConfig& config);
    void onAllRegionsCompleted();

    // State Variables
    State m_currentState;
    QString m_currentStatus;
    QString m_gameVersion;
    QString m_connectionStatus;
    bool m_shouldStop;
    bool m_gameWasClosed;

    // Update State Variables
    bool m_waitingForUpdate;
    bool m_updateCheckRequested;

    // Process Tracking Variables
    DWORD m_lastKnownPid;
    std::array<uintptr_t, 3> m_foundAddresses;
    bool m_addressesValid;

    // Thread Management Variables
    std::unique_ptr<WorkerThread> m_workerThread;
    std::vector<std::unique_ptr<MemoryRegionSearchThread>> m_searchThreads;
    QMutex m_resultMutex;
    int m_completedSearches;
    HANDLE m_processHandle;

    // External Dependencies
    std::unique_ptr<ConfigManager> m_configManager;
    VersionConfig m_currentConfig;
};

#endif // MEMORYSCANNER_H
