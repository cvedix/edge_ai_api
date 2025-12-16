cmake_minimum_required(VERSION 3.14)
project(edge_ai_api VERSION 2025.0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position-independent code globally (required for shared libraries)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
        
        # CMAKE_POSITION_INDEPENDENT_CODE is already set globally above
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
# Plog Logging Library (header-only)
# ============================================
if(AUTO_DOWNLOAD_DEPENDENCIES)
    message(STATUS "Downloading Plog logging library...")
    include(FetchContent)
    
    FetchContent_Declare(
        plog
        GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
        GIT_TAG        1.1.11
        GIT_SHALLOW    TRUE
    )
    
    FetchContent_MakeAvailable(plog)
    
    # Plog is header-only, so we just need to add the include directory
    set(PLOG_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/plog-src/include" CACHE PATH "Plog include directory" FORCE)
    
    message(STATUS "✓ Plog downloaded and configured successfully")
    message(STATUS "  Include: ${PLOG_INCLUDE_DIR}")
else()
    # Try to find system-installed Plog
    find_path(PLOG_INCLUDE_DIR plog/Log.h
        PATHS
            /usr/include
            /usr/local/include
            ${CMAKE_SOURCE_DIR}/third_party/plog/include
    )
    
    if(PLOG_INCLUDE_DIR)
        message(STATUS "✓ Found system Plog: ${PLOG_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR 
            "Plog not found. Please install Plog or enable auto-download:\n"
            "  cmake .. -DAUTO_DOWNLOAD_DEPENDENCIES=ON")
    endif()
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
    src/api/solution_handler.cpp
    src/api/group_handler.cpp
    src/api/config_handler.cpp
    src/api/node_handler.cpp
    src/api/system_info_handler.cpp
    src/api/log_handler.cpp
    src/models/model_upload_handler.cpp
    src/videos/video_upload_handler.cpp
    src/fonts/font_upload_handler.cpp
    src/core/watchdog.cpp
    src/core/health_monitor.cpp
    src/core/endpoint_monitor.cpp
    src/core/request_middleware.cpp
    src/core/cors_filter.cpp
    src/core/cors_helper.cpp
    src/core/ai_processor.cpp
    src/core/ai_watchdog.cpp
    src/core/backpressure_controller.cpp
    src/instances/instance_registry.cpp
    src/instances/queue_monitor.cpp
    src/solutions/solution_registry.cpp
    src/groups/group_registry.cpp
    src/core/pipeline_builder.cpp
    src/utils/cvedix_mqtt_client_impl.cpp
    src/instances/instance_storage.cpp
    src/solutions/solution_storage.cpp
    src/groups/group_storage.cpp
    src/core/node_pool_manager.cpp
    src/core/node_storage.cpp
    src/core/node_template_registry.cpp
    src/config/system_config.cpp
    src/core/uuid_generator.cpp
    src/core/platform_detector.cpp
    src/core/log_manager.cpp
    src/models/create_instance_request.cpp
    src/models/update_instance_request.cpp
    src/models/solution_config.cpp
    src/models/group_info.cpp
    src/api/group_handler.cpp
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

# Set compile options for this target (modern CMake way)
target_compile_options(edge_ai_api PRIVATE -Wall -Wextra)

# Set RPATH configuration
# Use RPATH (not RUNPATH) so it works even without LD_LIBRARY_PATH
# This ensures the executable can find libraries at runtime
set_target_properties(edge_ai_api PROPERTIES
    # Use BUILD_RPATH when running from build directory
    BUILD_WITH_INSTALL_RPATH FALSE
    BUILD_RPATH_USE_ORIGIN TRUE
    # RPATH for build directory (Drogon libraries in build/lib)
    BUILD_RPATH "${CMAKE_BINARY_DIR}/lib;/usr/local/lib"
    # RPATH for installed executable
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;/usr/local/lib"
    INSTALL_RPATH_USE_LINK_PATH TRUE
    # Force use of RPATH instead of RUNPATH
    LINK_FLAGS "-Wl,--disable-new-dtags"
)

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

# ============================================
# Cereal (required by CVEDIX SDK)
# ============================================
# CVEDIX SDK expects cereal at ../../../third_party/cereal relative to its headers
# We need to create a symlink structure or add include path
message(STATUS "Checking for Cereal library...")

# Try to find cereal in system
find_path(CEREAL_INCLUDE_DIR cereal/cereal.hpp
    PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_BINARY_DIR}/_deps/cereal-src/include
)

if(NOT CEREAL_INCLUDE_DIR AND AUTO_DOWNLOAD_DEPENDENCIES)
    message(STATUS "Cereal not found. Downloading headers only...")
    include(FetchContent)
    
    # Download cereal source without building (header-only library)
    FetchContent_Declare(
        cereal
        GIT_REPOSITORY https://github.com/USCiLab/cereal.git
        GIT_TAG        v1.3.2
        GIT_SHALLOW    TRUE
    )
    
    # Only fetch, don't configure/build (cereal is header-only)
    FetchContent_GetProperties(cereal)
    if(NOT cereal_POPULATED)
        FetchContent_Populate(cereal)
        set(CEREAL_INCLUDE_DIR "${cereal_SOURCE_DIR}/include" CACHE PATH "Cereal include directory" FORCE)
        message(STATUS "✓ Cereal headers downloaded to: ${CEREAL_INCLUDE_DIR}")
    endif()
endif()

if(CEREAL_INCLUDE_DIR)
    message(STATUS "✓ Found Cereal: ${CEREAL_INCLUDE_DIR}")
    
    # CVEDIX SDK expects cereal at ../../../third_party/cereal relative to /opt/cvedix/include/cvedix/nodes/broker/cereal_archive/
    # This means it looks for /opt/cvedix/include/cvedix/third_party/cereal/cereal.hpp
    # Create the exact directory structure CVEDIX SDK expects in build directory
    set(CVEDIX_THIRD_PARTY_DIR "${CMAKE_BINARY_DIR}/cvedix_third_party")
    set(CVEDIX_CEREAL_DIR "${CVEDIX_THIRD_PARTY_DIR}/cereal")
    file(MAKE_DIRECTORY "${CVEDIX_THIRD_PARTY_DIR}")
    
    # Create symlink: build/cvedix_third_party/cereal -> cereal include dir
    if(NOT EXISTS "${CVEDIX_CEREAL_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CEREAL_INCLUDE_DIR}/cereal"
            "${CVEDIX_CEREAL_DIR}"
            RESULT_VARIABLE SYMLINK_RESULT
            ERROR_VARIABLE SYMLINK_ERROR
            OUTPUT_QUIET
        )
        if(NOT SYMLINK_RESULT EQUAL 0)
            message(WARNING "⚠ Failed to create cereal symlink in build directory: ${SYMLINK_ERROR}")
            message(WARNING "  This may cause compilation errors. Run: sudo ./scripts/fix_all_symlinks.sh")
        endif()
    endif()
    
    # Check if system symlink exists (required for CVEDIX SDK)
    # Check both /usr/include/cvedix and /opt/cvedix/include/cvedix locations
    set(CEREAL_SYMLINK_FOUND FALSE)
    if(EXISTS "/usr/include/cvedix/third_party/cereal")
        message(STATUS "✓ Cereal symlink found at /usr/include/cvedix/third_party/cereal")
        set(CEREAL_SYMLINK_FOUND TRUE)
    elseif(EXISTS "/opt/cvedix/include/cvedix/third_party/cereal")
        message(STATUS "✓ Cereal symlink found at /opt/cvedix/include/cvedix/third_party/cereal")
        set(CEREAL_SYMLINK_FOUND TRUE)
    endif()
    
    if(NOT CEREAL_SYMLINK_FOUND)
        message(WARNING "")
        message(WARNING "⚠ CVEDIX SDK requires cereal symlink")
        if(EXISTS "/opt/cvedix/include/cvedix")
            message(WARNING "  For /opt/cvedix installation:")
            message(WARNING "    sudo mkdir -p /opt/cvedix/include/cvedix/third_party")
            message(WARNING "    sudo ln -sf ${CEREAL_INCLUDE_DIR}/cereal /opt/cvedix/include/cvedix/third_party/cereal")
        else()
            message(WARNING "  For /usr/include installation:")
            message(WARNING "    Run: sudo bash ${CMAKE_SOURCE_DIR}/scripts/fix_all_symlinks.sh")
            message(WARNING "  Or manually:")
            message(WARNING "    sudo mkdir -p /usr/include/cvedix/third_party")
            message(WARNING "    sudo ln -sf ${CEREAL_INCLUDE_DIR}/cereal /usr/include/cvedix/third_party/cereal")
        endif()
        message(WARNING "")
    endif()
    message(STATUS "✓ Configured cereal paths for CVEDIX SDK")
else()
    message(WARNING "Cereal not found. CVEDIX SDK features may not work.")
    message(WARNING "To install: sudo apt-get install libcereal-dev (if available) or enable AUTO_DOWNLOAD_DEPENDENCIES")
endif()

# ============================================
# cpp-base64 (required by CVEDIX SDK)
# ============================================
# CVEDIX SDK expects cpp_base64 at ../../../third_party/cpp_base64 relative to its headers
# This means it looks for /opt/cvedix/include/cvedix/third_party/cpp_base64/base64.h
message(STATUS "Checking for cpp-base64 library...")

# Try to find cpp-base64 in system
find_path(CPP_BASE64_INCLUDE_DIR base64.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/cvedix/include/cvedix/third_party/cpp_base64
        ${CMAKE_BINARY_DIR}/_deps/cpp-base64-src
)

if(NOT CPP_BASE64_INCLUDE_DIR AND AUTO_DOWNLOAD_DEPENDENCIES)
    message(STATUS "cpp-base64 not found. Downloading headers only...")
    include(FetchContent)
    
    # Download cpp-base64 source without building (header-only library)
    FetchContent_Declare(
        cpp-base64
        GIT_REPOSITORY https://github.com/ReneNyffenegger/cpp-base64.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    
    # Only fetch, don't configure/build (cpp-base64 is header-only)
    FetchContent_GetProperties(cpp-base64)
    if(NOT cpp-base64_POPULATED)
        FetchContent_Populate(cpp-base64)
        set(CPP_BASE64_INCLUDE_DIR "${cpp-base64_SOURCE_DIR}" CACHE PATH "cpp-base64 include directory" FORCE)
        message(STATUS "✓ cpp-base64 headers downloaded to: ${CPP_BASE64_INCLUDE_DIR}")
    endif()
endif()

if(CPP_BASE64_INCLUDE_DIR)
    message(STATUS "✓ Found cpp-base64: ${CPP_BASE64_INCLUDE_DIR}")
    
    # CVEDIX SDK expects cpp_base64 at ../../../third_party/cpp_base64 relative to /opt/cvedix/include/cvedix/nodes/broker/
    # This means it looks for /opt/cvedix/include/cvedix/third_party/cpp_base64/base64.h
    # Create the exact directory structure CVEDIX SDK expects in build directory
    set(CVEDIX_THIRD_PARTY_DIR "${CMAKE_BINARY_DIR}/cvedix_third_party")
    set(CVEDIX_CPP_BASE64_DIR "${CVEDIX_THIRD_PARTY_DIR}/cpp_base64")
    file(MAKE_DIRECTORY "${CVEDIX_THIRD_PARTY_DIR}")
    
    # Create symlink: build/cvedix_third_party/cpp_base64 -> cpp-base64 source dir
    if(NOT EXISTS "${CVEDIX_CPP_BASE64_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CPP_BASE64_INCLUDE_DIR}"
            "${CVEDIX_CPP_BASE64_DIR}"
            RESULT_VARIABLE SYMLINK_RESULT
            ERROR_VARIABLE SYMLINK_ERROR
            OUTPUT_QUIET
        )
        if(NOT SYMLINK_RESULT EQUAL 0)
            message(WARNING "⚠ Failed to create cpp-base64 symlink in build directory: ${SYMLINK_ERROR}")
            message(WARNING "  This may cause compilation errors. Run: sudo ./scripts/fix_all_symlinks.sh")
        endif()
    endif()
    
    # Check if system symlink exists (required for CVEDIX SDK)
    # Check both /usr/include/cvedix and /opt/cvedix/include/cvedix locations
    set(CPP_BASE64_SYMLINK_FOUND FALSE)
    if(EXISTS "/usr/include/cvedix/third_party/cpp_base64/base64.h")
        message(STATUS "✓ cpp-base64 symlink found at /usr/include/cvedix/third_party/cpp_base64")
        set(CPP_BASE64_SYMLINK_FOUND TRUE)
    elseif(EXISTS "/opt/cvedix/include/cvedix/third_party/cpp_base64/base64.h")
        message(STATUS "✓ cpp-base64 symlink found at /opt/cvedix/include/cvedix/third_party/cpp_base64")
        set(CPP_BASE64_SYMLINK_FOUND TRUE)
    endif()
    
    if(NOT CPP_BASE64_SYMLINK_FOUND)
        message(WARNING "")
        message(WARNING "⚠ CVEDIX SDK requires cpp-base64 symlink")
        if(EXISTS "/opt/cvedix/include/cvedix")
            message(WARNING "  For /opt/cvedix installation:")
            message(WARNING "    sudo mkdir -p /opt/cvedix/include/cvedix/third_party/cpp_base64")
            message(WARNING "    sudo ln -sf ${CPP_BASE64_INCLUDE_DIR}/base64.h /opt/cvedix/include/cvedix/third_party/cpp_base64/base64.h")
        else()
            message(WARNING "  For /usr/include installation:")
            message(WARNING "    sudo mkdir -p /usr/include/cvedix/third_party/cpp_base64")
            message(WARNING "    sudo ln -sf ${CPP_BASE64_INCLUDE_DIR}/base64.h /usr/include/cvedix/third_party/cpp_base64/base64.h")
        endif()
        message(WARNING "")
    endif()
    message(STATUS "✓ Configured cpp-base64 paths for CVEDIX SDK")
else()
    message(WARNING "cpp-base64 not found. CVEDIX SDK enhanced console broker features may not work.")
    message(WARNING "To install: enable AUTO_DOWNLOAD_DEPENDENCIES or manually download from https://github.com/ReneNyffenegger/cpp-base64")
endif()

# ============================================
# CVEDIX SDK (required for AI processing)
# ============================================
# CVEDIX SDK is expected to be installed system-wide (not downloaded via FetchContent)
# CMake will search for it in standard system paths:
#   - Config files: /usr/lib/cmake/cvedix/cvedix-config.cmake
#   - Headers: /usr/include/cvedix/
#   - Libraries: /usr/lib/ (or /usr/lib64/)
#
# CVEDIX SDK may also be installed in /opt/cvedix/ (non-standard location)
# We check both locations and add /opt/cvedix to CMAKE_PREFIX_PATH if it exists
#
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

# Check if CVEDIX SDK is installed in /opt/cvedix (standard location for this project)
if(EXISTS "/opt/cvedix/lib/cmake/cvedix/cvedix-config.cmake")
    message(STATUS "Found CVEDIX SDK in /opt/cvedix, adding to CMAKE_PREFIX_PATH")
    list(APPEND CMAKE_PREFIX_PATH "/opt/cvedix")
    message(STATUS "✓ Added /opt/cvedix to CMAKE_PREFIX_PATH")
    
    # Create symlink structure for CVEDIX SDK third_party dependencies
    # CVEDIX SDK headers use relative paths like ../../third_party/cpp_base64/base64.h
    # We need to create the structure in /opt/cvedix/include/cvedix/third_party/
    if(CPP_BASE64_INCLUDE_DIR AND NOT EXISTS "/opt/cvedix/include/cvedix/third_party/cpp_base64/base64.h")
        message(STATUS "Creating cpp_base64 symlink structure for CVEDIX SDK...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory "/opt/cvedix/include/cvedix/third_party/cpp_base64"
            RESULT_VARIABLE MKDIR_RESULT
            ERROR_VARIABLE MKDIR_ERROR
            OUTPUT_QUIET
        )
        if(MKDIR_RESULT EQUAL 0)
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${CPP_BASE64_INCLUDE_DIR}/base64.h"
                "/opt/cvedix/include/cvedix/third_party/cpp_base64/base64.h"
                RESULT_VARIABLE SYMLINK_RESULT
                ERROR_VARIABLE SYMLINK_ERROR
                OUTPUT_QUIET
            )
            if(SYMLINK_RESULT EQUAL 0)
                message(STATUS "✓ Created cpp_base64 symlink in /opt/cvedix/include/cvedix/third_party/cpp_base64/")
            else()
                message(WARNING "⚠ Could not create cpp_base64 symlink (may need sudo):")
                message(WARNING "  Error: ${SYMLINK_ERROR}")
                message(WARNING "  Run: sudo ./scripts/fix_all_symlinks.sh")
            endif()
        else()
            message(WARNING "⚠ Could not create directory (may need sudo):")
            message(WARNING "  Error: ${MKDIR_ERROR}")
            message(WARNING "  Run: sudo ./scripts/fix_all_symlinks.sh")
        endif()
    endif()
endif()

# Fix tinyexpr library path before find_package
# CVEDIX SDK expects libtinyexpr.so in /usr/lib but it may be in /opt/cvedix/lib
# Create symlink in build directory to work around this issue
if(EXISTS "/opt/cvedix/lib/libtinyexpr.so" AND NOT EXISTS "/usr/lib/libtinyexpr.so")
    # Create a symlink in build directory
    set(TINYEXPR_BUILD_LIB "${CMAKE_BINARY_DIR}/libtinyexpr.so")
    if(NOT EXISTS "${TINYEXPR_BUILD_LIB}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink
            "/opt/cvedix/lib/libtinyexpr.so"
            "${TINYEXPR_BUILD_LIB}"
            RESULT_VARIABLE SYMLINK_RESULT
            ERROR_VARIABLE SYMLINK_ERROR
            OUTPUT_QUIET
        )
        if(SYMLINK_RESULT EQUAL 0)
            message(STATUS "✓ Created tinyexpr symlink in build directory")
        else()
            message(WARNING "⚠ Failed to create tinyexpr symlink in build directory: ${SYMLINK_ERROR}")
        endif()
    endif()
    # Also try to create symlink in /usr/lib (may require sudo, but try anyway)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "/opt/cvedix/lib/libtinyexpr.so"
        "/usr/lib/libtinyexpr.so"
        RESULT_VARIABLE SYMLINK_USR_RESULT
        ERROR_VARIABLE SYMLINK_USR_ERROR
        OUTPUT_QUIET
    )
    if(SYMLINK_USR_RESULT EQUAL 0)
        message(STATUS "✓ Created tinyexpr symlink in /usr/lib")
    else()
        message(WARNING "⚠ Could not create symlink in /usr/lib (may need sudo):")
        message(WARNING "  Error: ${SYMLINK_USR_ERROR}")
        message(WARNING "  Run: sudo ./scripts/fix_all_symlinks.sh")
    endif()
