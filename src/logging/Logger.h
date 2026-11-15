// Logger.h - Thread-safe in-memory ring buffer (for the live log view) plus an
// optional size-rotated file sink. No OS dependencies.
#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

const char* toString(LogLevel l);

struct LogEntry {
    uint64_t id = 0;   // strictly increasing, starts at 1
    std::chrono::system_clock::time_point time;
    LogLevel level = LogLevel::Info;
    std::string text;
};

class Logger {
public:
    explicit Logger(size_t capacity = 2000, uintmax_t maxFileBytes = 1u << 20);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, std::string text);

    // Entries currently held (oldest first).
    std::vector<LogEntry> snapshot() const;
    // Entries with id > lastId (oldest first).
    std::vector<LogEntry> since(uint64_t lastId) const;

    size_t   size() const;
    uint64_t totalLogged() const;
    void     clear();   // empties the ring buffer only, not the file

    bool openFile(const std::filesystem::path& path, std::string* error = nullptr);
    void closeFile();
    std::filesystem::path filePath() const;

    static std::string format(const LogEntry& e);   // "HH:MM:SS.mmm [INFO ] text"

    static Logger& global();

private:
    void writeToFile(const LogEntry& e);
    void rotateIfNeeded();

    mutable std::mutex mtx_;
    std::vector<LogEntry> ring_;     // circular storage
    size_t   head_   = 0;            // index of oldest element
    size_t   count_  = 0;
    size_t   capacity_;
    uint64_t nextId_ = 1;

    std::filesystem::path filePath_;
    std::ofstream file_;
    uintmax_t fileBytes_ = 0;
    uintmax_t maxFileBytes_;
};

template <typename... Args>
void logAt(LogLevel lvl, std::format_string<Args...> fmt, Args&&... args) {
    Logger::global().log(lvl, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args> void logDebug(std::format_string<Args...> f, Args&&... a) { logAt(LogLevel::Debug, f, std::forward<Args>(a)...); }
template <typename... Args> void logInfo (std::format_string<Args...> f, Args&&... a) { logAt(LogLevel::Info,  f, std::forward<Args>(a)...); }
template <typename... Args> void logWarn (std::format_string<Args...> f, Args&&... a) { logAt(LogLevel::Warn,  f, std::forward<Args>(a)...); }
template <typename... Args> void logError(std::format_string<Args...> f, Args&&... a) { logAt(LogLevel::Error, f, std::forward<Args>(a)...); }
