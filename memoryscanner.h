#ifndef MEMORYSCANNER_H
#define MEMORYSCANNER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QMutex>
#include <vector>
#include <array>
#include <windows.h>

class MemoryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool inAutoplay READ inAutoplay NOTIFY inAutoplayChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit MemoryScanner(QObject *parent = nullptr);
    ~MemoryScanner();

    // QML callable methods
    Q_INVOKABLE void toggle();

    // Property getters
    bool isScanning() const;
    bool inAutoplay() const;
    QString statusText() const;

    // Thread worker methods (public so worker thread can call them)
    void performAddressScanning();
    void performAutoplayLoop();
    bool shouldStopScanning() const;

signals:
    void scanningChanged(bool scanning);
    void inAutoplayChanged(bool active);
    void statusTextChanged(const QString& text);

private:
    enum class State {
        Idle,
        Scanning,
        Autoplay
    };

    // Pattern search result structure
    struct PatternSearchResult {
        int patternIndex;
        uintptr_t address;
        bool found;
    };

    // Boyer-Moore pattern searcher with wildcard support
    class PatternMatcher {
    public:
        explicit PatternMatcher(const std::vector<int>& pattern);
        size_t find(const uint8_t* data, size_t dataSize) const;
        size_t getPatternSize() const { return m_patternSize; }

    private:
        std::vector<int> m_pattern;
        std::array<size_t, 256> m_badCharTable;
        size_t m_patternSize;

        void buildBadCharTable();
    };

    // Memory region search thread
    class MemoryRegionSearchThread : public QThread {
    public:
        MemoryRegionSearchThread(MemoryScanner* scanner, HANDLE process,
                                 const std::vector<std::vector<int>>& patterns,
                                 uint8_t* startAddr, uint8_t* endAddr, int threadId);

        std::vector<PatternSearchResult> getResults() const { return m_results; }
        int getThreadId() const { return m_threadId; }

    protected:
        void run() override;

    private:
        MemoryScanner* m_scanner;
        HANDLE m_process;
        std::vector<std::vector<int>> m_patterns;
        uint8_t* m_startAddr;
        uint8_t* m_endAddr;
        int m_threadId;
        std::vector<PatternSearchResult> m_results;
    };

    // Core functionality
    void setState(State newState);
    void updateStatus(const QString& status);
    void startScanning();
    void startAutoplay();
    void stop();
    void cleanupThread();

    // Process management
    DWORD findGameProcessId() const;
    HANDLE openGameProcess() const;
    bool isProcessRunning(HANDLE processHandle) const;

    // Memory operations
    bool readProcessMemory(HANDLE process, uintptr_t address, void* buffer, size_t size) const;
    bool writeProcessMemory(HANDLE process, uintptr_t address, const void* buffer, size_t size) const;

    // Pattern scanning (legacy single-threaded method)
    uintptr_t scanForPattern(HANDLE process, const PatternMatcher& matcher);

    // New async pattern scanning methods
    void onMemoryRegionSearchCompleted(MemoryRegionSearchThread* completedThread);
    void onAllRegionsCompleted();

    // Autoplay logic
    void disableGameAutoplay(HANDLE process);

    // Member variables
    State m_currentState;
    QString m_currentStatus;
    QThread* m_workerThread;
    bool m_shouldStop;
    bool m_gameWasClosed;

    // Game process tracking
    DWORD m_lastKnownPid;
    std::array<uintptr_t, 3> m_foundAddresses;
    bool m_addressesValid;

    // Async search management
    std::vector<MemoryRegionSearchThread*> m_searchThreads;
    QMutex m_resultMutex;
    int m_completedSearches;
    HANDLE m_processHandle; // Store process handle for search threads

    // Constants
    static constexpr size_t MEMORY_CHUNK_SIZE = 8 * 1024 * 1024; // 8MB chunks
    static constexpr int AUTOPLAY_CHECK_INTERVAL = 50; // 50ms
    static constexpr int NUM_SEARCH_THREADS = 4; // Number of memory region search threads
};

#endif // MEMORYSCANNER_H