endif()

find_package(cvedix REQUIRED)

# Add all include directories for edge_ai_api target (modern CMake way)
# Base project include directory
target_include_directories(edge_ai_api PRIVATE ${CMAKE_SOURCE_DIR}/include)

# OpenCV include directories
if(OpenCV_FOUND)
    if(TARGET opencv_core)
        # Modern CMake way - targets handle includes automatically
        # But we may still need to add include dirs for pkg-config path
        if(OpenCV_INCLUDE_DIRS)
            target_include_directories(edge_ai_api PRIVATE ${OpenCV_INCLUDE_DIRS})
        endif()
    else()
        # Fallback to pkg-config libraries
        if(OpenCV_INCLUDE_DIRS)
            target_include_directories(edge_ai_api PRIVATE ${OpenCV_INCLUDE_DIRS})
        endif()
    endif()
endif()

# Plog include directory
if(PLOG_INCLUDE_DIR)
    target_include_directories(edge_ai_api PRIVATE ${PLOG_INCLUDE_DIR})
endif()

# CVEDIX SDK include directory (if installed in /opt/cvedix)
if(EXISTS "/opt/cvedix/include")
    target_include_directories(edge_ai_api PRIVATE "/opt/cvedix/include")
endif()

# Add cereal and cpp-base64 include paths for CVEDIX SDK
# CVEDIX SDK headers include cereal with relative path ../../../third_party/cereal/cereal.hpp
# CVEDIX SDK headers include base64 with relative path ../../../third_party/cpp_base64/base64.h
# We've created the structure in build directory, now add it to include paths
if(CEREAL_INCLUDE_DIR)
    # Add build directory so compiler can resolve cvedix_third_party/cereal
    target_include_directories(edge_ai_api PRIVATE "${CMAKE_BINARY_DIR}")
    target_include_directories(edge_ai_api PRIVATE "${CEREAL_INCLUDE_DIR}")
    message(STATUS "✓ Added cereal include paths for CVEDIX SDK")
