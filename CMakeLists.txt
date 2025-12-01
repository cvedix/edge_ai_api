cmake_minimum_required(VERSION 3.14)
project(edge_ai_api VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# ============================================
# Check System Dependencies
# ============================================
# Dependencies are checked directly below

# ============================================
# Drogon HTTP Framework Dependencies
# ============================================
# Option: Auto-download missing dependencies using FetchContent
option(AUTO_DOWNLOAD_DEPENDENCIES "Automatically download missing dependencies" ON)

# Check for OpenSSL (required)
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
    message(FATAL_ERROR 
        "OpenSSL not found. Drogon requires OpenSSL.\n"
        "To install: sudo apt-get install libssl-dev")
endif()

# Check for UUID (required by Drogon)
find_library(UUID_LIBRARY uuid)
if(NOT UUID_LIBRARY)
    if(AUTO_DOWNLOAD_DEPENDENCIES)
        message(STATUS "UUID library not found. Drogon can work without it, but some features may be limited.")
        message(STATUS "To install: sudo apt-get install uuid-dev")
        # UUID is usually small, we can continue without it
        set(UUID_LIBRARY "")
    else()
        message(WARNING 
            "UUID library not found. Some Drogon features may not work.\n"
            "To install: sudo apt-get install uuid-dev")
    endif()
else()
    message(STATUS "✓ Found UUID library")
endif()

# Check for jsoncpp - try to download if not found
find_package(Jsoncpp QUIET)
if(NOT Jsoncpp_FOUND)
    if(AUTO_DOWNLOAD_DEPENDENCIES)
        message(STATUS "Jsoncpp not found. Attempting to download using FetchContent...")
        include(FetchContent)
        
        # Enable position-independent code for jsoncpp (required for shared libraries)
        # This must be set before FetchContent_MakeAvailable
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        
        FetchContent_Declare(
            jsoncpp
            GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
            GIT_TAG        1.9.5
            GIT_SHALLOW    TRUE
        )
        
        # Set jsoncpp build options
        set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
        set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
        set(JSONCPP_WITH_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)
        set(JSONCPP_WITH_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)  # Disable to avoid path issues
        set(JSONCPP_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/jsoncpp-src/include" CACHE PATH "" FORCE)
        
        # Build jsoncpp as shared library with -fPIC for linking into shared libraries
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
        
        FetchContent_MakeAvailable(jsoncpp)
        
        # Ensure jsoncpp is built with -fPIC
        if(TARGET jsoncpp_lib)
            set_target_properties(jsoncpp_lib PROPERTIES
                POSITION_INDEPENDENT_CODE ON
            )
        endif()
        if(TARGET jsoncpp_static)
            set_target_properties(jsoncpp_static PROPERTIES
                POSITION_INDEPENDENT_CODE ON
            )
        endif()
        
        # After FetchContent_MakeAvailable, jsoncpp should be available
        # Use source include directory (always exists)
        set(JSONCPP_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/_deps/jsoncpp-src/include" CACHE PATH "Jsoncpp include directories" FORCE)
        
        # For library, jsoncpp_lib target should be available
        # The actual library file is built in build/lib/ directory, not _deps/jsoncpp-build/
        # Find the actual library file location
        find_library(JSONCPP_LIBRARIES
            NAMES jsoncpp
            PATHS 
                ${CMAKE_BINARY_DIR}/lib
                ${CMAKE_BINARY_DIR}/_deps/jsoncpp-build
                ${CMAKE_BINARY_DIR}/_deps/jsoncpp-build/lib
            NO_DEFAULT_PATH
        )
        
        # If not found yet (not built), use expected path in build/lib/
        if(NOT JSONCPP_LIBRARIES)
            set(JSONCPP_LIBRARIES "${CMAKE_BINARY_DIR}/lib/libjsoncpp.a")
        endif()
        
        # Verify include directory exists
        if(NOT EXISTS "${JSONCPP_INCLUDE_DIRS}/json/json.h")
            message(FATAL_ERROR 
                "Jsoncpp include directory not found: ${JSONCPP_INCLUDE_DIRS}/json/json.h")
        endif()
        
        # Set variables that Drogon's FindJsoncpp.cmake expects
        set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARIES} CACHE FILEPATH "Jsoncpp library" FORCE)
        set(Jsoncpp_FOUND TRUE CACHE BOOL "Jsoncpp found" FORCE)
        
        message(STATUS "✓ Jsoncpp downloaded and configured successfully")
        message(STATUS "  Include: ${JSONCPP_INCLUDE_DIRS}")
        message(STATUS "  Library target: jsoncpp_lib (will be built automatically)")
        message(STATUS "  Library path: ${JSONCPP_LIBRARIES}")
    else()
        message(FATAL_ERROR 
            "Jsoncpp not found. Drogon requires jsoncpp.\n"
            "To install: sudo apt-get install libjsoncpp-dev\n"
            "Or enable auto-download: cmake .. -DAUTO_DOWNLOAD_DEPENDENCIES=ON")
    endif()
else()
    message(STATUS "✓ Found system Jsoncpp")
endif()

# ============================================
# Drogon HTTP Framework
# ============================================
# Option: Use FetchContent to automatically download and build Drogon
# Set DROGON_USE_FETCHCONTENT=OFF to use system-installed Drogon instead
option(DROGON_USE_FETCHCONTENT "Use FetchContent to download Drogon automatically" ON)

if(DROGON_USE_FETCHCONTENT)
    message(STATUS "Using FetchContent to download Drogon automatically...")
    
    # Ensure FetchContent is included (may have been included for jsoncpp)
    if(NOT DEFINED FetchContent_MakeAvailable)
        include(FetchContent)
    endif()
    
    # Configure Drogon version (use latest stable release)
    set(DROGON_VERSION "v1.9.0" CACHE STRING "Drogon version to fetch")
    
    FetchContent_Declare(
        drogon
        GIT_REPOSITORY https://github.com/drogonframework/drogon.git
        GIT_TAG        ${DROGON_VERSION}
        GIT_SHALLOW   TRUE
    )
    
    # Set Drogon build options
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_ORM OFF CACHE BOOL "" FORCE)  # Disable ORM if not needed
    
    # UUID is optional for Drogon, but FindUUID.cmake may fail if not found
    # We'll let Drogon handle it - it should work without UUID
    
    # Make Drogon available (dependencies should be ready now)
    FetchContent_MakeAvailable(drogon)
    
    message(STATUS "✓ Drogon fetched and built successfully")
