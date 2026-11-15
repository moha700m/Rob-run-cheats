// Process.h - External process attach + memory read helpers (Windows).
//
// Read-only: the handle is opened with PROCESS_VM_READ | PROCESS_QUERY_INFORMATION
// only. Nothing is ever written to, allocated in or injected into the target.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

class Process {
public:
    Process() = default;
    ~Process() { detach(); }
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // PID of the first process whose exe name matches (case-insensitive), or 0.
    static DWORD findPid(const wchar_t* exeName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        DWORD pid = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, exeName) == 0) { pid = pe.th32ProcessID; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

    // Opens the process and resolves the main module. On failure returns false
    // and fills `error` (if given) with a readable reason.
    bool attach(const wchar_t* exeName, std::string* error = nullptr) {
        detach();
        const DWORD pid = findPid(exeName);
        if (!pid) { setError(error, "process not found"); return false; }

        HANDLE h = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!h) {
            const DWORD e = GetLastError();
            setError(error, "OpenProcess failed (error " + std::to_string(e) + ")" +
                            (e == ERROR_ACCESS_DENIED ? " - access denied; try running as administrator" : ""));
            return false;
        }
        pid_ = pid;
        handle_ = h;
        if (!resolveModule(exeName, moduleBase_, moduleSize_)) {
            setError(error, "main module not found in process");
            detach();
            return false;
        }
        return true;
    }

    void detach() {
        if (handle_) { CloseHandle(handle_); handle_ = nullptr; }
        pid_ = 0; moduleBase_ = 0; moduleSize_ = 0;
    }

    bool      isAttached() const { return handle_ != nullptr; }
    DWORD     pid()        const { return pid_; }
    HANDLE    handle()     const { return handle_; }
    uintptr_t base()       const { return moduleBase_; }
    size_t    size()       const { return moduleSize_; }

    bool stillAlive() const {
        if (!handle_) return false;
        DWORD code = 0;
        if (!GetExitCodeProcess(handle_, &code)) return false;
        return code == STILL_ACTIVE;
    }

    template <typename T>
    bool read(uintptr_t address, T& out) const {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return readRaw(address, &out, sizeof(T));
    }

    template <typename T>
    T readOr(uintptr_t address, T fallback = {}) const {
        T v{};
        return read(address, v) ? v : fallback;
    }

    bool readRaw(uintptr_t address, void* buffer, size_t bytes) const {
        if (!handle_ || !address || !buffer || !bytes) return false;
        SIZE_T n = 0;
        return ReadProcessMemory(handle_, reinterpret_cast<LPCVOID>(address), buffer, bytes, &n) && n == bytes;
    }

    // Reads up to maxLen chars, stops at the first NUL. Safe against unreadable memory.
    std::string readInlineString(uintptr_t address, size_t maxLen = 32) const {
        if (!handle_ || !address || !maxLen) return {};
        std::vector<char> buf(maxLen + 1, 0);
        if (!readRaw(address, buf.data(), maxLen)) return {};
        buf[maxLen] = '\0';
        return std::string(buf.data());
    }

private:
    static void setError(std::string* e, std::string msg) { if (e) *e = std::move(msg); }

    bool resolveModule(const wchar_t* moduleName, uintptr_t& base, size_t& size) const {
        base = 0; size = 0;
        HMODULE mods[1024];
        DWORD needed = 0;
        if (!EnumProcessModulesEx(handle_, mods, sizeof(mods), &needed, LIST_MODULES_ALL)) return false;
        const DWORD count = std::min<DWORD>(needed / sizeof(HMODULE), 1024);
        for (DWORD i = 0; i < count; ++i) {
            wchar_t name[MAX_PATH]{};
            if (!GetModuleBaseNameW(handle_, mods[i], name, MAX_PATH)) continue;
            if (_wcsicmp(name, moduleName) == 0) {
                MODULEINFO info{};
                if (GetModuleInformation(handle_, mods[i], &info, sizeof(info))) {
                    base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                    size = info.SizeOfImage;
                    return true;
                }
            }
        }
        return false;
    }

    DWORD     pid_        = 0;
    HANDLE    handle_     = nullptr;
    uintptr_t moduleBase_ = 0;
    size_t    moduleSize_ = 0;
};
