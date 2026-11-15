#include "TestFramework.h"
#include "core/DataWorker.h"
#include "game/DemoProvider.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

namespace {

    struct FakeLive : IGameDataProvider {
        std::atomic<bool> found{ false };
        std::atomic<int>  mode{ 0 };     // 0 not running, 1 ok, 2 sdk missing, 3 read failed, 4 throws
        std::atomic<int>  releases{ 0 };
        DataSource source() const override { return DataSource::Live; }
        const char* name() const override { return "Fake"; }
        ProbeResult probe() override { return { found.load(), found.load() ? 4242u : 0u }; }
        UpdateResult update(WorldSnap& out) override {
            using C = UpdateResult::Code;
            switch (mode.load()) {
            case 1: {
                out = WorldSnap{}; out.source = DataSource::Live; out.valid = true;
                out.entities.resize(3);
                return {};
            }
            case 2: return { C::SdkNotConfigured, "offsets.ini missing: rva.view_matrix" };
            case 3: return { C::ReadFailed, "view matrix unreadable" };
            case 4: throw std::runtime_error("boom");
            default: return { C::NotRunning, "RobRun.exe not found" };
            }
        }
        void release() override { ++releases; }
    };

    template <class P>
    bool waitFor(P pred, int timeoutMs = 3000) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < end) {
            if (pred()) return true;
            std::this_thread::sleep_for(5ms);
        }
        return pred();
    }

} // namespace

TEST(worker, stopped_and_no_game_is_idle) {
    FakeLive live; DemoProvider demo;
    DataWorker w(live, demo);
    CHECK(waitFor([&] { return w.status().link == Link::Idle; }));
    CHECK(!w.snapshot().valid);
}

TEST(worker, game_detected_while_stopped) {
    FakeLive live; DemoProvider demo;
    live.found = true;
    DataWorker w(live, demo);
    CHECK(waitFor([&] { return w.status().link == Link::GameDetected; }));
    CHECK_EQ(w.status().game.pid, 4242u);
}

TEST(worker, running_without_game_waits_and_does_not_crash) {
    FakeLive live; DemoProvider demo;
    DataWorker w(live, demo);
    w.start();
    CHECK(waitFor([&] { return w.status().link == Link::Waiting; }));
    CHECK(!w.snapshot().valid);
    CHECK(w.status().failures > 0);
}

TEST(worker, live_data_flows_when_provider_ok) {
    FakeLive live; DemoProvider demo;
    live.mode = 1;
    DataWorker w(live, demo);
    w.setIntervalMs(5);
    w.start();
    CHECK(waitFor([&] { return w.status().link == Link::Live; }));
    CHECK(waitFor([&] { return w.status().updates >= 3; }));
    CHECK(w.snapshot().source == DataSource::Live);
    CHECK_EQ(w.status().entityCount, 3u);
}

TEST(worker, sdk_not_configured_is_reported_with_detail) {
    FakeLive live; DemoProvider demo;
    live.mode = 2;
    DataWorker w(live, demo);
    w.start();
    CHECK(waitFor([&] { return w.status().link == Link::SdkNotConfigured; }));
    CHECK(w.status().detail.find("view_matrix") != std::string::npos);
    CHECK(!w.snapshot().valid);
}

TEST(worker, read_failure_and_exception_become_error_state_and_worker_survives) {
    FakeLive live; DemoProvider demo;
    live.mode = 3;
    DataWorker w(live, demo);
    w.start();
    CHECK(waitFor([&] { return w.status().link == Link::Error; }));
    live.mode = 4;
    CHECK(waitFor([&] { return w.status().detail.find("boom") != std::string::npos; }));
    live.mode = 1;
    CHECK(waitFor([&] { return w.status().link == Link::Live; }));
}

TEST(worker, demo_mode_produces_demo_tagged_data_even_without_game) {
    FakeLive live; DemoProvider demo;
    DataWorker w(live, demo);
    w.setDemo(true);
    w.setIntervalMs(5);
    w.start();
    CHECK(waitFor([&] { return w.status().link == Link::Demo; }));
    CHECK(waitFor([&] { return w.snapshot().valid; }));
    auto s = w.snapshot();
    CHECK(s.source == DataSource::Demo);
    CHECK_EQ(int(s.entities.size()), DemoProvider::kEntityCount);
    CHECK(w.status().source == DataSource::Demo);
}

TEST(worker, stop_releases_live_handles_and_clears_snapshot) {
    FakeLive live; DemoProvider demo;
    live.mode = 1;
    DataWorker w(live, demo);
    w.setIntervalMs(5);
    w.start();
    CHECK(waitFor([&] { return w.status().link == Link::Live; }));
    w.stop();
    CHECK(waitFor([&] { return w.status().link == Link::Idle; }));
    CHECK(live.releases.load() >= 1);
    CHECK(!w.snapshot().valid);
}

TEST(worker, restart_cycles_are_clean) {
    FakeLive live; DemoProvider demo;
    live.mode = 1;
    DataWorker w(live, demo);
    w.setIntervalMs(5);
    for (int i = 0; i < 5; ++i) {
        w.start();
        CHECK(waitFor([&] { return w.status().link == Link::Live && w.status().updates >= 1; }));
        w.stop();
        CHECK(waitFor([&] { return w.status().link == Link::Idle; }));
    }
}

TEST(worker, shutdown_is_prompt_even_with_long_interval) {
    FakeLive live; DemoProvider demo;
    live.mode = 1;
    const auto t0 = std::chrono::steady_clock::now();
    {
        DataWorker w(live, demo);
        w.setIntervalMs(1000);
        w.start();
        CHECK(waitFor([&] { return w.status().link == Link::Live; }));
    }   // destructor must not wait out the 1 s interval
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ms < 900);
}

TEST(worker, shutdown_is_idempotent) {
    FakeLive live; DemoProvider demo;
    DataWorker w(live, demo);
    w.shutdown();
    w.shutdown();
    CHECK(live.releases.load() >= 1);
}
