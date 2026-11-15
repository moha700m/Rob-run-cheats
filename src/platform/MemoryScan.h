// MemoryScan.h - Signature scan over the attached process's main module.
// Built on the pure matcher in core/Signature.h. Not used by the default flow
// (offsets come from offsets.ini); available for resolving RVAs at runtime once
// you have a stable signature from your own disassembly.
#pragma once
#include "Process.h"
#include "core/Signature.h"

#include <algorithm>
#include <vector>

namespace mem {

    // Returns the absolute address of the first match inside the main module, or 0.
    inline uintptr_t findPattern(const Process& proc, const std::string& signature,
                                 size_t chunkSize = 0x10000) {
        if (!proc.isAttached() || !proc.base() || !proc.size()) return 0;
        const auto pat = sig::parse(signature);
        if (!pat) return 0;
        if (chunkSize < pat->size() * 2) chunkSize = pat->size() * 2;

        std::vector<uint8_t> buf(chunkSize);
        const uintptr_t start = proc.base();
        const uintptr_t end   = start + proc.size();
        const size_t overlap  = pat->size() - 1;

        for (uintptr_t cur = start; cur < end;) {
            const size_t want = static_cast<size_t>(std::min<uintptr_t>(chunkSize, end - cur));
            if (proc.readRaw(cur, buf.data(), want)) {
                if (auto hit = sig::find(buf.data(), want, *pat)) return cur + *hit;
            }
            if (want <= overlap) break;
            cur += want - overlap;      // overlap chunks so matches spanning a boundary are found
        }
        return 0;
    }

    // Resolves a RIP-relative operand: target = insn + insnLen + rel32 at (insn + insnOffset).
    inline uintptr_t resolveRip(const Process& proc, uintptr_t insnAddr, size_t insnOffset, size_t insnLen) {
        int32_t rel = 0;
        if (!proc.read<int32_t>(insnAddr + insnOffset, rel)) return 0;
        return static_cast<uintptr_t>(sig::ripTarget(insnAddr, insnLen, rel));
    }

} // namespace mem
