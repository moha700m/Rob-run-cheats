#include "SdkOffsets.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

std::vector<std::string> SdkOffsets::missingRequired() const {
    std::vector<std::string> m;
    if (!viewMatrix)  m.emplace_back("rva.view_matrix");
    if (!localPlayer) m.emplace_back("rva.local_player");
    if (!entityList)  m.emplace_back("rva.entity_list");
    if (!position)    m.emplace_back("entity.position");
    if (!health)      m.emplace_back("entity.health");
    if (!team)        m.emplace_back("entity.team");
    return m;
}

namespace offsets {
namespace {

    std::string_view trim(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
        while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' || s.back()  == '\r')) s.remove_suffix(1);
        return s;
    }

    // "0x1F40", "1F40h" is not accepted on purpose; decimal also allowed.
    bool parseNumber(std::string_view t, uint64_t& out) {
        t = trim(t);
        if (t.empty()) return false;
        int base = 10;
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) { base = 16; t.remove_prefix(2); }
        uint64_t v = 0;
        const auto r = std::from_chars(t.data(), t.data() + t.size(), v, base);
        if (r.ec != std::errc() || r.ptr != t.data() + t.size()) return false;
        out = v;
        return true;
    }

    bool parseBool(std::string_view t, bool& out) {
        std::string l(trim(t));
        std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (l == "1" || l == "true" || l == "yes") { out = true;  return true; }
        if (l == "0" || l == "false" || l == "no") { out = false; return true; }
        return false;
    }

} // namespace

LoadResult parse(SdkOffsets& o, std::string_view text) {
    LoadResult res;
    res.fileFound = true;

    std::map<std::string, std::string> kv;
    size_t pos = 0, lineNo = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        ++lineNo;
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) { res.warnings.push_back("line " + std::to_string(lineNo) + ": missing '='"); continue; }
        kv[std::string(trim(line.substr(0, eq)))] = std::string(trim(line.substr(eq + 1)));
    }

    std::set<std::string> used;
    auto num = [&](const char* k, std::optional<uint64_t>& f) {
        auto it = kv.find(k);
        if (it == kv.end()) return;
        used.insert(k);
        uint64_t v;
        if (parseNumber(it->second, v)) f = v;
        else res.warnings.push_back(std::string("invalid number for '") + k + "': '" + it->second + "' (left unset)");
    };
    auto u32 = [&](const char* k, uint32_t& f) {
        auto it = kv.find(k);
        if (it == kv.end()) return;
        used.insert(k);
        uint64_t v;
        if (parseNumber(it->second, v) && v <= 0xFFFFFFFFull) f = static_cast<uint32_t>(v);
        else res.warnings.push_back(std::string("invalid number for '") + k + "': '" + it->second + "' (keeping default)");
    };
    auto flag = [&](const char* k, bool& f) {
        auto it = kv.find(k);
        if (it == kv.end()) return;
        used.insert(k);
        bool b;
        if (parseBool(it->second, b)) f = b;
        else res.warnings.push_back(std::string("invalid bool for '") + k + "': '" + it->second + "'");
    };

    if (auto it = kv.find("build.label"); it != kv.end()) { o.buildLabel = it->second; used.insert("build.label"); }
    num("build.module_size", o.expectedModuleSize);

    num("rva.local_player", o.localPlayer);
    num("rva.entity_list",  o.entityList);
    num("rva.entity_count", o.entityCount);
    num("rva.view_matrix",  o.viewMatrix);
    num("rva.game_state",   o.gameState);

    num("entity.position",      o.position);
    num("entity.health",        o.health);
    num("entity.max_health",    o.maxHealth);
    num("entity.team",          o.team);
    num("entity.is_alive",      o.isAlive);
    num("entity.name",          o.name);
    num("entity.head_position", o.headPosition);
    num("entity.visible",       o.visible);
    num("entity.is_local_flag", o.isLocalFlag);
    num("entity.rotation",      o.rotation);

    flag("layout.health_is_int32", o.healthIsInt32);
    flag("layout.name_is_pointer", o.nameIsPointer);
    u32 ("layout.is_alive_size",   o.isAliveSize);
    u32 ("layout.name_max_len",    o.nameMaxLen);
    u32 ("layout.entity_stride",   o.entityStride);
    u32 ("layout.max_entities",    o.maxEntities);

    for (const auto& [k, v] : kv)
        if (!used.count(k)) res.warnings.push_back("unknown key '" + k + "' ignored");
    for (auto& w : checkLimits(o)) res.warnings.push_back(std::move(w));
    return res;
}

