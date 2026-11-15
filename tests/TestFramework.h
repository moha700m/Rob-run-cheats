// Minimal dependency-free test framework (no network fetch needed in CI).
#pragma once
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace tf {

struct Case {
    std::string suite, name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> fn) {
        registry().push_back({ suite, name, std::move(fn) });
    }
};

struct Failure { std::string msg; };
inline int& failCount() { static int n = 0; return n; }

[[noreturn]] inline void fail(const char* file, int line, const std::string& what) {
    std::ostringstream ss;
    ss << file << ":" << line << ": " << what;
    throw Failure{ ss.str() };
}

template <class A, class B>
void checkEq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream ss;
        ss << "CHECK_EQ(" << ea << ", " << eb << ") failed: " << a << " vs " << b;
        fail(file, line, ss.str());
    }
}

} // namespace tf

#define TF_CAT2(a, b) a##b
#define TF_CAT(a, b) TF_CAT2(a, b)
#define TEST(suite, name)                                                         \
    static void TF_CAT(tf_fn_, __LINE__)();                                       \
    static tf::Registrar TF_CAT(tf_reg_, __LINE__)(#suite, #name, TF_CAT(tf_fn_, __LINE__)); \
    static void TF_CAT(tf_fn_, __LINE__)()

#define CHECK(cond)    do { if (!(cond)) tf::fail(__FILE__, __LINE__, "CHECK(" #cond ") failed"); } while (0)
#define CHECK_EQ(a, b) tf::checkEq((a), (b), #a, #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) do { if (!(std::fabs((double)(a) - (double)(b)) <= (eps))) \
    tf::fail(__FILE__, __LINE__, "CHECK_NEAR(" #a ", " #b ") failed"); } while (0)
