#include "Settings.h"

#include "core/Keys.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

namespace config {
namespace {

    std::string_view trim(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
        while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' || s.back()  == '\r')) s.remove_suffix(1);
        return s;
    }

    std::string fmtFloat(float v) {
        char buf[48];
        const auto r = std::to_chars(buf, buf + sizeof(buf), v);
        return std::string(buf, r.ptr);
    }

    bool parseFloat(std::string_view t, float& out) {
        t = trim(t);
        if (t.empty()) return false;
        float v = 0;
        const auto r = std::from_chars(t.data(), t.data() + t.size(), v);
        if (r.ec != std::errc() || r.ptr != t.data() + t.size() || !std::isfinite(v)) return false;
        out = v;
        return true;
    }

    bool parseInt(std::string_view t, int& out) {
        t = trim(t);
        if (t.empty()) return false;
        int v = 0;
        const auto r = std::from_chars(t.data(), t.data() + t.size(), v);
        if (r.ec != std::errc() || r.ptr != t.data() + t.size()) return false;
        out = v;
        return true;
    }

    bool parseBool(std::string_view t, bool& out) {
        t = trim(t);
        std::string l(t);
        std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (l == "1" || l == "true" || l == "yes" || l == "on")  { out = true;  return true; }
        if (l == "0" || l == "false" || l == "no" || l == "off") { out = false; return true; }
        return false;
    }

    // ---- Reader ----
    struct Reader {
        std::map<std::string, std::string> kv;
        std::set<std::string> used;
        std::vector<std::string>* warnings;

        const std::string* find(const char* k) {
            auto it = kv.find(k);
            if (it == kv.end()) return nullptr;
            used.insert(k);
            return &it->second;
        }
        void bad(const char* k, const std::string& v) {
            warnings->push_back(std::string("invalid value for '") + k + "': '" + v + "' (keeping default)");
        }

        void str(const char* k, std::string& f)  { if (auto* v = find(k)) f = std::string(trim(*v)); }
        void flag(const char* k, bool& f)        { if (auto* v = find(k)) { bool b; if (parseBool(*v, b)) f = b; else bad(k, *v); } }
        void real(const char* k, float& f)       { if (auto* v = find(k)) { float x; if (parseFloat(*v, x)) f = x; else bad(k, *v); } }
        void integer(const char* k, int& f)      { if (auto* v = find(k)) { int x; if (parseInt(*v, x)) f = x; else bad(k, *v); } }
        template <class E> void enum_(const char* k, E& f) {
            if (auto* v = find(k)) { int x; if (parseInt(*v, x)) f = static_cast<E>(x); else bad(k, *v); }
        }
        void color(const char* k, float (&f)[4]) {
            if (auto* v = find(k)) {
                float tmp[4]; std::stringstream ss(*v); std::string part; int i = 0; bool ok = true;
                while (std::getline(ss, part, ',')) { if (i >= 4 || !parseFloat(part, tmp[i])) { ok = false; break; } ++i; }
                if (ok && i == 4) for (int j = 0; j < 4; ++j) f[j] = tmp[j]; else bad(k, *v);
            }
        }
        void key(const char* k, int& f) {
            if (auto* v = find(k)) {
                const auto vk = keys::vkFromName(std::string(trim(*v)));
                if (vk) f = *vk; else bad(k, *v);
            }
        }
    };

    // ---- Writer ----
    struct Writer {
        std::ostringstream out;
        void line(const char* k, const std::string& v) { out << k << " = " << v << "\n"; }
        void str(const char* k, const std::string& f)  { line(k, f); }
        void flag(const char* k, bool f)               { line(k, f ? "true" : "false"); }
        void real(const char* k, float f)              { line(k, fmtFloat(f)); }
        void integer(const char* k, int f)             { line(k, std::to_string(f)); }
        template <class E> void enum_(const char* k, E f) { line(k, std::to_string(static_cast<int>(f))); }
        void color(const char* k, const float (&f)[4]) {
            line(k, fmtFloat(f[0]) + ", " + fmtFloat(f[1]) + ", " + fmtFloat(f[2]) + ", " + fmtFloat(f[3]));
        }
        void key(const char* k, int f)                 { line(k, keys::nameFromVk(f)); }
    };

