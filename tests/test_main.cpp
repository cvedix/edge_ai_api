#include <gtest/gtest.h>
#include <iostream>

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

