// GameData.h - Snapshot types and the provider interface that separates "where
// the data comes from" (real process memory vs Demo Mode) from the rest of the
// application. Pure; no OS dependencies.
#pragma once
#include "core/VecMath.h"

#include <cstdint>
#include <string>
#include <vector>

enum class DataSource { None, Live, Demo };

struct EntitySnap {
    uint64_t address = 0;
    Vec3     pos{};
    Vec3     head{};
    float    hp    = 0.f;
    float    maxHp = 1.f;
    int      team  = 0;
    bool     alive   = false;
    bool     visible = true;
    bool     isLocal = false;
    std::string name;
};

struct WorldSnap {
    DataSource source  = DataSource::None;
    bool       valid   = false;     // matrix + local player resolved
    bool       inMatch = true;
    Matrix4    viewMatrix{};
    Vec3       localPos{};
    int        localTeam = -1;
    uint64_t   localAddress = 0;
    std::vector<EntitySnap> entities;
};

struct ProbeResult {
    bool     found = false;
    uint32_t pid   = 0;
};

struct UpdateResult {
    enum class Code {
        Ok,
        NotRunning,         // target process not found
        SdkNotConfigured,   // offsets.ini incomplete
        AttachFailed,       // OpenProcess / module lookup failed
        ModuleMismatch,     // build identity check failed
        ReadFailed,         // required read failed or implausible data
    };
    Code        code = Code::Ok;
    std::string detail;     // human-readable, safe to show in UI/log
    bool ok() const { return code == Code::Ok; }
};

class IGameDataProvider {
public:
    virtual ~IGameDataProvider() = default;
    virtual DataSource  source() const = 0;
    virtual const char* name()   const = 0;

    // Cheap "is the game running?" check. Does not open the process.
    virtual ProbeResult probe() = 0;

    // Produces a fresh snapshot. Only ever called from the worker thread.
    virtual UpdateResult update(WorldSnap& out) = 0;

    // Drop any OS handles. Called when stopping or on shutdown.
    virtual void release() = 0;
};
