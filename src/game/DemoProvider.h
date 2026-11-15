// DemoProvider.h - Synthetic data for Demo Mode. Everything produced here is
// fabricated, named "DEMO-n", and tagged DataSource::Demo so the UI can never
// present it as real game data.
#pragma once
#include "GameData.h"

#include <atomic>
#include <functional>

class DemoProvider final : public IGameDataProvider {
public:
    using Clock = std::function<double()>;   // seconds
    explicit DemoProvider(Clock clock = nullptr);

    DataSource  source() const override { return DataSource::Demo; }
    const char* name()   const override { return "Demo"; }
    ProbeResult probe() override { return {}; }
    UpdateResult update(WorldSnap& out) override;
    void release() override {}

    void setAspect(float aspect) { aspect_.store(aspect > 0.1f ? aspect : 16.f / 9.f); }

    static constexpr int kEntityCount = 8;

private:
    Clock clock_;
    std::atomic<float> aspect_{ 16.f / 9.f };
};