endif()

if(CPP_BASE64_INCLUDE_DIR)
    # Add build directory so compiler can resolve cvedix_third_party/cpp_base64
    # CMAKE_BINARY_DIR is already added above for cereal, so we don't need to add it again
    target_include_directories(edge_ai_api PRIVATE "${CPP_BASE64_INCLUDE_DIR}")
    message(STATUS "✓ Added cpp-base64 include paths for CVEDIX SDK")
endif()

target_link_libraries(edge_ai_api PRIVATE cvedix::cvedix_instance_sdk)
message(STATUS "✓ CVEDIX SDK linked: cvedix::cvedix_instance_sdk")

# Link OpenCV (for CVEDIX SDK)
if(OpenCV_FOUND)
    if(TARGET opencv_core)
        # Modern CMake way
        # CVEDIX SDK requires opencv_dnn module (for cv::dnn::Net)
        # Only link the modules we actually need (avoid CUDA modules if not built)
        target_link_libraries(edge_ai_api PRIVATE 
            opencv_core
            opencv_imgproc
            opencv_imgcodecs
            opencv_videoio
            opencv_highgui
            opencv_dnn
        )
        # Explicitly exclude CUDA modules if they don't exist
        # This prevents linker errors when CUDA modules are not available
    else()
        # Fallback to pkg-config libraries, but filter out CUDA modules
        set(OPENCV_LIBS ${OpenCV_LIBRARIES})
        # Remove CUDA modules from the list
        list(FILTER OPENCV_LIBS EXCLUDE REGEX "opencv_cuda")
        target_link_libraries(edge_ai_api PRIVATE ${OPENCV_LIBS})
    endif()
    message(STATUS "✓ OpenCV linked to edge_ai_api")
