#include "memoryscanner.h"

MemoryScanner::MemoryRegionScanThread::MemoryRegionScanThread(
    MemoryScanner* scanner, HANDLE process, const VersionConfig& config,
    uint8_t* startAddr, uint8_t* endAddr, int threadId)
    : m_scanner(scanner), m_process(process), m_config(config),
    m_startAddr(startAddr), m_endAddr(endAddr), m_threadId(threadId)
{
    m_result = {threadId, 0, false};
}

void MemoryScanner::MemoryRegionScanThread::run()
{
    PatternMatcher matcher(m_config.autoplayPattern);
    if (!matcher.isValid()) {
        return;
    }

    MEMORY_BASIC_INFORMATION memInfo;
    uint8_t* address = m_startAddr;
    std::vector<uint8_t> buffer(Constants::MEMORY_CHUNK_SIZE);

    while (address < m_endAddr && VirtualQueryEx(m_process, address, &memInfo, sizeof(memInfo))) {
        if (m_scanner->shouldStop()) {
            return;
        }

        uint8_t* regionStart = static_cast<uint8_t*>(memInfo.BaseAddress);
        if (regionStart >= m_endAddr) {
            break;
        }

        bool isWritable = (memInfo.State == MEM_COMMIT) && !(memInfo.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (memInfo.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

        if (isWritable && !m_result.found) {
            size_t regionSize = memInfo.RegionSize;

            if (regionStart + regionSize > m_endAddr) {
                regionSize = m_endAddr - regionStart;
                if (regionSize == 0) {
                    break;
                }
            }

            size_t offset = 0;
            while (offset < regionSize && !m_scanner->shouldStop()) {
                size_t chunkSize = std::min(Constants::MEMORY_CHUNK_SIZE, regionSize - offset);
                SIZE_T bytesRead;

                if (ReadProcessMemory(m_process, regionStart + offset, buffer.data(), chunkSize, &bytesRead) && bytesRead > 0) {
                    if (!m_result.found && bytesRead >= matcher.getPatternSize()) {
                        size_t foundPos = matcher.search(buffer.data(), bytesRead);
                        if (foundPos != SIZE_MAX) {
                            m_result = {m_threadId, reinterpret_cast<uintptr_t>(regionStart) + offset + foundPos, true};
                            qDebug() << "[LOG] Found pattern at" << Qt::hex << m_result.address;
                            return;
                        }
                    }
                }
                offset += chunkSize;
            }
        }
        address = static_cast<uint8_t*>(memInfo.BaseAddress) + memInfo.RegionSize;
    }
}

void MemoryScanner::WorkerThread::run()
{
    if (m_isAutoplay) {
        m_scanner->runAutoplay();
    } else {
        m_scanner->scanMemory();
    }
}

MemoryScanner::MemoryScanner(QObject *parent)
    : QObject(parent)
    , m_state(State::Idle)
    , m_status("Made by Amphibi")
    , m_gameVersion("Not Detected")
    , m_connectionStatus("")
    , m_shouldStop(false)
    , m_gameWasClosed(false)
    , m_waitingForUpdate(false)
    , m_updateRequested(false)
    , m_lastPid(0)
    , m_addressesValid(false)
    , m_completedScans(0)
    , m_process(nullptr)
    , m_config(std::make_unique<ConfigManager>())
{
    qRegisterMetaType<quintptr>("quintptr");
    m_addresses.fill(0);
    loadConfig();
}

MemoryScanner::~MemoryScanner()
{
    stop();
    cleanup();
}

bool MemoryScanner::isScanning() const
{
    return m_state == State::Scanning;
}

bool MemoryScanner::inAutoplay() const
{
    return m_state == State::Autoplay;
}

QString MemoryScanner::statusText() const
{
    return m_status;
}

QString MemoryScanner::gameVersion() const
{
    return m_gameVersion;
}

QString MemoryScanner::connectionStatus() const
{
    return m_connectionStatus;
}

bool MemoryScanner::shouldStop() const
{
    return m_shouldStop;
}

void MemoryScanner::reloadConfig()
{
    loadConfig();
}

void MemoryScanner::loadConfig()
{
    QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath(Constants::CONFIG_FILENAME);

    if (!QFile::exists(configPath)) {
        qDebug() << "[LOG] Config file does not exist:" << configPath;
        updateConnectionStatus("Config file not found");
        return;
    }

    if (m_config->loadFromFile(configPath)) {
        qDebug() << "[LOG] Successfully loaded configurations";
    } else {
        qDebug() << "[ERROR] Failed to load configuration:" << m_config->getLastError();
        updateConnectionStatus("Config load failed");
    }
}

void MemoryScanner::toggle()
{
    switch (m_state) {
    case State::Idle:
        if (!m_updateRequested) {
            m_updateRequested = true;
            m_waitingForUpdate = true;
            emit updateCheckStarted();
            return;
        }

        loadConfig();
        m_gameWasClosed = false;

        if (m_addressesValid) {
            DWORD currentPid = ProcessManager::getProcessId(Constants::GAME_PROCESS_NAME);
            if (currentPid != 0 && currentPid == m_lastPid) {
                startAutoplay();
                return;
            } else {
                m_addressesValid = false;
                m_addresses.fill(0);
            }
        }
        startScan();
        break;

    case State::Scanning:
    case State::Autoplay:
        stop();
        break;
    }
}

void MemoryScanner::onUpdateDone()
{
    if (m_waitingForUpdate && m_state == State::Idle) {
        m_waitingForUpdate = false;
        toggle();
    }
}

void MemoryScanner::setState(State newState)
{
    if (m_state == newState) {
        return;
    }

    State oldState = m_state;
    m_state = newState;

    if ((oldState == State::Scanning) != (newState == State::Scanning)) {
        emit scanningChanged(newState == State::Scanning);
    }

    if ((oldState == State::Autoplay) != (newState == State::Autoplay)) {
        emit inAutoplayChanged(newState == State::Autoplay);
    }
}

void MemoryScanner::updateStatus(const QString& status)
{
    if (m_status != status) {
        m_status = status;
        emit statusTextChanged(status);
    }
}

void MemoryScanner::updateConnectionStatus(const QString& status)
{
    if (m_connectionStatus != status) {
        m_connectionStatus = status;
        emit connectionStatusChanged(status);
    }
}

void MemoryScanner::updateGameVersion(const QString& version)
{
    if (m_gameVersion != version) {
        m_gameVersion = version;
        emit gameVersionChanged(version);
    }
}

void MemoryScanner::startScan()
{
    cleanup();
    setState(State::Scanning);
    m_shouldStop = false;

    m_worker = std::make_unique<WorkerThread>(this, false);
    connect(m_worker.get(), &QThread::finished, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            cleanup();
        });
    });
    m_worker->start();
}