LoadResult load(SdkOffsets& o, const std::filesystem::path& path) {
    LoadResult res;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return res;   // fileFound = false
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        res.fileFound = true;
        res.ok = false;
        res.warnings.push_back("cannot open " + path.string());
        return res;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse(o, ss.str());
}

std::vector<std::string> checkLimits(const SdkOffsets& o) {
    std::vector<std::string> w;
    auto rva = [&](const char* n, const std::optional<uint64_t>& v) {
        if (v && *v > 0x7FFFFFFFull) w.push_back(std::string(n) + " is larger than 2 GiB - not a plausible RVA");
    };
    rva("rva.local_player", o.localPlayer); rva("rva.entity_list", o.entityList);
    rva("rva.entity_count", o.entityCount); rva("rva.view_matrix", o.viewMatrix);
    rva("rva.game_state", o.gameState);
    auto fld = [&](const char* n, const std::optional<uint64_t>& v) {
        if (v && *v > 0x100000ull) w.push_back(std::string(n) + " is larger than 1 MiB - not a plausible struct offset");
    };
    fld("entity.position", o.position); fld("entity.health", o.health); fld("entity.max_health", o.maxHealth);
    fld("entity.team", o.team); fld("entity.is_alive", o.isAlive); fld("entity.name", o.name);
    fld("entity.head_position", o.headPosition); fld("entity.visible", o.visible);
    fld("entity.is_local_flag", o.isLocalFlag);
    if (o.isAliveSize != 1 && o.isAliveSize != 4) w.push_back("layout.is_alive_size must be 1 or 4");
    if (o.nameMaxLen == 0 || o.nameMaxLen > 256) w.push_back("layout.name_max_len must be 1..256");
    if (o.entityStride < 4 || o.entityStride > 4096) w.push_back("layout.entity_stride must be 4..4096");
    if (o.maxEntities == 0 || o.maxEntities > 4096) w.push_back("layout.max_entities must be 1..4096");
    return w;
}

std::string templateText() {
    return R"(# RobRun offsets - fill in from YOUR OWN dump of the RobRun.exe build you run.
# This project ships no real values. A line that is missing/commented is "unset".
# A real value of zero must be written explicitly (e.g. "entity.team = 0x0").
# Numbers: hex with 0x prefix, or decimal. Module-relative values (rva.*) are
# offsets from the RobRun.exe image base, not absolute addresses.
# See docs/OFFSETS.md for how to find each value.

# --- Build identity (strongly recommended) ---
# build.label       = RobRun <version/date>, dumped by <name>
# build.module_size = 0x00000000     # SizeOfImage of RobRun.exe for THIS build; attach is refused on mismatch

# --- Module-relative (REQUIRED: view_matrix, local_player, entity_list) ---
# rva.view_matrix  =     # 4x4 float, row-major, view*projection
# rva.local_player =     # address holding a POINTER to the local player struct
# rva.entity_list  =     # address holding a POINTER to an array of entity pointers
# rva.entity_count =     # optional int32 (falls back to layout.max_entities)
# rva.game_state   =     # optional int32, non-zero = in a match

# --- Per entity (REQUIRED: position, health, team) ---
# entity.position      =   # 3 x float x,y,z (Z-up)
# entity.health        =
# entity.max_health    =   # optional
# entity.team          =   # int32
# entity.is_alive      =   # optional (otherwise health > 0)
# entity.name          =   # optional
# entity.head_position =   # optional
# entity.visible       =   # optional
# entity.is_local_flag =   # optional

# --- Layout options (defaults shown) ---
# layout.health_is_int32 = false
# layout.is_alive_size   = 4
# layout.name_is_pointer = false
# layout.name_max_len    = 32
# layout.entity_stride   = 8
# layout.max_entities    = 128
)";
}

} // namespace offsets
