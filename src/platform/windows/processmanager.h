#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

// Qt includes
#include <QString>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>

// System includes
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winver.h>

class ProcessHandle
{
public:
    ProcessHandle() : m_handle(nullptr) {}
    explicit ProcessHandle(HANDLE handle) : m_handle(handle) {}
    ~ProcessHandle() { close(); }

    ProcessHandle(ProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    ProcessHandle& operator=(ProcessHandle&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;

    HANDLE get() const { return m_handle; }
    bool isValid() const { return m_handle != nullptr; }
    operator bool() const { return isValid(); }

    void close() {
        if (m_handle) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

class ProcessManager
{
public:
    static DWORD getProcessId(const QString& processName);
    static ProcessHandle openProcess(const QString& processName, DWORD accessRights);
    static bool isProcessRunning(HANDLE processHandle);
    static bool readMemory(HANDLE process, uintptr_t address, void* buffer, size_t size);
    static bool writeMemory(HANDLE process, uintptr_t address, const void* buffer, size_t size);
    static void getSystemMemoryLimits(uint8_t*& minAddress, uint8_t*& maxAddress);
    static QString computeProcessMD5(DWORD pid);

private:
    ProcessManager() = delete;
};

#endif // PROCESSMANAGER_H
