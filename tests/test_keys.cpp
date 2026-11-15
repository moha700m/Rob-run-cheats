#include "TestFramework.h"
#include "core/Keys.h"

TEST(keys, names_roundtrip_for_defaults) {
    for (const char* n : { "INSERT", "F8", "F7", "F6", "F9", "END", "A", "Z", "0", "NUMPAD5", "HOME" }) {
        auto vk = keys::vkFromName(n);
        CHECK(vk.has_value());
        CHECK_EQ(keys::nameFromVk(*vk), std::string(n));
    }
}

TEST(keys, known_vk_values_match_winuser_header) {
    CHECK_EQ(*keys::vkFromName("INSERT"), 0x2D);
    CHECK_EQ(*keys::vkFromName("END"), 0x23);
    CHECK_EQ(*keys::vkFromName("F1"), 0x70);
    CHECK_EQ(*keys::vkFromName("F12"), 0x7B);
    CHECK_EQ(*keys::vkFromName("a"), 0x41);
}

TEST(keys, invalid_names_rejected) {
    CHECK(!keys::vkFromName("").has_value());
    CHECK(!keys::vkFromName("F25").has_value());
    CHECK(!keys::vkFromName("F0").has_value());
    CHECK(!keys::vkFromName("BANANA").has_value());
}

TEST(keys, bindable_rules) {
    CHECK(keys::isBindable(0x77));
    CHECK(!keys::isBindable(0x01));   // mouse
    CHECK(!keys::isBindable(0x1B));   // ESC reserved
    CHECK(!keys::isBindable(0x10));   // shift
    CHECK(!keys::isBindable(0));
    CHECK(!keys::isBindable(0xFF));
}

TEST(keys, duplicates_found) {
    auto d = keys::findDuplicates({ 1, 2, 3, 2, 0, 0 });
    CHECK_EQ(d.size(), size_t(1));
    CHECK_EQ(d[0].first, 1);
    CHECK_EQ(d[0].second, 3);
}

TEST(keys, edge_tracker_fires_once_per_press) {
    bool down = false;
    keys::EdgeTracker t([&](int) { return down; });
    CHECK(!t.pressed(0x77));
    down = true;
    CHECK(t.pressed(0x77));
    CHECK(!t.pressed(0x77));   // held
    down = false;
    CHECK(!t.pressed(0x77));
    down = true;
    CHECK(t.pressed(0x77));    // pressed again
    CHECK(!t.pressed(0));      // invalid code never fires
}
