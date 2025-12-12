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
    test_instance_storage.cpp
    test_instance_update.cpp
    test_instance_status_summary.cpp
    test_instance_get_config.cpp
    test_instance_configure_stream.cpp
    test_solution_handler.cpp
    test_recognition_handler.cpp
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
    ${CMAKE_SOURCE_DIR}/src/api/solution_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/api/instance_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/api/recognition_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/core/watchdog.cpp
    ${CMAKE_SOURCE_DIR}/src/core/health_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/endpoint_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/request_middleware.cpp
    ${CMAKE_SOURCE_DIR}/src/instances/instance_storage.cpp
    ${CMAKE_SOURCE_DIR}/src/instances/instance_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline_builder.cpp
    ${CMAKE_SOURCE_DIR}/src/solutions/solution_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/solutions/solution_storage.cpp
    ${CMAKE_SOURCE_DIR}/src/config/system_config.cpp
    ${CMAKE_SOURCE_DIR}/src/core/platform_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/models/solution_config.cpp
    ${CMAKE_SOURCE_DIR}/src/models/update_instance_request.cpp
    ${CMAKE_SOURCE_DIR}/src/models/create_instance_request.cpp
    ${CMAKE_SOURCE_DIR}/src/core/uuid_generator.cpp
)

# Link jsoncpp if available
if(TARGET jsoncpp_lib)
    target_link_libraries(edge_ai_api_tests PRIVATE jsoncpp_lib)
elseif(Jsoncpp_FOUND)
    target_link_libraries(edge_ai_api_tests PRIVATE ${JSONCPP_LIBRARIES})
    target_include_directories(edge_ai_api_tests PRIVATE ${JSONCPP_INCLUDE_DIRS})
endif()

# Link CVEDIX SDK (required for instance_registry and pipeline_builder)
if(TARGET cvedix::cvedix_instance_sdk)
    target_link_libraries(edge_ai_api_tests PRIVATE cvedix::cvedix_instance_sdk)
    message(STATUS "✓ CVEDIX SDK linked to tests")
endif()

# Link OpenCV (required for CVEDIX SDK)
if(OpenCV_FOUND)
    if(TARGET opencv_core)
        # CVEDIX SDK requires opencv_dnn module (for cv::dnn::Net)
        target_link_libraries(edge_ai_api_tests PRIVATE 
            opencv_core
            opencv_imgproc
            opencv_imgcodecs
            opencv_videoio
            opencv_highgui
            opencv_dnn
        )
    else()
        target_link_libraries(edge_ai_api_tests PRIVATE ${OpenCV_LIBRARIES})
    endif()
    message(STATUS "✓ OpenCV linked to tests")
endif()

# Add include directories for cereal and CVEDIX (same as main project)
# Note: CEREAL_INCLUDE_DIR is set in parent CMakeLists.txt
if(DEFINED CEREAL_INCLUDE_DIR AND CEREAL_INCLUDE_DIR)
    target_include_directories(edge_ai_api_tests PRIVATE "${CMAKE_BINARY_DIR}")
    target_include_directories(edge_ai_api_tests PRIVATE "${CEREAL_INCLUDE_DIR}")
endif()

# Link stdc++fs for std::experimental::filesystem (if needed)
# Some systems require explicit linking for filesystem library
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(edge_ai_api_tests PRIVATE stdc++fs)
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

