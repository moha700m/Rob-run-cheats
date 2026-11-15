#include "TestFramework.h"
#include "game/SdkOffsets.h"

TEST(offsets, empty_config_is_not_ready_and_lists_missing_fields) {
    SdkOffsets o;
    CHECK(!o.ready());
    auto m = o.missingRequired();
    CHECK_EQ(m.size(), size_t(6));
}

TEST(offsets, template_parses_to_all_unset_without_warnings) {
    SdkOffsets o;
    auto r = offsets::parse(o, offsets::templateText());
    CHECK(r.warnings.empty());
    CHECK(!o.viewMatrix.has_value());
    CHECK(!o.ready());
}

TEST(offsets, zero_is_a_real_value_distinct_from_unset) {
    SdkOffsets o;
    offsets::parse(o, "entity.team = 0x0\nentity.health = 0\n");
    CHECK(o.team.has_value());
    CHECK_EQ(*o.team, uint64_t(0));
    CHECK(o.health.has_value());
    CHECK(!o.position.has_value());
}

TEST(offsets, complete_set_is_ready) {
    SdkOffsets o;
    auto r = offsets::parse(o,
        "build.label = test fixture, NOT a real RobRun dump\n"
        "build.module_size = 0x2A0000\n"
        "rva.view_matrix = 0x100\nrva.local_player = 0x200\nrva.entity_list = 0x300\n"
        "entity.position = 0x10\nentity.health = 0x20\nentity.team = 0x24\n");
    CHECK(r.warnings.empty());
    CHECK(o.ready());
    CHECK_EQ(*o.expectedModuleSize, uint64_t(0x2A0000));
    CHECK_EQ(*o.viewMatrix, uint64_t(0x100));
}

TEST(offsets, bad_numbers_warn_and_stay_unset) {
    SdkOffsets o;
    auto r = offsets::parse(o, "rva.view_matrix = 0xZZ\nentity.health = -4\nrva.entity_list = 12 34\n");
    CHECK(!o.viewMatrix.has_value());
    CHECK(!o.health.has_value());
    CHECK(!o.entityList.has_value());
    CHECK(r.warnings.size() >= 3);
}

TEST(offsets, implausible_values_are_flagged) {
    SdkOffsets o;
    auto r = offsets::parse(o, "rva.view_matrix = 0xFFFFFFFFF\nentity.health = 0x4000000\n"
                               "layout.is_alive_size = 3\nlayout.entity_stride = 1\n");
    CHECK(r.warnings.size() >= 4);
}

TEST(offsets, unknown_keys_are_reported) {
    SdkOffsets o;
    auto r = offsets::parse(o, "rva.nonsense = 1\n");
    CHECK_EQ(r.warnings.size(), size_t(1));
}

TEST(offsets, layout_options_parse) {
    SdkOffsets o;
    offsets::parse(o, "layout.health_is_int32 = true\nlayout.name_is_pointer = yes\nlayout.is_alive_size = 1\n"
                      "layout.entity_stride = 16\nlayout.max_entities = 64\n");
    CHECK(o.healthIsInt32);
    CHECK(o.nameIsPointer);
    CHECK_EQ(o.isAliveSize, 1u);
    CHECK_EQ(o.entityStride, 16u);
    CHECK_EQ(o.maxEntities, 64u);
}

TEST(offsets, missing_file_reports_not_found) {
    SdkOffsets o;
    auto r = offsets::load(o, "/definitely/not/here/offsets.ini");
    CHECK(!r.fileFound);
}