endif()

# ============================================
# Fix missing libopencv_freetype.so.410
# ============================================
# CVEDIX SDK requires libopencv_freetype.so.410 but OpenCV 4.10 may not have it
# Copy from OpenCV 4.6 if available (temporary workaround)
if(EXISTS "/usr/lib/x86_64-linux-gnu/libopencv_freetype.so.4.6.0")
    set(FREETYPE_410_PATH "${CMAKE_BINARY_DIR}/lib/libopencv_freetype.so.410")
    add_custom_command(TARGET edge_ai_api POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "/usr/lib/x86_64-linux-gnu/libopencv_freetype.so.4.6.0"
            "${FREETYPE_410_PATH}"
        COMMENT "Copying libopencv_freetype.so.410 for CVEDIX SDK compatibility"
    )
    message(STATUS "✓ Will copy libopencv_freetype.so.410 to build/lib")
endif()

# ============================================
# hwinfo - Hardware Information Library
# ============================================
# Try submodule first (if available)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/hwinfo/CMakeLists.txt")
    message(STATUS "Adding hwinfo submodule...")
    add_subdirectory(third_party/hwinfo)
    target_link_libraries(edge_ai_api PRIVATE lfreist-hwinfo::hwinfo)
    message(STATUS "✓ hwinfo linked to edge_ai_api")
    # Add system_info_handler if hwinfo is available
    target_sources(edge_ai_api PRIVATE src/api/system_info_handler.cpp)
    # Define macro to enable SystemInfoHandler in main.cpp
    target_compile_definitions(edge_ai_api PRIVATE ENABLE_SYSTEM_INFO_HANDLER)
    message(STATUS "✓ system_info_handler enabled (hwinfo available)")
