#include "MemoryScanner.h"
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QMutexLocker>
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>

MemoryScanner::PatternMatcher::PatternMatcher(const std::vector<int>& pattern)
    : m_pattern(pattern), m_patternSize(pattern.size())
{
    buildBadCharTable();
}

void MemoryScanner::PatternMatcher::buildBadCharTable()
{
    m_badCharTable.fill(m_patternSize);

    for (size_t i = 0; i < m_patternSize - 1; ++i) {
        if (m_pattern[i] != -1) {
            uint8_t byte = static_cast<uint8_t>(m_pattern[i]);
            m_badCharTable[byte] = m_patternSize - 1 - i;
        } else {
            for (size_t j = 0; j < 256; ++j) {
                if (m_badCharTable[j] > m_patternSize - 1 - i) {
                    m_badCharTable[j] = m_patternSize - 1 - i;
                }
            }
        }
    }
}

size_t MemoryScanner::PatternMatcher::find(const uint8_t* data, size_t dataSize) const
{
    if (m_patternSize == 0 || dataSize < m_patternSize) {
        return SIZE_MAX;
    }

    size_t pos = 0;
    while (pos <= dataSize - m_patternSize) {
        size_t patternPos = m_patternSize - 1;

        while (true) {
            uint8_t dataByte = data[pos + patternPos];
            int patternByte = m_pattern[patternPos];

            if (patternByte != -1 && dataByte != static_cast<uint8_t>(patternByte)) {
                pos += m_badCharTable[data[pos + m_patternSize - 1]];
                break;
            }

            if (patternPos == 0) {
                return pos;
            }
            --patternPos;
        }
    }

    return SIZE_MAX;
}

MemoryScanner::MemoryRegionSearchThread::MemoryRegionSearchThread(
    MemoryScanner* scanner, HANDLE process,
    const std::vector<std::vector<int>>& patterns,
    uint8_t* startAddr, uint8_t* endAddr, int threadId)
    : m_scanner(scanner), m_process(process), m_patterns(patterns),
    m_startAddr(startAddr), m_endAddr(endAddr), m_threadId(threadId)
{
    // Initialize results
    m_results.resize(patterns.size());
    for (size_t i = 0; i < patterns.size(); ++i) {
        m_results[i] = {static_cast<int>(i), 0, false};
    }
}

