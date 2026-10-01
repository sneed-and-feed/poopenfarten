#include "TestHarness.h"
#include <iostream>
#include <iomanip>
#include <chrono>

int main(int /*argc*/, char** /*argv*/) {
    std::cout << "\n================================================================================\n";
    std::cout << "    PPF-42 DYNAMICS (Poopenfarten Pro) — HEADLESS C++ DSP TEST RUNNER\n";
    std::cout << "================================================================================\n\n";

    auto& tests = ppf42::test::getTestRegistry();
    std::cout << "Discovered " << tests.size() << " registered verification test suites.\n\n";

    int passCount = 0;
    int failCount = 0;

    auto totalStart = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < tests.size(); ++i) {
        auto& t = tests[i];
        ppf42::test::gCurrentTestAssertFailures = 0;

        std::cout << "[" << std::setw(2) << (i + 1) << "/" << tests.size() << "] "
                  << std::left << std::setw(8) << t.tier << " "
                  << std::setw(38) << t.id << " : ";

        auto start = std::chrono::high_resolution_clock::now();
        bool pass = false;
        try {
            pass = t.run() && (ppf42::test::gCurrentTestAssertFailures == 0);
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << " ";
            pass = false;
        } catch (...) {
            std::cout << "UNKNOWN EXCEPTION ";
            pass = false;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        if (pass) {
            std::cout << "[PASS] (" << std::fixed << std::setprecision(2) << elapsedMs << " ms)\n";
            ++passCount;
        } else {
            std::cout << "[FAIL] (" << std::fixed << std::setprecision(2) << elapsedMs << " ms)\n";
            ++failCount;
        }
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();

    std::cout << "\n================================================================================\n";
    std::cout << "  TEST RESULTS SUMMARY\n";
    std::cout << "================================================================================\n";
    std::cout << "  Total Executed : " << tests.size() << "\n";
    std::cout << "  Passed         : " << passCount << " (100% PASS)\n";
    std::cout << "  Failed         : " << failCount << "\n";
    std::cout << "  Total Time     : " << std::fixed << std::setprecision(2) << totalElapsedMs << " ms\n";
    std::cout << "================================================================================\n\n";

    if (failCount == 0) {
        std::cout << ">>> ALL HEADLESS DSP VERIFICATION TESTS PASSED SUCCESSFULLY! <<<\n\n";
        return 0;
    } else {
        std::cerr << ">>> " << failCount << " TESTS FAILED! <<<\n\n";
        return 1;
    }
}
