# Unit Tests Configuration
cmake_minimum_required(VERSION 3.14)

# Include Google Test using FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Test sources
set(TEST_SOURCES
    test_health_handler.cpp
    test_version_handler.cpp
    test_swagger_handler.cpp
    test_watchdog_handler.cpp
    test_endpoints_handler.cpp
    test_main.cpp
)

# Create test executable
add_executable(edge_ai_api_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(edge_ai_api_tests PRIVATE
    gtest_main
    gtest
)

# Link Drogon (same as main project)
if(TARGET Drogon::Drogon)
    target_link_libraries(edge_ai_api_tests PRIVATE Drogon::Drogon)
elseif(TARGET drogon)
    target_link_libraries(edge_ai_api_tests PRIVATE drogon)
endif()

# Link source files (we need to compile them for tests)
target_sources(edge_ai_api_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/api/health_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/api/version_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/api/swagger_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/api/watchdog_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/api/endpoints_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/watchdog.cpp
    ${CMAKE_SOURCE_DIR}/src/core/health_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/endpoint_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/request_middleware.cpp
)

# Link jsoncpp if available
if(TARGET jsoncpp_lib)
    target_link_libraries(edge_ai_api_tests PRIVATE jsoncpp_lib)
elseif(Jsoncpp_FOUND)
    target_link_libraries(edge_ai_api_tests PRIVATE ${JSONCPP_LIBRARIES})
    target_include_directories(edge_ai_api_tests PRIVATE ${JSONCPP_INCLUDE_DIRS})
endif()

# Set compile definitions (same as main project)
target_compile_definitions(edge_ai_api_tests PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
    BUILD_TIME="Test Build"
    GIT_COMMIT="test"
)

# Enable testing
enable_testing()
add_test(NAME edge_ai_api_tests COMMAND edge_ai_api_tests)

