#pragma once

#include <cmath>
#include <functional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace safecrowd::tests {

class TestFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct TestCase {
    std::string name;
    std::function<void()> run;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* testName, std::function<void()> testBody) {
        registry().push_back({testName, std::move(testBody)});
    }
};

[[noreturn]] inline void fail(const char* file, int line, const std::string& message) {
    std::ostringstream stream;
    stream << file << ":" << line << ": " << message;
    throw TestFailure(stream.str());
}

template <typename T>
std::string stringify(const T& value) {
    if constexpr (requires(std::ostream& stream, const T& item) { stream << item; }) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        return std::to_string(static_cast<Underlying>(value));
    } else {
        return "<non-printable>";
    }
}

inline void expectTrue(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        fail(file, line, std::string("Expected true: ") + expr);
    }
}

template <typename L, typename R>
void expectEqual(const L& lhs, const R& rhs, const char* lhsExpr, const char* rhsExpr, const char* file, int line) {
    if (!(lhs == rhs)) {
        std::ostringstream stream;
        stream << "Expected " << lhsExpr << " == " << rhsExpr << " but got "
               << stringify(lhs) << " and " << stringify(rhs);
        fail(file, line, stream.str());
    }
}

inline void expectNear(double lhs, double rhs, double epsilon, const char* lhsExpr, const char* rhsExpr, const char* file, int line) {
    if (std::fabs(lhs - rhs) > epsilon) {
        std::ostringstream stream;
        stream << "Expected " << lhsExpr << " ~= " << rhsExpr << " within " << epsilon
               << " but got " << lhs << " and " << rhs;
        fail(file, line, stream.str());
    }
}

}  // namespace safecrowd::tests

#define SC_TEST(name) \
    static void name(); \
    static ::safecrowd::tests::Registrar name##_registrar(#name, &name); \
    static void name()

#define SC_EXPECT_TRUE(expr) \
    ::safecrowd::tests::expectTrue((expr), #expr, __FILE__, __LINE__)

#define SC_EXPECT_EQ(lhs, rhs) \
    ::safecrowd::tests::expectEqual((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define SC_EXPECT_NEAR(lhs, rhs, epsilon) \
    ::safecrowd::tests::expectNear((lhs), (rhs), (epsilon), #lhs, #rhs, __FILE__, __LINE__)
