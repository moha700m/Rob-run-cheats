// DataWorker.h - Background thread that probes for the game and produces world
// snapshots from the selected provider (live process memory or Demo Mode). The
// UI thread only reads published copies, never touches providers.
#pragma once
#include "game/GameData.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

enum class Link {
    Idle,               // stopped, game not running
    GameDetected,       // stopped, game process is running
    Waiting,            // running (live), waiting for the game process
    SdkNotConfigured,   // running (live), game found but offsets.ini incomplete
    Live,               // running (live), real data flowing
    Demo,               // running, synthetic data
    Error,              // running (live), attach/read problem
};

const char* toString(Link l);

struct SessionStatus {
    bool        running = false;
    bool        demo    = false;
    Link        link    = Link::Idle;
    ProbeResult game;
    DataSource  source  = DataSource::None;
    std::string detail;
    uint32_t    entityCount   = 0;
    uint64_t    updates       = 0;   // successful updates since start()
    uint64_t    failures      = 0;   // failed updates since start()
    double      updatesPerSec = 0.0;
};

class DataWorker {
public:
    DataWorker(IGameDataProvider& live, IGameDataProvider& demo);
    ~DataWorker();

    DataWorker(const DataWorker&) = delete;
    DataWorker& operator=(const DataWorker&) = delete;

    void start();                 // begin producing data (live or demo)
    void stop();                  // stop producing data, release live handles
    bool running() const { return running_.load(); }

    void setDemo(bool on);
    bool demo() const { return demo_.load(); }
    void setIntervalMs(int ms);

    SessionStatus status() const;
    WorldSnap     snapshot() const;

    // Joins the thread. Idempotent; also called by the destructor.
    void shutdown();

private:
    void loop(std::stop_token st);
    void wake();
    void publish(const SessionStatus& s, WorldSnap&& snap);

    IGameDataProvider& live_;
    IGameDataProvider& demoProv_;

    std::atomic<bool> running_{ false };
    std::atomic<bool> demo_{ false };
    std::atomic<int>  intervalMs_{ 16 };
    std::atomic<bool> releaseLive_{ false };
    std::atomic<bool> resetCounters_{ false };
    std::atomic<bool> wakeFlag_{ false };

    std::mutex wakeMtx_;
    std::condition_variable_any wakeCv_;

    mutable std::mutex pubMtx_;
    SessionStatus status_;
    WorldSnap     snap_;

    std::jthread thread_;
};
