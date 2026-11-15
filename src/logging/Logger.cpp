#include "Logger.h"

#include <ctime>
#include <system_error>

const char* toString(LogLevel l) {
    switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

Logger::Logger(size_t capacity, uintmax_t maxFileBytes)
    : capacity_(capacity ? capacity : 1), maxFileBytes_(maxFileBytes) {
    ring_.resize(capacity_);
}

Logger::~Logger() { closeFile(); }

Logger& Logger::global() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, std::string text) {
    std::lock_guard<std::mutex> lk(mtx_);
    LogEntry e;
    e.id    = nextId_++;
    e.time  = std::chrono::system_clock::now();
    e.level = level;
    e.text  = std::move(text);

    if (count_ < capacity_) {
        ring_[(head_ + count_) % capacity_] = e;
        ++count_;
    } else {
        ring_[head_] = e;
        head_ = (head_ + 1) % capacity_;
    }
    writeToFile(e);
}

std::vector<LogEntry> Logger::snapshot() const { return since(0); }

std::vector<LogEntry> Logger::since(uint64_t lastId) const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<LogEntry> out;
    out.reserve(count_);
    for (size_t i = 0; i < count_; ++i) {
        const LogEntry& e = ring_[(head_ + i) % capacity_];
        if (e.id > lastId) out.push_back(e);
    }
    return out;
}

size_t Logger::size() const { std::lock_guard<std::mutex> lk(mtx_); return count_; }
uint64_t Logger::totalLogged() const { std::lock_guard<std::mutex> lk(mtx_); return nextId_ - 1; }

void Logger::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    head_ = 0;
    count_ = 0;
}

bool Logger::openFile(const std::filesystem::path& path, std::string* error) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (file_.is_open()) file_.close();
    std::error_code ec;
    fileBytes_ = std::filesystem::exists(path, ec) ? std::filesystem::file_size(path, ec) : 0;
    file_.open(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!file_) {
        if (error) *error = "cannot open log file: " + path.string();
        filePath_.clear();
        return false;
    }
    filePath_ = path;
    return true;
}

void Logger::closeFile() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (file_.is_open()) file_.close();
    filePath_.clear();
}

std::filesystem::path Logger::filePath() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return filePath_;
}

void Logger::rotateIfNeeded() {
    if (fileBytes_ < maxFileBytes_) return;
    file_.close();
    std::error_code ec;
    std::filesystem::path old = filePath_;
    old += ".1";
    std::filesystem::remove(old, ec);
    std::filesystem::rename(filePath_, old, ec);
    file_.open(filePath_, std::ios::out | std::ios::trunc | std::ios::binary);
    fileBytes_ = 0;
}

void Logger::writeToFile(const LogEntry& e) {
    if (!file_.is_open()) return;
    rotateIfNeeded();
    if (!file_.is_open()) return;
    const std::string line = format(e) + "\n";
    file_.write(line.data(), static_cast<std::streamsize>(line.size()));
    file_.flush();
    fileBytes_ += line.size();
}

std::string Logger::format(const LogEntry& e) {
    using namespace std::chrono;
    const auto secs = time_point_cast<seconds>(e.time);
    const auto ms   = duration_cast<milliseconds>(e.time - secs).count();
    const std::time_t t = system_clock::to_time_t(e.time);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    return std::format("{:02}:{:02}:{:02}.{:03} [{}] {}", tmv.tm_hour, tmv.tm_min,
                       tmv.tm_sec, static_cast<int>(ms), toString(e.level), e.text);
}
