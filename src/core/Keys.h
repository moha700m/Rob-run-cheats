// Keys.h - Virtual-key code <-> name table used by the hotkey settings, plus an
// edge detector that is independent of the OS (key state is injected).
#pragma once
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace keys {

    // Accepts "F8", "INSERT", "A", "NUMPAD3", ... (case-insensitive).
    std::optional<int> vkFromName(const std::string& name);

    // Canonical name, or "VK_0x.." when the code is not in the table.
    std::string nameFromVk(int vk);

    bool isBindable(int vk);

    // Returns pairs of indexes whose (non-zero) codes collide.
    std::vector<std::pair<int, int>> findDuplicates(const std::vector<int>& codes);

    // Detects released->pressed transitions. `down` is injected so this works
    // without GetAsyncKeyState.
    class EdgeTracker {
    public:
        using DownFn = std::function<bool(int vk)>;
        explicit EdgeTracker(DownFn down) : down_(std::move(down)) {}

        bool pressed(int vk) {
            if (vk <= 0 || vk > 255) return false;
            const bool now  = down_(vk);
            const bool edge = now && !prev_[static_cast<size_t>(vk)];
            prev_[static_cast<size_t>(vk)] = now;
            return edge;
        }
        void reset() { prev_.fill(false); }

    private:
        DownFn down_;
        std::array<bool, 256> prev_{};
    };

} // namespace keys