void MemoryScanner::MemoryRegionSearchThread::run()
{
    std::vector<MemoryScanner::PatternMatcher> matchers;
    for (const auto& pattern : m_patterns) {
        matchers.emplace_back(pattern);
    }

    MEMORY_BASIC_INFORMATION memInfo;
    uint8_t* address = m_startAddr;
    std::vector<uint8_t> buffer(MemoryScanner::MEMORY_CHUNK_SIZE);

    int foundPatterns = 0;

    while (address < m_endAddr && VirtualQueryEx(m_process, address, &memInfo, sizeof(memInfo))) {
        if (m_scanner->shouldStopScanning()) {
            return;
        }

        uint8_t* regionStart = static_cast<uint8_t*>(memInfo.BaseAddress);
        if (regionStart >= m_endAddr) {
            break;
        }

        bool isReadable = (memInfo.State == MEM_COMMIT) &&
                          !(memInfo.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                          (memInfo.Protect & (PAGE_READONLY | PAGE_READWRITE |
                                              PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

        if (isReadable && foundPatterns < static_cast<int>(m_patterns.size())) {
            size_t regionSize = memInfo.RegionSize;

            if (regionStart + regionSize > m_endAddr) {
                regionSize = m_endAddr - regionStart;
                if (regionSize == 0) {
                    break;
                }
            }

            size_t offset = 0;

            while (offset < regionSize && !m_scanner->shouldStopScanning()) {
                size_t chunkSize = std::min(MemoryScanner::MEMORY_CHUNK_SIZE, regionSize - offset);
                SIZE_T bytesRead;

                if (ReadProcessMemory(m_process, regionStart + offset, buffer.data(), chunkSize, &bytesRead) && bytesRead > 0) {
                    for (size_t i = 0; i < matchers.size(); ++i) {
                        if (!m_results[i].found && bytesRead >= matchers[i].getPatternSize()) {
                            size_t foundPos = matchers[i].find(buffer.data(), bytesRead);
                            if (foundPos != SIZE_MAX) {
                                m_results[i] = {static_cast<int>(i),
                                                reinterpret_cast<uintptr_t>(regionStart) + offset + foundPos,
                                                true};
                                foundPatterns++;

                                if (foundPatterns >= static_cast<int>(m_patterns.size())) return;
                            }
                        }
                    }
                }

                offset += chunkSize;
            }
        }

        address = static_cast<uint8_t*>(memInfo.BaseAddress) + memInfo.RegionSize;
    }
}

class WorkerThread : public QThread
{
public:
    WorkerThread(MemoryScanner* scanner, bool isAutoplay = false)
        : m_scanner(scanner), m_isAutoplay(isAutoplay) {}

protected:
    void run() override {
        if (m_isAutoplay) {
            m_scanner->performAutoplayLoop();
        } else {
            m_scanner->performAddressScanning();
        }
    }

private:
    MemoryScanner* m_scanner;
    bool m_isAutoplay;
};

MemoryScanner::MemoryScanner(QObject *parent)
    : QObject(parent)
    , m_currentState(State::Idle)
    , m_currentStatus("Made by Amphibi")
    , m_workerThread(nullptr)
    , m_shouldStop(false)
    , m_lastKnownPid(0)
    , m_addressesValid(false)
    , m_completedSearches(0)
    , m_processHandle(nullptr)
    , m_gameWasClosed(false)
{
    qRegisterMetaType<quintptr>("quintptr");
    m_foundAddresses.fill(0);
}

MemoryScanner::~MemoryScanner()
{
    stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
        if (m_workerThread->isRunning()) {
            m_workerThread->terminate();
            m_workerThread->wait(1000);
        }
        delete m_workerThread;
    }
}

bool MemoryScanner::isScanning() const
{
    return m_currentState == State::Scanning;
}

bool MemoryScanner::inAutoplay() const
{
    return m_currentState == State::Autoplay;
}

QString MemoryScanner::statusText() const
{
    return m_currentStatus;
}

bool MemoryScanner::shouldStopScanning() const
{
    return m_shouldStop;
}

void MemoryScanner::setState(State newState)
{
    if (m_currentState == newState) {
        return;
    }

    State oldState = m_currentState;
    m_currentState = newState;

    if ((oldState == State::Scanning) != (newState == State::Scanning)) {
        emit scanningChanged(newState == State::Scanning);
    }

    if ((oldState == State::Autoplay) != (newState == State::Autoplay)) {
        emit inAutoplayChanged(newState == State::Autoplay);
    }
}

void MemoryScanner::updateStatus(const QString& status)
{
    if (m_currentStatus != status) {
        m_currentStatus = status;
        emit statusTextChanged(status);
        qDebug() << "Status:" << status;
    }
}

void MemoryScanner::toggle()
{
    switch (m_currentState) {
    case State::Idle:
        m_gameWasClosed = false;
        if (m_addressesValid) {
            DWORD currentPid = findGameProcessId();
            if (currentPid != 0 && currentPid == m_lastKnownPid) {
                startAutoplay();
                return;
            } else {
                m_addressesValid = false;
                m_foundAddresses.fill(0);
            }
        }
        startScanning();
        break;

    case State::Scanning:
    case State::Autoplay:
        stop();
        break;
    }
}

DWORD MemoryScanner::findGameProcessId() const
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
    DWORD pid = 0;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"beatbanger.exe") == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

HANDLE MemoryScanner::openGameProcess() const
{
    DWORD pid = findGameProcessId();
    if (pid == 0) {
        return nullptr;
    }

    return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
}

bool MemoryScanner::isProcessRunning(HANDLE processHandle) const
{
    if (!processHandle) {
        return false;
    }

    DWORD exitCode;
    return GetExitCodeProcess(processHandle, &exitCode) && exitCode == STILL_ACTIVE;
}

bool MemoryScanner::readProcessMemory(HANDLE process, uintptr_t address, void* buffer, size_t size) const
{
    SIZE_T bytesRead;
    return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead)
           && bytesRead == size;
}

