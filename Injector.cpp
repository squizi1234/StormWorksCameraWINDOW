#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

// Функция поиска процесса по точному имени
DWORD GetProcessByName(const char* targetName) {
    DWORD pid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe)) {
        do {
            // Преобразуем WCHAR в char для сравнения
            char currentName[MAX_PATH];
            wcstombs(currentName, pe.szExeFile, MAX_PATH);
            if (_stricmp(currentName, targetName) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return pid;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("[USAGE] inject.exe <FullDllPath> <ProcessName>\n");
        system("pause");
        return -1;
    }

    const char* dllPath = argv[1];
    const char* processName = argv[2];

    DWORD pid = GetProcessByName(processName);
    if (pid == 0) {
        printf("Process '%s' not found.\n", processName);
        return -1;
    }

    printf("[INFO] Injecting %s into %s (PID: %d)\n", dllPath, processName, pid);

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[ERROR] OpenProcess failed (Run as Admin!). Error: %d\n", GetLastError());
        return -1;
    }

    // Выделение памяти под путь к DLL
    LPVOID remotePath = VirtualAllocEx(hProcess, nullptr, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProcess, remotePath, dllPath, strlen(dllPath) + 1, nullptr);

    // Получение адреса LoadLibraryA
    LPVOID loadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    // Запуск потока
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibrary, remotePath, 0, nullptr);

    if (hThread) {
        printf("[SUCCESS] DLL Injected successfully.\n");
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    else {
        printf("[ERROR] CreateRemoteThread failed. Error: %d\n", GetLastError());
    }

    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return 0;
}