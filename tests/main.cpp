// Test runner. Usage: RobRunTests [suite] [--report file.md]
#include "TestFramework.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    std::string suite, report;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--report") == 0 && i + 1 < argc) report = argv[++i];
        else if (argv[i][0] != '-') suite = argv[i];
    }

    int run = 0, failed = 0;
    std::string rows;
    for (const auto& c : tf::registry()) {
        if (!suite.empty() && c.suite != suite) continue;
        ++run;
        const auto t0 = std::chrono::steady_clock::now();
        std::string err;
        try { c.fn(); }
        catch (const tf::Failure& f) { err = f.msg; }
        catch (const std::exception& e) { err = std::string("exception: ") + e.what(); }
        catch (...) { err = "unknown exception"; }
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        if (err.empty()) {
            std::printf("[ PASS ] %s.%s (%lld ms)\n", c.suite.c_str(), c.name.c_str(), (long long)ms);
            rows += "| " + c.suite + "." + c.name + " | PASS | " + std::to_string(ms) + " ms |\n";
        } else {
            ++failed;
            std::printf("[ FAIL ] %s.%s\n         %s\n", c.suite.c_str(), c.name.c_str(), err.c_str());
            rows += "| " + c.suite + "." + c.name + " | **FAIL** | " + err + " |\n";
        }
    }
    std::printf("\n%d run, %d failed\n", run, failed);
    if (run == 0) { std::printf("no tests matched\n"); return 2; }

    if (!report.empty()) {
        std::ofstream out(report, std::ios::app);
        out << "### Unit tests" << (suite.empty() ? "" : " (" + suite + ")") << "\n\n"
            << "| Test | Result | Detail |\n|---|---|---|\n" << rows << "\n"
            << run << " run, " << failed << " failed\n\n";
    }
    return failed ? 1 : 0;
}
