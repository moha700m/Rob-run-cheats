#include "Keys.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace keys {
namespace {

    struct Named { const char* name; int vk; };

    // Values are the documented Win32 VK_* constants (winuser.h).
    constexpr Named kNamed[] = {
        {"BACKSPACE", 0x08}, {"TAB", 0x09}, {"ENTER", 0x0D}, {"PAUSE", 0x13},
        {"CAPSLOCK", 0x14},  {"SPACE", 0x20}, {"PAGEUP", 0x21}, {"PAGEDOWN", 0x22},
        {"END", 0x23},       {"HOME", 0x24}, {"LEFT", 0x25}, {"UP", 0x26},
        {"RIGHT", 0x27},     {"DOWN", 0x28}, {"INSERT", 0x2D}, {"DELETE", 0x2E},
        {"NUMLOCK", 0x90},   {"SCROLLLOCK", 0x91},
        {"MULTIPLY", 0x6A},  {"ADD", 0x6B}, {"SUBTRACT", 0x6D}, {"DECIMAL", 0x6E},
        {"DIVIDE", 0x6F},
    };

    std::string upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }

} // namespace

std::optional<int> vkFromName(const std::string& nameIn) {
    const std::string n = upper(nameIn);
    if (n.empty()) return std::nullopt;

    if (n.size() == 1) {
        const char c = n[0];
        if (c >= 'A' && c <= 'Z') return static_cast<int>(c);   // VK_A..VK_Z == ASCII
        if (c >= '0' && c <= '9') return static_cast<int>(c);   // VK_0..VK_9 == ASCII
    }
    if (n[0] == 'F' && n.size() >= 2 && n.size() <= 3 &&
        std::all_of(n.begin() + 1, n.end(), [](unsigned char c) { return std::isdigit(c); })) {
        const int num = std::atoi(n.c_str() + 1);
        if (num >= 1 && num <= 24) return 0x70 + (num - 1);     // VK_F1 = 0x70
    }
    if (n.rfind("NUMPAD", 0) == 0 && n.size() == 7 && std::isdigit(static_cast<unsigned char>(n[6])))
        return 0x60 + (n[6] - '0');                              // VK_NUMPAD0 = 0x60

    for (const auto& k : kNamed)
        if (n == k.name) return k.vk;
    return std::nullopt;
}

std::string nameFromVk(int vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, static_cast<char>(vk));
    if (vk >= 0x70 && vk <= 0x87) return "F" + std::to_string(vk - 0x70 + 1);
    if (vk >= 0x60 && vk <= 0x69) return "NUMPAD" + std::to_string(vk - 0x60);
    for (const auto& k : kNamed)
        if (k.vk == vk) return k.name;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "VK_0x%02X", vk & 0xFF);
    return buf;
}

bool isBindable(int vk) {
    // Exclude mouse buttons (0x01-0x06), modifiers/menu (0x10-0x12) and ESC (0x1B,
    // reserved for cancelling a rebind).
    if (vk <= 0 || vk > 0xFE) return false;
    if (vk >= 0x01 && vk <= 0x06) return false;
    if (vk >= 0x10 && vk <= 0x12) return false;
    if (vk == 0x1B) return false;
    return vkFromName(nameFromVk(vk)).has_value();
}

std::vector<std::pair<int, int>> findDuplicates(const std::vector<int>& codes) {
    std::vector<std::pair<int, int>> out;
    for (size_t i = 0; i < codes.size(); ++i)
        for (size_t j = i + 1; j < codes.size(); ++j)
            if (codes[i] != 0 && codes[i] == codes[j])
                out.emplace_back(static_cast<int>(i), static_cast<int>(j));
    return out;
}

} // namespace keys