else()
    message(STATUS "Looking for system-installed Drogon...")
    find_package(Drogon REQUIRED)
    
    if(NOT Drogon_FOUND)
        message(FATAL_ERROR 
            "Drogon not found. Please install Drogon or set DROGON_USE_FETCHCONTENT=ON")
    endif()
endif()

# ============================================
# OpenCV (required for CVEDIX SDK)
# ============================================
find_package(OpenCV QUIET)
if(NOT OpenCV_FOUND)
    # Try to find using pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OpenCV QUIET opencv4)
        if(OpenCV_FOUND)
            message(STATUS "✓ Found OpenCV via pkg-config")
            set(OpenCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
            set(OpenCV_LIBRARIES ${OpenCV_LIBRARIES})
        endif()
    endif()
endif()

if(NOT OpenCV_FOUND)
    message(WARNING 
        "OpenCV not found. CVEDIX SDK features may not work.\n"
        "To install: sudo apt-get install libopencv-dev")
else()
    message(STATUS "✓ Found OpenCV")
    message(STATUS "  Version: ${OpenCV_VERSION}")
    message(STATUS "  Include dirs: ${OpenCV_INCLUDE_DIRS}")
endif()

# ============================================
# Include directories
# ============================================
include_directories(${CMAKE_SOURCE_DIR}/include)
if(OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()

# ============================================
# OpenSSL for SHA256 (used in cache - optional for now)
# ============================================
# find_package(OpenSSL REQUIRED)  # Uncomment when using cache

# ============================================
# Source files
# ============================================
set(SOURCES
    src/main.cpp
    src/api/health_handler.cpp
    src/api/version_handler.cpp
    src/api/watchdog_handler.cpp
    src/api/endpoints_handler.cpp
    src/api/swagger_handler.cpp
    src/api/create_instance_handler.cpp
    src/api/instance_handler.cpp
    src/models/model_upload_handler.cpp
    src/core/watchdog.cpp
    src/core/health_monitor.cpp
    src/core/endpoint_monitor.cpp
    src/core/request_middleware.cpp
    src/core/ai_processor.cpp
    src/core/ai_watchdog.cpp
    src/instances/instance_registry.cpp
    src/core/solution_registry.cpp
    src/core/pipeline_builder.cpp
    src/instances/instance_storage.cpp
    src/core/uuid_generator.cpp
    src/models/create_instance_request.cpp
    src/models/solution_config.cpp
    # Infrastructure components (available but not used in main yet)
    # src/core/resource_manager.cpp
    # src/core/rate_limiter.cpp
    # src/core/ai_cache.cpp
    # src/core/priority_queue.cpp
    # src/core/circuit_breaker.cpp
    # src/core/performance_monitor.cpp
    # AI handlers (not needed for base code)
    # src/api/ai_handler.cpp
    # src/api/ai_websocket.cpp
    # src/api/metrics_handler.cpp
)

# ============================================
# Create executable
# ============================================
add_executable(edge_ai_api ${SOURCES})

# ============================================
# Link libraries
# ============================================
# Link Drogon - try Drogon::Drogon first, fallback to drogon
if(TARGET Drogon::Drogon)
    target_link_libraries(edge_ai_api PRIVATE Drogon::Drogon)
elseif(TARGET drogon)
    target_link_libraries(edge_ai_api PRIVATE drogon)
else()
    message(FATAL_ERROR "Drogon target not found. Cannot link.")
endif()

# Link CVEDIX SDK
# Workaround: Define check_required_components if not available
# (needed for cvedix-config.cmake which uses this function)
if(NOT COMMAND check_required_components)
    function(check_required_components _package_name)
        foreach(_comp ${ARGN})
            # Check if target exists (for modern CMake packages)
            string(REPLACE "_" "::" _comp_target "${_package_name}::${_comp}")
            if(NOT TARGET "${_comp_target}")
                # Also check variable-based approach
                set(_comp_var "${_package_name}_${_comp}_FOUND")
                if(NOT ${_comp_var})
                    set(${_package_name}_FOUND FALSE)
                    set(${_package_name}_NOT_FOUND_MESSAGE "${_package_name} ${_comp} component not found")
                    if(${_package_name}_FIND_REQUIRED)
                        message(FATAL_ERROR "${${_package_name}_NOT_FOUND_MESSAGE}")
                    else()
                        if(NOT ${_package_name}_FIND_QUIETLY)
                            message(STATUS "${${_package_name}_NOT_FOUND_MESSAGE}")
                        endif()
                    endif()
                endif()
            endif()
        endforeach()
    endfunction()
endif()
find_package(cvedix REQUIRED)
target_link_libraries(edge_ai_api PRIVATE cvedix::cvedix_instance_sdk)
message(STATUS "✓ CVEDIX SDK linked: cvedix::cvedix_instance_sdk")

# Link OpenCV (for CVEDIX SDK)
if(OpenCV_FOUND)
    if(TARGET opencv_core)
        # Modern CMake way
        target_link_libraries(edge_ai_api PRIVATE 
            opencv_core
            opencv_imgproc
            opencv_imgcodecs
            opencv_videoio
            opencv_highgui
        )
    else()
        # Fallback to pkg-config libraries
        target_link_libraries(edge_ai_api PRIVATE ${OpenCV_LIBRARIES})
    endif()
    message(STATUS "✓ OpenCV linked to edge_ai_api")
endif()

# Additional libraries (if needed)
# target_link_libraries(edge_ai_api PRIVATE 
#     OpenSSL::SSL      # Uncomment when using cache
#     OpenSSL::Crypto   # Uncomment when using cache
# )

# ============================================
# Git commit information
# ============================================
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT GIT_COMMIT)
        set(GIT_COMMIT "unknown")
    endif()
else()
    set(GIT_COMMIT "unknown")
endif()

# ============================================
# Build time
# ============================================
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S" UTC)

# ============================================
# Compile definitions
# ============================================
target_compile_definitions(edge_ai_api PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
    BUILD_TIME="${BUILD_TIME}"
    GIT_COMMIT="${GIT_COMMIT}"
)

# ============================================
# Install target (optional)
# ============================================
install(TARGETS edge_ai_api
    RUNTIME DESTINATION bin
)

# ============================================
# Unit Tests
# ============================================
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
