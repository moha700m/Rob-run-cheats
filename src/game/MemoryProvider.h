// MemoryProvider.h - Live game data via read-only ReadProcessMemory.
//
// Everything it reads is described by SdkOffsets (offsets.ini). With incomplete
// offsets it refuses to read anything and reports SdkNotConfigured - it never
// falls back to guessed values.
#pragma once
#include "GameData.h"
#include "SdkOffsets.h"
#include "platform/Process.h"

#include <chrono>
#include <mutex>
#include <string>

class MemoryProvider final : public IGameDataProvider {
public:
    MemoryProvider() = default;

    // Thread-safe; takes effect on the next update()/probe().
    void configure(const std::wstring& processName, const SdkOffsets& offsets);

    DataSource  source() const override { return DataSource::Live; }
    const char* name()   const override { return "Process memory"; }
    ProbeResult probe() override;
    UpdateResult update(WorldSnap& out) override;
    void release() override;

private:
    struct Config { std::wstring processName = L"RobRun.exe"; SdkOffsets offsets; };

    Config snapshotConfig();
    UpdateResult readWorld(const SdkOffsets& o, WorldSnap& out);

    std::mutex cfgMtx_;
    Config     cfg_;
    bool       cfgDirty_ = true;

    Process    proc_;                       // worker thread only
    Config     active_;                     // worker thread only
    std::chrono::steady_clock::time_point lastFind_{}, lastAttach_{};
    bool       knownRunning_ = false;
    bool       warnedNoModuleSize_ = false;
    std::string lastAttachError_;
};
