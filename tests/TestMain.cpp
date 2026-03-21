#include <exception>
#include <iostream>

#include "TestSupport.h"

int main() {
    int failedCount = 0;

    for (const auto& test : safecrowd::tests::registry()) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failedCount;
            std::cerr << "[FAIL] " << test.name << '\n'
                      << "  " << ex.what() << '\n';
        }
    }

    if (failedCount > 0) {
        std::cerr << failedCount << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All tests passed.\n";
    return 0;
}
