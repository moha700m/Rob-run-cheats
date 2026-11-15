// Guards against drift between files shipped in the repo and the code.
#include "TestFramework.h"
#include "game/SdkOffsets.h"

#include <fstream>
#include <sstream>

#ifndef ROBRUN_SOURCE_DIR
#define ROBRUN_SOURCE_DIR "."
#endif

static std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

static std::string normalizeEol(std::string s) {
    std::string out;
    for (char c : s) if (c != '\r') out.push_back(c);
    return out;
}

TEST(repo, offsets_template_file_matches_builtin_template) {
    const std::string file = normalizeEol(slurp(std::string(ROBRUN_SOURCE_DIR) + "/offsets.template.ini"));
    CHECK(!file.empty());
    CHECK(file == normalizeEol(offsets::templateText()));
}

TEST(repo, shipped_template_contains_no_real_values) {
    SdkOffsets o;
    const auto r = offsets::parse(o, slurp(std::string(ROBRUN_SOURCE_DIR) + "/offsets.template.ini"));
    CHECK(r.warnings.empty());
    CHECK(!o.localPlayer && !o.entityList && !o.viewMatrix && !o.position && !o.health && !o.team);
    CHECK(!o.expectedModuleSize);
}
