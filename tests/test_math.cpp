#include "TestFramework.h"
#include "core/VecMath.h"
#include "core/Validate.h"
#include "core/Signature.h"

#include <cmath>
#include <limits>

namespace {
    constexpr float kPi = 3.14159265f;
    Matrix4 cam(float yaw = 0.f) { return makeViewProjection({ 0, 0, 0 }, yaw, 0.f, 90.f * kPi / 180.f, 16.f / 9.f); }
}

TEST(math, point_straight_ahead_projects_to_screen_center) {
    Vec2 p;
    CHECK(worldToScreen({ 10, 0, 0 }, cam(), 1920, 1080, p));
    CHECK_NEAR(p.x, 960.0, 0.5);
    CHECK_NEAR(p.y, 540.0, 0.5);
}

TEST(math, right_of_camera_is_right_on_screen_and_up_is_up) {
    Vec2 right, up;
    // Facing +X in a Z-up right-handed world, the right-hand side is -Y.
    CHECK(worldToScreen({ 10, -3, 0 }, cam(), 1920, 1080, right));
    CHECK(right.x > 960.f);
    CHECK(worldToScreen({ 10, 0, 3 }, cam(), 1920, 1080, up));
    CHECK(up.y < 540.f);
}

TEST(math, point_behind_camera_is_rejected) {
    Vec2 p;
    CHECK(!worldToScreen({ -10, 0, 0 }, cam(), 1920, 1080, p));
}

TEST(math, yaw_rotates_view) {
    Vec2 p;
    // yaw = 90 deg looks along +Y
    CHECK(worldToScreen({ 0, 10, 0 }, cam(kPi / 2), 1920, 1080, p));
    CHECK_NEAR(p.x, 960.0, 0.5);
}

TEST(math, nan_matrix_does_not_project) {
    Matrix4 m;  // all zeros -> clipW = 0
    Vec2 p;
    CHECK(!worldToScreen({ 1, 2, 3 }, m, 100, 100, p));
    m.m[3][3] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!worldToScreen({ 1, 2, 3 }, m, 100, 100, p));
}

TEST(math, box_has_expected_aspect_and_is_ordered) {
    Vec2 tl, br;
    CHECK(worldBoxToScreen({ 10, 0, -1 }, 1.8f, 0.6f, cam(), 1920, 1080, tl, br));
    CHECK(tl.y < br.y);
    CHECK(tl.x < br.x);
    CHECK_NEAR((br.x - tl.x) / (br.y - tl.y), 0.6 / 1.8, 0.01);
}

TEST(validate, pointers) {
    CHECK(!validate::plausiblePointer(0));
    CHECK(!validate::plausiblePointer(0x1000));
    CHECK(validate::plausiblePointer(0x00007FF612340000ull));
    CHECK(!validate::plausiblePointer(0xFFFFF80000000000ull));
}

TEST(validate, numbers_and_matrix) {
    CHECK(validate::plausibleHealth(50.f, 100.f));
    CHECK(!validate::plausibleHealth(std::numeric_limits<float>::quiet_NaN(), 100.f));
    CHECK(!validate::plausibleHealth(50.f, 0.f));
    CHECK(validate::plausibleWorldPos({ 1, 2, 3 }));
    CHECK(!validate::plausibleWorldPos({ 1e9f, 0, 0 }));
    CHECK(!validate::viewMatrixUsable(Matrix4{}));
    CHECK(validate::viewMatrixUsable(cam()));
    CHECK(validate::printableAscii("DEMO-1"));
    CHECK(!validate::printableAscii(std::string("a\x01" "b")));
    CHECK(validate::inRange(0x1010, 0x1000, 0x100));
    CHECK(!validate::inRange(0x1100, 0x1000, 0x100));
}

TEST(signature, parse_accepts_wildcards_and_rejects_garbage) {
    auto p = sig::parse("48 8B 05 ? ? ?? 3");
    CHECK(p.has_value());
    CHECK_EQ(p->size(), size_t(7));
    CHECK((*p)[3].wild);
    CHECK_EQ(int((*p)[6].value), 3);
    CHECK(!sig::parse("").has_value());
    CHECK(!sig::parse("48 ZZ").has_value());
    CHECK(!sig::parse("123").has_value());
}

TEST(signature, find_with_wildcards_and_bounds) {
    const uint8_t data[] = { 0x00, 0x48, 0x8B, 0x05, 0xAA, 0xBB, 0xCC, 0xDD, 0x48, 0x85 };
    auto p = *sig::parse("48 8B 05 ? ? ? ? 48 85");
    auto hit = sig::find(data, sizeof(data), p);
    CHECK(hit.has_value());
    CHECK_EQ(*hit, size_t(1));
    CHECK(!sig::find(data, 4, p).has_value());                       // buffer shorter than pattern
    CHECK(!sig::find(data, sizeof(data), *sig::parse("48 8B 06")).has_value());
    CHECK(sig::find(data, sizeof(data), *sig::parse("85")).has_value());   // match at the very end
}

TEST(signature, rip_target) {
    CHECK_EQ(sig::ripTarget(0x1000, 7, 0x20), uint64_t(0x1027));
    CHECK_EQ(sig::ripTarget(0x1000, 7, -0x10), uint64_t(0x0FF7));
}
