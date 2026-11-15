// SdkOffsets.h - Build-specific RobRun layout values, loaded at runtime from
// offsets.ini (see docs/OFFSETS.md and offsets.template.ini).
//
// IMPORTANT: this repository ships NO real offsets. Every value is optional and
// "unset" (std::nullopt) is distinct from a real 0. Nothing here was dumped
// from a RobRun build by this project; the user supplies them.
//
// This file replaces the old compile-time SDK.h (all of whose values were the
// placeholder 0x0). Runtime loading lets offsets be changed without rebuilding
// and lets the app validate them against the running module.
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct SdkOffsets {
    // Provenance, free text shown in the UI/log (e.g. "RobRun 1.2.3 dumped 2026-11-01 by <you>").
    std::string buildLabel;

    // If set, attaching fails unless the module's SizeOfImage matches exactly.
    std::optional<uint64_t> expectedModuleSize;

    // Module-relative (RVA) values: moduleBase + value.
    std::optional<uint64_t> localPlayer;   // location of a pointer to the local player struct
    std::optional<uint64_t> entityList;    // location of a pointer to the entity-pointer array
    std::optional<uint64_t> entityCount;   // optional int32 count
    std::optional<uint64_t> viewMatrix;    // 4x4 float, row-major
    std::optional<uint64_t> gameState;     // optional int32, non-zero = in match

    // Per-entity offsets (relative to the entity struct).
    std::optional<uint64_t> position;      // 3 x float (x,y,z), Z-up
    std::optional<uint64_t> health;
    std::optional<uint64_t> maxHealth;
    std::optional<uint64_t> team;          // int32
    std::optional<uint64_t> isAlive;
    std::optional<uint64_t> name;
    std::optional<uint64_t> headPosition;  // optional 3 x float
    std::optional<uint64_t> visible;       // optional
    std::optional<uint64_t> isLocalFlag;   // optional; otherwise compared with the local player pointer
    std::optional<uint64_t> rotation;      // reserved, currently unused

    // Layout options.
    bool     healthIsInt32    = false;   // false: float, true: int32
    uint32_t isAliveSize      = 4;       // 1 or 4 bytes
    bool     nameIsPointer    = false;   // false: inline char[nameMaxLen]
    uint32_t nameMaxLen       = 32;
    uint32_t entityStride     = 8;       // bytes between entity-pointer slots
    uint32_t maxEntities      = 128;     // used when entityCount is unset/invalid

    // Names (as written in offsets.ini) of required values that are not set.
    std::vector<std::string> missingRequired() const;
    bool ready() const { return missingRequired().empty(); }
};

namespace offsets {

    struct LoadResult {
        bool fileFound = false;
        bool ok        = true;
        std::vector<std::string> warnings;
    };

    LoadResult parse(SdkOffsets& out, std::string_view text);
    LoadResult load(SdkOffsets& out, const std::filesystem::path& path);

    // Sanity limits for values that came from a file (does not need a process).
    std::vector<std::string> checkLimits(const SdkOffsets& o);

    // Content of offsets.template.ini (every value commented out / unset).
    std::string templateText();

} // namespace offsets