    template <class T> void clampTo(T& v, T lo, T hi, const char* name, std::vector<std::string>& notes) {
        const T old = v;
        v = std::clamp(v, lo, hi);
        if (v != old) notes.push_back(std::string(name) + " out of range, clamped");
    }
    void clampColor(float (&c)[4]) { for (float& x : c) x = std::isfinite(x) ? std::clamp(x, 0.f, 1.f) : 1.f; }

} // namespace

std::vector<std::string> sanitize(Settings& s) {
    std::vector<std::string> notes;
    clampTo(s.maxDistance, 1.0f, 100000.0f, "esp.max_distance", notes);
    clampTo(s.minDistance, 0.0f, 100000.0f, "esp.min_distance", notes);
    clampTo(s.fovRadius, 5.0f, 4000.0f, "esp.fov_radius", notes);
    clampTo(s.boxThickness, 1, 8, "esp.box_thickness", notes);
    clampTo(s.readIntervalMs, 1, 1000, "perf.read_interval_ms", notes);
    clampTo(s.mainWidth, 640, 7680, "window.main_width", notes);
    clampTo(s.mainHeight, 420, 4320, "window.main_height", notes);
    clampTo(s.hudOpacity, 0.1f, 1.0f, "overlay.hud_opacity", notes);
    clampTo(s.hudW, 120.0f, 2000.0f, "overlay.hud_w", notes);
    clampTo(s.hudH, 60.0f, 2000.0f, "overlay.hud_h", notes);
    clampTo(s.hudX, 0.0f, 16000.0f, "overlay.hud_x", notes);
    clampTo(s.hudY, 0.0f, 16000.0f, "overlay.hud_y", notes);
    if (s.minDistance > s.maxDistance) { s.minDistance = 0.f; notes.push_back("esp.min_distance > max_distance, reset to 0"); }
    if (s.overlayMode != OverlayMode::FitGameWindow && s.overlayMode != OverlayMode::FullScreen) {
        s.overlayMode = OverlayMode::FitGameWindow; notes.push_back("overlay.mode invalid, reset");
    }
    if (s.processName.empty() || s.processName.size() > 128) { s.processName = "RobRun.exe"; notes.push_back("general.process_name invalid, reset"); }
    if (s.offsetsFile.empty()) { s.offsetsFile = "offsets.ini"; notes.push_back("general.offsets_file empty, reset"); }
    clampColor(s.colEnemy); clampColor(s.colTeam); clampColor(s.colText);
    clampColor(s.colShadow); clampColor(s.colCrosshair);

    // Hotkeys: every code must be bindable and unique; repair with defaults.
    const Hotkeys def;
    int* slots[] = { &s.hotkeys.toggleMenu, &s.hotkeys.startStop, &s.hotkeys.toggleOverlay,
                     &s.hotkeys.layoutEdit, &s.hotkeys.toggleDemo, &s.hotkeys.quit };
    const int defs[] = { def.toggleMenu, def.startStop, def.toggleOverlay, def.layoutEdit, def.toggleDemo, def.quit };
    for (int i = 0; i < 6; ++i)
        if (!keys::isBindable(*slots[i])) { *slots[i] = defs[i]; notes.push_back("hotkey " + std::to_string(i) + " not bindable, reset"); }
    // Repair collisions: the later slot falls back to its default; if that still
    // collides, to the first unused F-key.
    for (int pass = 0; pass < 6; ++pass) {
        std::vector<int> codes; for (int* p : slots) codes.push_back(*p);
        const auto dups = keys::findDuplicates(codes);
        if (dups.empty()) break;
        const int b = dups.front().second;
        auto taken = [&](int vk) { for (int i = 0; i < 6; ++i) if (i != b && *slots[i] == vk) return true; return false; };
        int pick = defs[b];
        if (taken(pick)) {
            for (int vk = 0x70; vk <= 0x7B; ++vk) if (!taken(vk)) { pick = vk; break; }
        }
        *slots[b] = pick;
        notes.push_back("duplicate hotkey, slot " + std::to_string(b) + " reassigned to " + keys::nameFromVk(pick));
    }
    return notes;
}

LoadResult parse(Settings& s, std::string_view text) {
    LoadResult res;
    res.fileFound = true;
    Reader rd;
    rd.warnings = &res.warnings;

    size_t lineNo = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        ++lineNo;
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            res.warnings.push_back("line " + std::to_string(lineNo) + ": missing '='");
            continue;
        }
        const std::string key(trim(line.substr(0, eq)));
        const std::string val(trim(line.substr(eq + 1)));
        if (key.empty()) { res.warnings.push_back("line " + std::to_string(lineNo) + ": empty key"); continue; }
        rd.kv[key] = val;
    }

    // Version must be understood before trusting anything else.
    if (auto it = rd.kv.find("version"); it != rd.kv.end()) {
        int v = 0;
        if (parseInt(it->second, v)) s.version = v; else res.warnings.push_back("invalid version value");
        rd.used.insert("version");
    }
    visitSettings(s, rd);
    for (const auto& [k, v] : rd.kv)
        if (!rd.used.count(k)) res.warnings.push_back("unknown key '" + k + "' ignored");

    for (auto& n : sanitize(s)) res.warnings.push_back(std::move(n));
    return res;
}

LoadResult load(Settings& s, const std::filesystem::path& path) {
    LoadResult res;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        res.fileFound = false;
        return res;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        res.fileFound = true;
        res.ok = false;
        res.warnings.push_back("cannot open " + path.string());
        return res;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse(s, ss.str());
}

std::string serialize(const Settings& sIn) {
    Settings s = sIn;
    Writer w;
    w.out << "# RobRun Overlay settings (edited by the app; safe to edit by hand while the app is closed)\n";
    w.out << "version = " << s.version << "\n";
    visitSettings(s, w);
    return w.out.str();
}

bool save(const Settings& s, const std::filesystem::path& path, std::string* error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { if (error) *error = "cannot write " + tmp.string(); return false; }
        const std::string text = serialize(s);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) { if (error) *error = "write failed for " + tmp.string(); return false; }
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        if (error) *error = "cannot replace " + path.string();
        return false;
    }
    return true;
}

} // namespace config
