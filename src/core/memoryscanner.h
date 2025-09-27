#ifndef MEMORYSCANNER_H
#define MEMORYSCANNER_H

// Qt includes
#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

// STL includes
#include <vector>
#include <array>
#include <memory>
#include <algorithm>

// System includes
#include <windows.h>

// Project includes
#include "patternmatcher.h"
#include "../utils/configmanager.h"
#include "../utils/constants.h"
#include "../platform/windows/processmanager.h"

class UpdateManager;

class MemoryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool inAutoplay READ inAutoplay NOTIFY inAutoplayChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString gameVersion READ gameVersion NOTIFY gameVersionChanged)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)

public:
    explicit MemoryScanner(QObject *parent = nullptr);
    ~MemoryScanner();

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void reloadConfig();
    Q_INVOKABLE void onUpdateDone();

    bool isScanning() const;
    bool inAutoplay() const;
    QString statusText() const;
    QString gameVersion() const;
    QString connectionStatus() const;
    void updateConnectionStatus(const QString& status);

signals:
    void scanningChanged(bool scanning);
    void inAutoplayChanged(bool active);
    void statusTextChanged(const QString& text);
    void connectionStatusChanged(const QString& text);
    void gameVersionChanged(const QString& version);
    void updateCheckStarted();

private:
    enum class State { Idle, Scanning, Autoplay };

    struct PatternSearchResult {
        int threadId;
        uintptr_t address;
        bool found;
    };

    class MemoryRegionScanThread : public QThread {
    public:
        MemoryRegionScanThread(MemoryScanner* scanner, HANDLE process,
                                 const VersionConfig& config, uint8_t* startAddr,
                                 uint8_t* endAddr, int threadId);
        PatternSearchResult getResult() const { return m_result; }
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

    void setState(State newState);
    void updateStatus(const QString& status);
    void updateGameVersion(const QString& version);
    void loadConfig();
    void startScan();
    void startAutoplay();
    void stop();
    void cleanup();
    void parallelScan(const VersionConfig& config);
    void regionComplete(MemoryRegionScanThread* thread, const VersionConfig& config);
    void allRegionsComplete();
    bool shouldStop() const;
    void scanMemory();
    void runAutoplay();
    HANDLE openProcess() const;

    State m_state;
    QString m_status;
    QString m_gameVersion;
    QString m_connectionStatus;
    bool m_shouldStop;
    bool m_gameWasClosed;
    bool m_waitingForUpdate;
    bool m_updateRequested;
    bool m_addressesValid;
    
    DWORD m_lastPid;
    std::array<uintptr_t, 3> m_addresses;
    
    std::unique_ptr<WorkerThread> m_worker;
    std::vector<std::unique_ptr<MemoryRegionScanThread>> m_scanThreads;
    QMutex m_mutex;
    int m_completedScans;
    HANDLE m_process;

    std::unique_ptr<ConfigManager> m_config;
    VersionConfig m_currentConfig;
};

#endif // MEMORYSCANNER_H
