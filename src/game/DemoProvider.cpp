#include "DemoProvider.h"

#include <chrono>
#include <cmath>
#include <string>

DemoProvider::DemoProvider(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) {
        const auto t0 = std::chrono::steady_clock::now();
        clock_ = [t0]() {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        };
    }
}

UpdateResult DemoProvider::update(WorldSnap& out) {
    constexpr float kPi = 3.14159265358979f;
    const double t = clock_();

    out = WorldSnap{};
    out.source = DataSource::Demo;
    out.valid = true;
    out.inMatch = true;
    out.localTeam = 0;
    out.localAddress = 0;

    // Fixed camera at the origin looking along +Y, slowly panning.
    const Vec3 eye{ 0.f, 0.f, 1.7f };
    const float yaw = kPi * 0.5f + 0.35f * std::sin(static_cast<float>(t) * 0.25f);
    out.localPos = eye;
    out.localPos.z = 0.f;
    out.viewMatrix = makeViewProjection(eye, yaw, 0.0f, 70.f * kPi / 180.f, aspect_.load());

    for (int i = 0; i < kEntityCount; ++i) {
        EntitySnap e;
        e.address = 0;
        const float phase  = static_cast<float>(i) * (2.f * kPi / kEntityCount);
        const float radius = 12.f + 6.f * static_cast<float>(i % 4);
        const float ang    = phase + static_cast<float>(t) * (0.15f + 0.03f * static_cast<float>(i));
        // Keep entities in front of the camera half of the time by orbiting around (0, 25).
        e.pos = { radius * std::cos(ang), 25.f + radius * std::sin(ang), 0.f };
        e.head = { e.pos.x, e.pos.y, 1.8f };
        e.maxHp = 100.f;
        e.hp = 55.f + 45.f * std::sin(static_cast<float>(t) * 0.5f + phase);
        e.team = (i % 3 == 0) ? 0 : 1;   // team 0 = same as local
        e.alive = true;
        e.visible = true;
        e.name = "DEMO-" + std::to_string(i + 1);
        out.entities.push_back(std::move(e));
    }
    return {};
}