void MemoryScanner::startAutoplay()
{
    cleanup();
    setState(State::Autoplay);
    m_shouldStop = false;

    m_worker = std::make_unique<WorkerThread>(this, true);
    connect(m_worker.get(), &QThread::finished, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            cleanup();
        });
    });
    m_worker->start();
}

void MemoryScanner::stop()
{
    if (m_state == State::Idle) {
        return;
    }

    if (m_state == State::Autoplay) {
        HANDLE process = openProcess();
        if (process) {
            if (m_addressesValid && m_addresses[0] != 0) {
                int autoplayValue = 0;
                SIZE_T bytesWritten;
                WriteProcessMemory(process, reinterpret_cast<LPVOID>(m_addresses[0]),
                    &autoplayValue, sizeof(autoplayValue), &bytesWritten);
            }
            CloseHandle(process);
        }
    }

    m_shouldStop = true;
    setState(State::Idle);

    m_updateRequested = false;
    m_waitingForUpdate = false;

    if (!m_gameWasClosed) {
        updateStatus("Made by Amphibi");
    }

    for (auto& thread : m_scanThreads) {
        if (thread && thread->isRunning()) {
            thread->quit();
            if (!thread->wait(Constants::THREAD_QUIT_TIMEOUT)) {
                thread->terminate();
                thread->wait(Constants::THREAD_TERMINATE_TIMEOUT);
            }
        }
    }
}

void MemoryScanner::cleanup()
{
    if (m_worker) {
        if (m_worker->isRunning()) {
            m_shouldStop = true;
            m_worker->quit();
            if (!m_worker->wait(Constants::THREAD_QUIT_TIMEOUT)) {
                m_worker->terminate();
                m_worker->wait(Constants::THREAD_TERMINATE_TIMEOUT);
            }
        }
        m_worker.reset();
    }
}