bool MemoryScanner::writeProcessMemory(HANDLE process, uintptr_t address, const void* buffer, size_t size) const
{
    SIZE_T bytesWritten;
    return WriteProcessMemory(process, reinterpret_cast<LPVOID>(address), buffer, size, &bytesWritten)
           && bytesWritten == size;
}

void MemoryScanner::startScanning()
{
    cleanupThread();

    setState(State::Scanning);
    m_shouldStop = false;

    m_workerThread = new WorkerThread(this, false);

    connect(m_workerThread, &QThread::finished, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            cleanupThread();
        });
    });

    m_workerThread->start();
}

void MemoryScanner::startAutoplay()
{
    cleanupThread();

    setState(State::Autoplay);
    m_shouldStop = false;

    m_workerThread = new WorkerThread(this, true);

    connect(m_workerThread, &QThread::finished, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            cleanupThread();
        });
    });

    m_workerThread->start();
}

void MemoryScanner::stop()
{
    if (m_currentState == State::Idle) {
        return;
    }

    if (m_currentState == State::Autoplay) {
        HANDLE process = openGameProcess();
        if (process) {
            disableGameAutoplay(process);
            CloseHandle(process);
        }
    }

    m_shouldStop = true;
    setState(State::Idle);

    if (!m_gameWasClosed) {
        updateStatus("Made by Amphibi");
    }

    for (auto* thread : m_searchThreads) {
        if (thread && thread->isRunning()) {
            thread->quit();
            if (!thread->wait(2000)) {
                thread->terminate();
                thread->wait(1000);
            }
        }
    }
}

void MemoryScanner::cleanupThread()
{
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_shouldStop = true;
            m_workerThread->quit();

            if (!m_workerThread->wait(2000)) {
                m_workerThread->terminate();
                m_workerThread->wait(1000);
            }
        }

        delete m_workerThread;
        m_workerThread = nullptr;
    }
}

void MemoryScanner::performAddressScanning()
{
    m_processHandle = openGameProcess();
    if (!m_processHandle) {
        QTimer::singleShot(0, this, [this]() {
            updateStatus("Game not found");
            setState(State::Idle);
        });
        return;
    }

    m_lastKnownPid = findGameProcessId();
    m_gameWasClosed = false;

    const std::vector<std::vector<int>> patterns = {
        // Pattern 0: Autoplay control
        { -1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x01,0x00,0x00,0x00,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,-1,-1,-1,-1,
         -1,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,
         0x00,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,-1,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,0x00,0x00,0x00,
         -1,0x00,0x00,0x00,0x00,0x00,0x00 },

        // Pattern 1: IsPlaying status
        { -1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         -1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0xF0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00 },

        // Pattern 2: Time value
        { -1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,-1,-1,-1,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,-1,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,-1,-1,-1,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,-1,-1,-1,-1,-1,-1,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         -1,-1,-1,-1,-1,-1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,-1,-1,-1,-1 }
    };

    QTimer::singleShot(0, this, [this]() {
        updateStatus("Starting parallel scan...");
    });

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uint8_t* memStart = static_cast<uint8_t*>(sysInfo.lpMinimumApplicationAddress);
    uint8_t* memEnd = static_cast<uint8_t*>(sysInfo.lpMaximumApplicationAddress);

    uintptr_t totalRange = reinterpret_cast<uintptr_t>(memEnd) - reinterpret_cast<uintptr_t>(memStart);
    uintptr_t regionSize = totalRange / NUM_SEARCH_THREADS;

    m_searchThreads.clear();
    m_completedSearches = 0;
    m_foundAddresses.fill(0);

    for (int i = 0; i < NUM_SEARCH_THREADS; ++i) {
        uint8_t* start = memStart + (i * regionSize);
        uint8_t* end = (i == NUM_SEARCH_THREADS - 1) ? memEnd : start + regionSize;

        auto* thread = new MemoryRegionSearchThread(this, m_processHandle, patterns, start, end, i);
        m_searchThreads.push_back(thread);

        connect(thread, &QThread::finished, this, [this, thread]() {
            onMemoryRegionSearchCompleted(thread);
        });

        thread->start();
    }
}

