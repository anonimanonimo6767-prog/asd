#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

class Memory {
public:
    HANDLE hProcess = nullptr;
    uintptr_t base  = 0;

    bool Attach(const std::wstring& procName) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        while (Process32NextW(snap, &pe)) {
            if (procName == pe.szExeFile) {
                DWORD pid = pe.th32ProcessID;
                CloseHandle(snap);
                hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
                base = GetBase(pid, procName);
                return hProcess != nullptr;
            }
        }
        CloseHandle(snap);
        return false;
    }

    template<typename T>
    T Read(uintptr_t addr) {
        T val{};
        ReadProcessMemory(hProcess, (LPCVOID)addr, &val, sizeof(T), nullptr);
        return val;
    }

    std::string ReadString(uintptr_t addr, size_t len = 64) {
        std::string s(len, '\0');
        ReadProcessMemory(hProcess, (LPCVOID)addr, s.data(), len, nullptr);
        s.erase(s.find('\0'));
        return s;
    }

private:
    uintptr_t GetBase(DWORD pid, const std::wstring& name) {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        while (Module32NextW(snap, &me)) {
            if (name == me.szModule) {
                CloseHandle(snap);
                return (uintptr_t)me.modBaseAddr;
            }
        }
        CloseHandle(snap);
        return 0;
    }
};