HANDLE MemoryScanner::openProcess() const
{
    DWORD pid = ProcessManager::getProcessId(Constants::GAME_PROCESS_NAME);
    if (pid == 0) {
        return nullptr;
    }
    return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
}

void MemoryScanner::scanMemory()
{
    m_process = openProcess();
    if (!m_process) {
        QTimer::singleShot(0, this, [this]() {
            updateStatus("Game not found");
            updateGameVersion("Not Detected");
            setState(State::Idle);
        });
        return;
    }

    m_lastPid = ProcessManager::getProcessId(Constants::GAME_PROCESS_NAME);
    m_gameWasClosed = false;

    QTimer::singleShot(0, this, [this]() {
        updateStatus("Getting game version");
    });

    QString processVersion = ProcessManager::computeProcessMD5(m_lastPid);
    qDebug() << "[LOG] Process MD5:" << processVersion;

    if (processVersion.isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            updateStatus("Failed to get version");
            setState(State::Idle);
        });
        CloseHandle(m_process);
        m_process = nullptr;
        return;
    }

    auto config = m_config->getVersionConfig(processVersion);
    if (!config.has_value()) {
        QTimer::singleShot(0, this, [this]() {
            updateStatus("Version isn't supported");
            setState(State::Idle);
        });
        CloseHandle(m_process);
        m_process = nullptr;
        return;
    }

    m_currentConfig = config.value();

    QTimer::singleShot(0, this, [this]() {
        updateGameVersion(m_currentConfig.displayName);
        updateStatus("Starting parallel scan...");
    });

    if (m_shouldStop) {
        CloseHandle(m_process);
        m_process = nullptr;
        return;
    }

    parallelScan(m_currentConfig);
}

void MemoryScanner::parallelScan(const VersionConfig& config)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uint8_t* memStart = static_cast<uint8_t*>(sysInfo.lpMinimumApplicationAddress);
    uint8_t* memEnd = static_cast<uint8_t*>(sysInfo.lpMaximumApplicationAddress);

    uintptr_t totalRange = reinterpret_cast<uintptr_t>(memEnd) - reinterpret_cast<uintptr_t>(memStart);
    uintptr_t regionSize = totalRange / Constants::NUM_SEARCH_THREADS;

    m_scanThreads.clear();
    m_scanThreads.reserve(Constants::NUM_SEARCH_THREADS);
    m_completedScans = 0;
    m_addresses.fill(0);

    for (int i = 0; i < Constants::NUM_SEARCH_THREADS; ++i) {
        uint8_t* start = memStart + (i * regionSize);
        uint8_t* end = (i == Constants::NUM_SEARCH_THREADS - 1) ? memEnd : start + regionSize;

        auto thread = std::make_unique<MemoryRegionScanThread>(this, m_process, config, start, end, i);
        connect(thread.get(), &QThread::finished, this, [this, thread = thread.get()]() {
            regionComplete(thread, m_currentConfig);
        });

        thread->start();
        m_scanThreads.push_back(std::move(thread));
    }
}

void MemoryScanner::regionComplete(MemoryRegionScanThread* completedThread, const VersionConfig& config)
{
    QMutexLocker locker(&m_mutex);

    m_completedScans++;
    auto result = completedThread->getResult();

    if (result.found && m_addresses[0] == 0) {
        m_addresses[0] = result.address;
        m_addresses[1] = result.address - config.isPlayingOffset;
        m_addresses[2] = result.address - config.timeOffset;

        qDebug() << "[LOG] Found addresses:"
            << "Autoplay:" << Qt::hex << m_addresses[0]
            << "IsPlaying:" << Qt::hex << m_addresses[1]
            << "Time:" << Qt::hex << m_addresses[2];
    }

    if (m_completedScans >= Constants::NUM_SEARCH_THREADS) {
        QTimer::singleShot(0, this, [this]() {
            allRegionsComplete();
        });
    }
}

