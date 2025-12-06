#include <gtest/gtest.h>
#include "core/logging_flags.h"
#include <atomic>
#include <iostream>

// Define logging flags for tests (they're normally defined in main.cpp)
std::atomic<bool> g_log_api{false};
std::atomic<bool> g_log_instance{false};
std::atomic<bool> g_log_sdk_output{false};

int main(int argc, char **argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "Edge AI API - Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    
    std::cout << "\n========================================" << std::endl;
    if (result == 0) {
        std::cout << "All tests PASSED!" << std::endl;
    } else {
        std::cout << "Some tests FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;
    
    return result;
}

