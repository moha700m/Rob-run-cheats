#include "TestFramework.h"
#include "logging/Logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

TEST(logger, ring_buffer_drops_oldest_and_ids_increase) {
    Logger l(5);
    for (int i = 1; i <= 8; ++i) l.log(LogLevel::Info, "m" + std::to_string(i));
    auto s = l.snapshot();
    CHECK_EQ(s.size(), size_t(5));
    CHECK_EQ(s.front().text, std::string("m4"));
    CHECK_EQ(s.back().text, std::string("m8"));
    CHECK_EQ(l.totalLogged(), uint64_t(8));
    for (size_t i = 1; i < s.size(); ++i) CHECK(s[i].id == s[i - 1].id + 1);
}

TEST(logger, since_returns_only_newer_entries) {
    Logger l(10);
    l.log(LogLevel::Info, "a"); l.log(LogLevel::Warn, "b"); l.log(LogLevel::Error, "c");
    auto n = l.since(2);
    CHECK_EQ(n.size(), size_t(1));
    CHECK_EQ(n[0].text, std::string("c"));
    CHECK(l.since(3).empty());
}

TEST(logger, clear_empties_buffer_but_keeps_counting) {
    Logger l(10);
    l.log(LogLevel::Info, "a");
    l.clear();
    CHECK_EQ(l.size(), size_t(0));
    l.log(LogLevel::Info, "b");
    CHECK_EQ(l.snapshot().front().id, uint64_t(2));
}

TEST(logger, format_contains_level_and_text) {
    Logger l(3);
    l.log(LogLevel::Warn, "hello");
    auto line = Logger::format(l.snapshot().front());
    CHECK(line.find("[WARN ]") != std::string::npos);
    CHECK(line.find("hello") != std::string::npos);
    CHECK_EQ(line[2], ':');   // HH:MM:SS.mmm
}

TEST(logger, file_sink_writes_lines) {
    auto dir = fs::temp_directory_path() / "robrun_test_logfile";
    fs::remove_all(dir); fs::create_directories(dir);
    auto path = dir / "t.log";
    Logger l(10);
    std::string err;
    CHECK(l.openFile(path, &err));
    l.log(LogLevel::Info, "line one");
    l.log(LogLevel::Error, "line two");
    l.closeFile();
    std::ifstream in(path);
    std::stringstream ss; ss << in.rdbuf();
    const std::string text = ss.str();
    CHECK(text.find("line one") != std::string::npos);
    CHECK(text.find("[ERROR] line two") != std::string::npos);
}

TEST(logger, file_rotates_when_too_large) {
    auto dir = fs::temp_directory_path() / "robrun_test_logrotate";
    fs::remove_all(dir); fs::create_directories(dir);
    auto path = dir / "r.log";
    Logger l(10, 256);
    CHECK(l.openFile(path));
    for (int i = 0; i < 40; ++i) l.log(LogLevel::Info, std::string(30, 'x'));
    l.closeFile();
    CHECK(fs::exists(path.string() + ".1"));
    CHECK(fs::file_size(path) < 1024);
}

TEST(logger, open_failure_reports_error) {
    Logger l(3);
    std::string err;
    CHECK(!l.openFile(fs::temp_directory_path() / "robrun_no_such_dir_zzz" / "x.log", &err));
    CHECK(!err.empty());
}

TEST(logger, concurrent_writers_do_not_lose_or_corrupt_entries) {
    Logger l(10000);
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&l, t] { for (int i = 0; i < 500; ++i) l.log(LogLevel::Debug, std::to_string(t) + ":" + std::to_string(i)); });
    for (auto& t : ts) t.join();
    CHECK_EQ(l.totalLogged(), uint64_t(2000));
    auto s = l.snapshot();
    CHECK_EQ(s.size(), size_t(2000));
    for (size_t i = 1; i < s.size(); ++i) CHECK(s[i].id == s[i - 1].id + 1);
}