void MemoryScanner::allRegionsComplete()
{
    for (auto& thread : m_scanThreads) {
        if (thread && thread->isRunning()) {
            thread->quit();
            thread->wait(Constants::THREAD_TERMINATE_TIMEOUT);
        }
    }
    m_scanThreads.clear();

    if (m_process) {
        CloseHandle(m_process);
        m_process = nullptr;
    }

    if (m_shouldStop) {
        setState(State::Idle);
        updateStatus("Made by Amphibi");
        return;
    }

    DWORD currentPid = ProcessManager::getProcessId(Constants::GAME_PROCESS_NAME);
    if (currentPid == 0 || currentPid != m_lastPid) {
        m_addressesValid = false;
        m_gameWasClosed = true;
        setState(State::Idle);
        updateStatus("Game was closed");
        return;
    }

    if (m_addresses[0] != 0) {
        qDebug() << "[LOG] Pattern scanning completed successfully";
        m_addressesValid = true;

        setState(State::Autoplay);
        m_worker = std::make_unique<WorkerThread>(this, true);
        connect(m_worker.get(), &QThread::finished, this, [this]() {
            QTimer::singleShot(0, this, [this]() {
                cleanup();
            });
        });
        m_worker->start();
    } else {
        qDebug() << "[LOG] Pattern not found in any memory region";
        m_addressesValid = false;
        setState(State::Idle);
        updateStatus("Addresses not found");
    }
}

void MemoryScanner::runAutoplay()
{
    HANDLE process = openProcess();
    if (!process) {
        QTimer::singleShot(0, this, [this]() {
            setState(State::Idle);
            updateStatus("Game not found");
            updateGameVersion("Not Detected");
        });
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        updateStatus("Autoplay is active");
    });

    const uintptr_t autoplayAddr = m_addresses[0];
    const uintptr_t isPlayingAddr = m_addresses[1];
    const uintptr_t timeAddr = m_addresses[2];

    uint8_t prev_isPlaying = 0;

    while (!m_shouldStop && m_state == State::Autoplay) {
        if (!ProcessManager::isProcessRunning(process)) {
            QTimer::singleShot(0, this, [this]() {
                m_gameWasClosed = true;
                setState(State::Idle);
                updateStatus("Game was closed");
                updateGameVersion("Not Detected");
            });
            m_addressesValid = false;
            break;
        }

        DWORD currentPid = ProcessManager::getProcessId(Constants::GAME_PROCESS_NAME);
        if (currentPid != m_lastPid) {
            QTimer::singleShot(0, this, [this]() {
                m_gameWasClosed = true;
                setState(State::Idle);
                updateStatus("Game was closed");
            });
            m_addressesValid = false;
            break;
        }

        uint8_t isPlaying = 0;
        double time = 0.0;

        SIZE_T bytesRead;
        if (ReadProcessMemory(process, reinterpret_cast<LPCVOID>(isPlayingAddr), &isPlaying,
            sizeof(isPlaying), &bytesRead) && bytesRead == sizeof(isPlaying) &&
            ReadProcessMemory(process, reinterpret_cast<LPCVOID>(timeAddr), &time,
            sizeof(time), &bytesRead) && bytesRead == sizeof(time)) {

            int autoplayValue = 0;
            if (m_gameVersion == "1.311") {
                if (isPlaying == 1 && prev_isPlaying == 0) {
                    QThread::msleep(550);
                }
                autoplayValue = (isPlaying == 1);
            } else {
                autoplayValue = (isPlaying == 1 && time > 0.0);
            }

            SIZE_T bytesWritten;
            WriteProcessMemory(process, reinterpret_cast<LPVOID>(autoplayAddr),
                &autoplayValue, sizeof(autoplayValue), &bytesWritten);
        }

        for (int i = 0; i < Constants::AUTOPLAY_CHECK_INTERVAL && !m_shouldStop && m_state == State::Autoplay; ++i) {
            QThread::msleep(1);
        }

        prev_isPlaying = isPlaying;
    }

    if (m_addressesValid && m_addresses[0] != 0) {
        int autoplayValue = 0;
        SIZE_T bytesWritten;
        WriteProcessMemory(process, reinterpret_cast<LPVOID>(m_addresses[0]),
            &autoplayValue, sizeof(autoplayValue), &bytesWritten);

        CloseHandle(process);

        if (m_state != State::Idle) {
            QTimer::singleShot(0, this, [this]() {
                setState(State::Idle);
                if (!m_gameWasClosed) {
                    updateStatus("Made by Amphibi");
                }
            });
        }
    }
}