elseif(AUTO_DOWNLOAD_DEPENDENCIES)
    # Use FetchContent to download hwinfo automatically
    message(STATUS "hwinfo submodule not found. Downloading via FetchContent...")
    
    # Ensure FetchContent is included (may have been included for other dependencies)
    if(NOT DEFINED FetchContent_MakeAvailable)
        include(FetchContent)
    endif()
    
    FetchContent_Declare(
        hwinfo
        GIT_REPOSITORY https://github.com/lfreist/hwinfo.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    
    FetchContent_MakeAvailable(hwinfo)
    
    target_link_libraries(edge_ai_api PRIVATE lfreist-hwinfo::hwinfo)
    message(STATUS "✓ hwinfo downloaded and linked to edge_ai_api")
    # Add system_info_handler if hwinfo is available
    target_sources(edge_ai_api PRIVATE src/api/system_info_handler.cpp)
    # Define macro to enable SystemInfoHandler in main.cpp
    target_compile_definitions(edge_ai_api PRIVATE ENABLE_SYSTEM_INFO_HANDLER)
    message(STATUS "✓ system_info_handler enabled (hwinfo available)")
else()
    message(WARNING "hwinfo not found. System info endpoints will not work.")
    message(WARNING "Options:")
    message(WARNING "  1. Initialize submodule: git submodule update --init --recursive")
    message(WARNING "  2. Enable auto-download: cmake -DAUTO_DOWNLOAD_DEPENDENCIES=ON ..")
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
# CVEDIX SDK Feature Flags
# ============================================
# Check if CVEDIX SDK was built with GStreamer support
# GStreamer is required for RTSP, RTMP, Image, and UDP source nodes
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER QUIET gstreamer-1.0)
    if(GSTREAMER_FOUND)
        target_compile_definitions(edge_ai_api PRIVATE CVEDIX_WITH_GSTREAMER)
        message(STATUS "✓ GStreamer support enabled (CVEDIX_WITH_GSTREAMER)")
    else()
        message(WARNING "⚠ GStreamer not found. RTSP/RTMP/Image/UDP source nodes will not be available.")
        message(WARNING "  To install: sudo apt-get install libgstreamer1.0-dev")
    endif()