void MemoryScanner::onMemoryRegionSearchCompleted(MemoryRegionSearchThread* completedThread)
{
    QMutexLocker locker(&m_resultMutex);

    m_completedSearches++;
    auto results = completedThread->getResults();
    for (const auto& result : results) {
        if (result.found && m_foundAddresses[result.patternIndex] == 0) {
            m_foundAddresses[result.patternIndex] = result.address;
        }
    }

    if (m_completedSearches >= NUM_SEARCH_THREADS) {
        QTimer::singleShot(0, this, [this]() {
            onAllRegionsCompleted();
        });
    }
}

void MemoryScanner::onAllRegionsCompleted()
{
    for (auto* thread : m_searchThreads) {
        if (thread) {
            if (thread->isRunning()) {
                thread->quit();
                thread->wait(1000);
            }
            thread->deleteLater();
        }
    }
    m_searchThreads.clear();

    if (m_processHandle) {
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
    }

    if (m_shouldStop) {
        setState(State::Idle);
        updateStatus("Made by Amphibi");
        return;
    }

    DWORD currentPid = findGameProcessId();
    if (currentPid == 0 || currentPid != m_lastKnownPid) {
        m_addressesValid = false;
        m_gameWasClosed = true;
        setState(State::Idle);
        updateStatus("Game was closed");
        return;
    }

    int foundCount = 0;
    for (int i = 0; i < 3; ++i) {
        if (m_foundAddresses[i] != 0) {
            foundCount++;
        }
    }

    if (foundCount == 3) {
        m_addressesValid = true;
        setState(State::Autoplay);

        m_workerThread = new WorkerThread(this, true);
        connect(m_workerThread, &QThread::finished, this, [this]() {
            QTimer::singleShot(0, this, [this]() {
                cleanupThread();
            });
        });
        m_workerThread->start();
    } else {
        m_addressesValid = false;
        setState(State::Idle);
        updateStatus("Addresses not found");
    }
}

void MemoryScanner::performAutoplayLoop()
{
    HANDLE process = openGameProcess();
    if (!process) {
        QTimer::singleShot(0, this, [this]() {
            setState(State::Idle);
            updateStatus("Game not found");
        });
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        updateStatus("Autoplay is active");
    });

    const uintptr_t autoplayAddr = m_foundAddresses[0];
    const uintptr_t isPlayingAddr = m_foundAddresses[1];
    const uintptr_t timeAddr = m_foundAddresses[2];

    while (!m_shouldStop && m_currentState == State::Autoplay) {
        if (!isProcessRunning(process)) {
            QTimer::singleShot(0, this, [this]() {
                m_gameWasClosed = true;
                setState(State::Idle);
                updateStatus("Game was closed");
            });
            m_addressesValid = false;
            break;
        }

        DWORD currentPid = findGameProcessId();
        if (currentPid != m_lastKnownPid) {
            QTimer::singleShot(0, this, [this]() {
                m_gameWasClosed = true;
                setState(State::Idle);
                updateStatus("Game was closed");
            });
            m_addressesValid = false;
            break;
        }

        int isPlaying = 0;
        float time = 0.0f;

        if (readProcessMemory(process, isPlayingAddr, &isPlaying, sizeof(isPlaying)) &&
            readProcessMemory(process, timeAddr, &time, sizeof(time))) {

            int autoplayValue = (isPlaying == 1 && time > 0.0f) ? 1 : 0;
            writeProcessMemory(process, autoplayAddr, &autoplayValue, sizeof(autoplayValue));
        }

        for (int i = 0; i < AUTOPLAY_CHECK_INTERVAL && !m_shouldStop && m_currentState == State::Autoplay; ++i) {
            QThread::msleep(1);
        }
    }

    disableGameAutoplay(process);
    CloseHandle(process);

    if (m_currentState != State::Idle) {
        QTimer::singleShot(0, this, [this]() {
            setState(State::Idle);
            if (!m_gameWasClosed) {
                updateStatus("Made by Amphibi");
            }
        });
    }
}

void MemoryScanner::disableGameAutoplay(HANDLE process)
{
    if (m_addressesValid && m_foundAddresses[0] != 0) {
        int autoplayValue = 0;
        writeProcessMemory(process, m_foundAddresses[0], &autoplayValue, sizeof(autoplayValue));
    }
}
