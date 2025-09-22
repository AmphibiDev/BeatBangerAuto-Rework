#include "processmanager.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <winver.h>

#pragma comment(lib, "version.lib")

DWORD ProcessManager::findProcessId(const QString& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        qDebug() << "[ERROR] Failed to create process snapshot";
        return 0;
    }

    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
    DWORD pid = 0;

    std::wstring wideProcessName = processName.toStdWString();

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, wideProcessName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

ProcessHandle ProcessManager::openProcess(const QString& processName, DWORD accessRights)
{
    DWORD pid = findProcessId(processName);
    if (pid == 0) {
        return ProcessHandle();
    }

    HANDLE handle = OpenProcess(accessRights, FALSE, pid);
    if (!handle) {
        qDebug() << "[ERROR] Failed to open process" << processName << "PID:" << pid << "Error:" << GetLastError();
        return ProcessHandle();
    }

    qDebug() << "[LOG] Successfully opened process" << processName << "with handle:" << handle;
    return ProcessHandle(handle);
}

bool ProcessManager::isProcessRunning(HANDLE processHandle)
{
    if (!processHandle) {
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(processHandle, &exitCode)) {
        qDebug() << "[ERROR] Failed to get process exit code, error:" << GetLastError();
        return false;
    }

    return exitCode == STILL_ACTIVE;
}

bool ProcessManager::readMemory(HANDLE process, uintptr_t address, void* buffer, size_t size)
{
    if (!process || !buffer || size == 0) {
        return false;
    }

    SIZE_T bytesRead;
    bool success = ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead);

    if (!success) {
        return false;
    }

    return bytesRead == size;
}

bool ProcessManager::writeMemory(HANDLE process, uintptr_t address, const void* buffer, size_t size)
{
    if (!process || !buffer || size == 0) {
        return false;
    }

    SIZE_T bytesWritten;
    bool success = WriteProcessMemory(process, reinterpret_cast<LPVOID>(address),
                                      buffer, size, &bytesWritten);

    if (!success) {
        qDebug() << "[ERROR] Failed to write memory at address" << Qt::hex << address << "Error:" << GetLastError();
        return false;
    }

    return bytesWritten == size;
}

void ProcessManager::getSystemMemoryLimits(uint8_t*& minAddress, uint8_t*& maxAddress)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    minAddress = static_cast<uint8_t*>(sysInfo.lpMinimumApplicationAddress);
    maxAddress = static_cast<uint8_t*>(sysInfo.lpMaximumApplicationAddress);
}

QString ProcessManager::getProcessMD5(DWORD pid) {
    if (pid == 0) return QString();

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return QString();

    HMODULE hMod;
    DWORD cbNeeded;
    QString hashResult;

    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
        wchar_t modulePath[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, hMod, modulePath, MAX_PATH)) {
            QFile file(QString::fromWCharArray(modulePath));
            if (file.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Md5);
                if (hash.addData(&file)) {
                    hashResult = hash.result().toHex();
                }
                file.close();
            }
        }
    }

    CloseHandle(hProcess);
    return hashResult;
}
