#include "TestFramework.h"
#include "game/DemoProvider.h"
#include "core/Validate.h"

#include <cmath>

TEST(demo, snapshot_is_tagged_demo_with_labeled_entities) {
    double t = 0;
    DemoProvider d([&] { return t; });
    WorldSnap s;
    CHECK(d.update(s).ok());
    CHECK(s.source == DataSource::Demo);
    CHECK(s.valid);
    CHECK_EQ(int(s.entities.size()), DemoProvider::kEntityCount);
    for (const auto& e : s.entities) {
        CHECK(e.name.rfind("DEMO-", 0) == 0);
        CHECK(e.alive);
        CHECK(validate::plausibleHealth(e.hp, e.maxHp));
        CHECK(validate::plausibleWorldPos(e.pos));
    }
    CHECK(validate::viewMatrixUsable(s.viewMatrix));
}

TEST(demo, is_deterministic_for_a_given_clock_and_changes_over_time) {
    double t = 3.0;
    DemoProvider a([&] { return t; }), b([&] { return t; });
    WorldSnap sa, sb;
    a.update(sa); b.update(sb);
    CHECK_EQ(sa.entities[2].pos.x, sb.entities[2].pos.x);
    t = 7.0;
    WorldSnap sc; a.update(sc);
    CHECK(sc.entities[2].pos.x != sa.entities[2].pos.x);
}

TEST(demo, at_least_one_entity_is_visible_on_screen_in_front_of_camera) {
    double t = 0;
    DemoProvider d([&] { return t; });
    int visible = 0;
    for (t = 0; t < 30; t += 1.0) {
        WorldSnap s; d.update(s);
        for (const auto& e : s.entities) {
            Vec2 tl, br;
            if (worldBoxToScreen(e.pos, 1.8f, 0.6f, s.viewMatrix, 1920, 1080, tl, br)) ++visible;
        }
    }
    CHECK(visible > 50);
}

TEST(demo, probe_never_reports_a_game) {
    DemoProvider d;
    CHECK(!d.probe().found);
}
