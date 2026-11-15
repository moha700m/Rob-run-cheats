// Signature.h - IDA-style byte signatures ("48 8B 05 ? ? ? ?"), parsing and
// matching over plain buffers. Pure; the process-memory scan lives in
// platform/MemoryScan.h.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace sig {

    struct Byte {
        uint8_t value = 0;
        bool    wild  = false;
    };

    // Returns nullopt for an empty signature or any token that is not a one- or
    // two-digit hex byte, "?" or "??".
    inline std::optional<std::vector<Byte>> parse(const std::string& text) {
        std::vector<Byte> out;
        std::istringstream iss(text);
        std::string tok;
        while (iss >> tok) {
            Byte b;
            if (tok == "?" || tok == "??") {
                b.wild = true;
            } else {
                if (tok.size() > 2) return std::nullopt;
                unsigned v = 0;
                for (char c : tok) {
                    v <<= 4;
                    if (c >= '0' && c <= '9')      v |= unsigned(c - '0');
                    else if (c >= 'a' && c <= 'f') v |= unsigned(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') v |= unsigned(c - 'A' + 10);
                    else return std::nullopt;
                }
                b.value = static_cast<uint8_t>(v);
            }
            out.push_back(b);
        }
        if (out.empty()) return std::nullopt;
        return out;
    }

    inline std::optional<size_t> find(const uint8_t* data, size_t size,
                                      const std::vector<Byte>& pat) {
        if (pat.empty() || size < pat.size()) return std::nullopt;
        const size_t last = size - pat.size();
        for (size_t i = 0; i <= last; ++i) {
            bool ok = true;
            for (size_t j = 0; j < pat.size(); ++j) {
                if (!pat[j].wild && data[i + j] != pat[j].value) { ok = false; break; }
            }
            if (ok) return i;
        }
        return std::nullopt;
    }

    // RIP-relative target: insnAddr + insnLen + rel32.
    inline uint64_t ripTarget(uint64_t insnAddr, size_t insnLen, int32_t rel32) {
        return insnAddr + insnLen + static_cast<int64_t>(rel32);
    }

} // namespace sig
