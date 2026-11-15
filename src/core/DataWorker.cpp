#include "DataWorker.h"

#include "logging/Logger.h"

#include <algorithm>

const char* toString(Link l) {
    switch (l) {
    case Link::Idle:             return "Stopped";
    case Link::GameDetected:     return "Stopped (game detected)";
    case Link::Waiting:          return "Waiting for game";
    case Link::SdkNotConfigured: return "Game found - offsets not configured";
    case Link::Live:             return "Live";
    case Link::Demo:             return "Demo data";
    case Link::Error:            return "Error";
    }
    return "?";
}

DataWorker::DataWorker(IGameDataProvider& live, IGameDataProvider& demo)
    : live_(live), demoProv_(demo) {
    thread_ = std::jthread([this](std::stop_token st) { loop(st); });
}

DataWorker::~DataWorker() { shutdown(); }

void DataWorker::shutdown() {
    if (thread_.joinable()) {
        thread_.request_stop();
        wakeCv_.notify_all();
        thread_.join();
    }
}

void DataWorker::wake() {
    wakeFlag_.store(true);
    wakeCv_.notify_all();
}

void DataWorker::start() {
    resetCounters_.store(true);
    running_.store(true);
    wake();
}

void DataWorker::stop() {
    running_.store(false);
    releaseLive_.store(true);
    wake();
}

void DataWorker::setDemo(bool on) {
    if (demo_.exchange(on) != on) {
        releaseLive_.store(true);
        resetCounters_.store(true);
        wake();
    }
}

void DataWorker::setIntervalMs(int ms) { intervalMs_.store(std::clamp(ms, 1, 1000)); }

SessionStatus DataWorker::status() const {
    std::lock_guard<std::mutex> lk(pubMtx_);
    return status_;
}

WorldSnap DataWorker::snapshot() const {
    std::lock_guard<std::mutex> lk(pubMtx_);
    return snap_;
}

void DataWorker::publish(const SessionStatus& s, WorldSnap&& snap) {
    std::lock_guard<std::mutex> lk(pubMtx_);
    status_ = s;
    snap_   = std::move(snap);
}

void DataWorker::loop(std::stop_token st) {
    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    clock::time_point lastProbe{};
    ProbeResult probeRes;
    clock::time_point rateStart = clock::now();
    uint64_t rateCount = 0;
    double   rate = 0.0;
    uint64_t updates = 0, failures = 0;

    Link lastLink = Link::Idle;
    std::string lastDetail;
    bool first = true;
    clock::time_point lastLogged{};

    while (!st.stop_requested()) {
        const bool run  = running_.load();
        const bool demo = demo_.load();

        if (releaseLive_.exchange(false)) {
            try { live_.release(); } catch (...) {}
        }
        if (resetCounters_.exchange(false)) {
            updates = failures = 0;
            rateStart = clock::now(); rateCount = 0; rate = 0.0;
        }

        const auto now = clock::now();
        if (now - lastProbe >= 1s) {
            try { probeRes = live_.probe(); } catch (const std::exception& e) {
                probeRes = {};
                logWarn("probe threw: {}", e.what());
            }
            lastProbe = now;
        }

        SessionStatus ns;
        ns.running = run;
        ns.demo    = demo;
        ns.game    = probeRes;

        WorldSnap snap;
        std::chrono::milliseconds wait = 250ms;

        if (!run) {
            ns.link = probeRes.found ? Link::GameDetected : Link::Idle;
            ns.detail = probeRes.found ? "RobRun process detected; press Start" : "Game not running";
        } else {
            IGameDataProvider& prov = demo ? demoProv_ : live_;
            UpdateResult r;
            try {
                r = prov.update(snap);
            } catch (const std::exception& e) {
                r.code = UpdateResult::Code::ReadFailed;
                r.detail = std::string("exception: ") + e.what();
            } catch (...) {
                r.code = UpdateResult::Code::ReadFailed;
                r.detail = "unknown exception";
            }

            if (r.ok()) {
                ns.link   = demo ? Link::Demo : Link::Live;
                ns.source = snap.source;
                ns.detail = demo ? "Synthetic demo data - NOT from the game"
                                 : "Reading live game data";
                ++updates; ++rateCount;
                ns.entityCount = static_cast<uint32_t>(snap.entities.size());
            } else {
                ++failures;
                snap = WorldSnap{};
                ns.detail = r.detail;
                switch (r.code) {
                case UpdateResult::Code::NotRunning:       ns.link = Link::Waiting; break;
                case UpdateResult::Code::SdkNotConfigured: ns.link = Link::SdkNotConfigured; break;
                default:                                   ns.link = Link::Error; break;
                }
                wait = 250ms;   // do not hammer a failing source
            }
            if (r.ok()) wait = std::chrono::milliseconds(intervalMs_.load());
        }

        const auto t = clock::now();
        if (t - rateStart >= 1s) {
            rate = static_cast<double>(rateCount) / std::chrono::duration<double>(t - rateStart).count();
            rateStart = t; rateCount = 0;
        }
        ns.updates = updates;
        ns.failures = failures;
        ns.updatesPerSec = run ? rate : 0.0;

        // Log link transitions once (and changed error detail at most every 5 s).
        if (first || ns.link != lastLink ||
            (ns.detail != lastDetail && t - lastLogged >= 5s)) {
            const LogLevel lvl = (ns.link == Link::Error || ns.link == Link::SdkNotConfigured)
                                     ? LogLevel::Warn : LogLevel::Info;
            Logger::global().log(lvl, std::string("link: ") + toString(ns.link) +
                                          (ns.detail.empty() ? "" : " - " + ns.detail));
            lastLink = ns.link; lastDetail = ns.detail; lastLogged = t; first = false;
        }

        publish(ns, std::move(snap));

        std::unique_lock<std::mutex> lk(wakeMtx_);
        wakeCv_.wait_for(lk, st, wait, [this] { return wakeFlag_.exchange(false); });
    }

    try { live_.release(); } catch (...) {}
}
