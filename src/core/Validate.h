// Validate.h - Plausibility checks applied to everything read from another
// process before it is used. Pure functions (no OS dependencies).
#pragma once
#include "VecMath.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace validate {

    inline bool finite(float v) { return std::isfinite(v); }

    inline bool finite(const Vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    // User-mode canonical x64 pointer: not null, not in the first 64 KiB, and
    // below the 47-bit user address-space limit.
    inline bool plausiblePointer(uint64_t p) {
        return p >= 0x10000ull && p < 0x00007FFFFFFF0000ull;
    }

    inline bool plausibleWorldPos(const Vec3& v, float limit = 1.0e6f) {
        return finite(v) && std::fabs(v.x) <= limit && std::fabs(v.y) <= limit &&
               std::fabs(v.z) <= limit;
    }

    inline bool plausibleHealth(float hp, float maxHp) {
        return finite(hp) && finite(maxHp) && hp >= -1.0e4f && hp <= 1.0e7f &&
               maxHp > 0.0f && maxHp <= 1.0e7f;
    }

    inline bool viewMatrixUsable(const Matrix4& m) {
        bool anyNonZero = false;
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) {
                if (!std::isfinite(m.m[r][c])) return false;
                if (m.m[r][c] != 0.0f) anyNonZero = true;
            }
        return anyNonZero;
    }

    inline bool printableAscii(const std::string& s) {
        for (unsigned char c : s)
            if (c < 0x20 || c > 0x7E) return false;
        return true;
    }

    inline bool inRange(uint64_t addr, uint64_t base, uint64_t size) {
        return addr >= base && addr - base < size;
    }

} // namespace validate