else()
    # If pkg-config not available, check for GStreamer libraries directly
    find_library(GSTREAMER_LIBRARY
        NAMES gstreamer-1.0
        PATHS
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/local/lib
    )
    if(GSTREAMER_LIBRARY)
        target_compile_definitions(edge_ai_api PRIVATE CVEDIX_WITH_GSTREAMER)
        message(STATUS "✓ GStreamer support enabled (CVEDIX_WITH_GSTREAMER)")
    else()
        message(WARNING "⚠ GStreamer not found. RTSP/RTMP/Image/UDP source nodes will not be available.")
    endif()
endif()

# Check if CVEDIX SDK was built with MQTT support
# This is typically defined in CVEDIX SDK's config file, but we can also enable it here
option(CVEDIX_WITH_MQTT "Enable MQTT support (requires CVEDIX SDK built with MQTT)" ON)

if(CVEDIX_WITH_MQTT)
    target_compile_definitions(edge_ai_api PRIVATE CVEDIX_WITH_MQTT)
    message(STATUS "✓ MQTT support enabled (CVEDIX_WITH_MQTT)")
    
    # cvedix_mqtt_client uses libmosquitto, need to link it
    find_library(MOSQUITTO_LIBRARY
        NAMES mosquitto
        PATHS
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/local/lib
            /opt/cvedix/lib
    )
    
    if(MOSQUITTO_LIBRARY)
        target_link_libraries(edge_ai_api PRIVATE ${MOSQUITTO_LIBRARY})
        message(STATUS "✓ Linked mosquitto library: ${MOSQUITTO_LIBRARY}")
    else()
        # Try pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(MOSQUITTO QUIET mosquitto)
            if(MOSQUITTO_FOUND)
                target_link_libraries(edge_ai_api PRIVATE ${MOSQUITTO_LIBRARIES})
                target_include_directories(edge_ai_api PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
                message(STATUS "✓ Linked mosquitto library via pkg-config: ${MOSQUITTO_LIBRARIES}")
            else()
                message(WARNING "⚠ mosquitto library not found. MQTT features may not work.")
                message(WARNING "  To install: sudo apt-get install libmosquitto-dev")
            endif()
        else()
            message(WARNING "⚠ mosquitto library not found. MQTT features may not work.")
            message(WARNING "  To install: sudo apt-get install libmosquitto-dev")
        endif()
    endif()
else()
    message(STATUS "⚠ MQTT support disabled")
endif()

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
