cmake_minimum_required(VERSION 3.14)
project(mqtt_json_receiver_sample VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# ============================================
# Options
# ============================================
option(AUTO_DOWNLOAD_DEPENDENCIES "Automatically download missing dependencies" ON)

# ============================================
# Find nlohmann/json
# ============================================
# Try multiple paths where nlohmann/json might be installed
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS
        /usr/include
        /usr/include/nlohmann
        /usr/local/include
        /usr/local/include/nlohmann
        ${CMAKE_SOURCE_DIR}/../third_party/nlohmann
        ${CMAKE_SOURCE_DIR}/../third_party
    NO_DEFAULT_PATH
)

# If not found, try default paths
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
endif()

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    # Try to find in third_party directory of parent project
    if(EXISTS "${CMAKE_SOURCE_DIR}/../third_party/nlohmann/json.hpp")
        set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../third_party")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/../third_party/json.hpp")
        set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../third_party")
    else()
        message(STATUS "")
        message(STATUS "⚠ nlohmann/json not found in system paths")
        message(STATUS "")
        message(STATUS "Có 2 cách để cài đặt:")
        message(STATUS "  1. Cài từ package (KHUYẾN NGHỊ - nhanh hơn):")
        message(STATUS "     sudo apt-get install nlohmann-json3-dev")
        message(STATUS "")
        message(STATUS "  2. Tự động download (có thể mất thời gian):")
        message(STATUS "     Đang thử download...")
        message(STATUS "")
        
        # Try to download using FetchContent
        if(AUTO_DOWNLOAD_DEPENDENCIES)
            include(FetchContent)
            
            FetchContent_Declare(
                nlohmann_json
                GIT_REPOSITORY https://github.com/nlohmann/json.git
                GIT_TAG v3.11.2
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE  # Show progress
            )
            
            # Set timeout to avoid hanging
            set(FETCHCONTENT_QUIET OFF)
            
            FetchContent_MakeAvailable(nlohmann_json)
            
            # nlohmann/json is header-only, so we just need the include directory
            set(NLOHMANN_JSON_INCLUDE_DIR "${nlohmann_json_SOURCE_DIR}/include" CACHE PATH "nlohmann/json include directory" FORCE)
            message(STATUS "✓ nlohmann/json downloaded to: ${NLOHMANN_JSON_INCLUDE_DIR}")
        else()
            message(FATAL_ERROR 
                "nlohmann/json not found.\n"
                "Vui lòng cài đặt: sudo apt-get install nlohmann-json3-dev\n"
                "Hoặc enable auto-download: cmake -DAUTO_DOWNLOAD_DEPENDENCIES=ON ..")
        endif()
    endif()
else()
    message(STATUS "✓ Found nlohmann/json: ${NLOHMANN_JSON_INCLUDE_DIR}")
endif()

# ============================================
# Find mosquitto library
# ============================================
find_library(MOSQUITTO_LIBRARY
    NAMES mosquitto
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /opt/cvedix/lib
)

if(NOT MOSQUITTO_LIBRARY)
    # Try pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MOSQUITTO QUIET mosquitto)
        if(MOSQUITTO_FOUND)
            set(MOSQUITTO_LIBRARY ${MOSQUITTO_LIBRARIES})
            set(MOSQUITTO_INCLUDE_DIRS ${MOSQUITTO_INCLUDE_DIRS})
        endif()
    endif()
endif()

if(NOT MOSQUITTO_LIBRARY)
    message(FATAL_ERROR 
        "mosquitto library not found.\n"
        "To install: sudo apt-get install libmosquitto-dev")
else()
    message(STATUS "✓ Found mosquitto library: ${MOSQUITTO_LIBRARY}")
endif()

# ============================================
# Find cpp-base64 library
# ============================================
message(STATUS "Checking for cpp-base64 library...")

# Try to find cpp-base64 in system
find_path(CPP_BASE64_INCLUDE_DIR base64.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/cvedix/include/cvedix/third_party/cpp_base64
        ${CMAKE_BINARY_DIR}/_deps/cpp-base64-src
        ${CMAKE_SOURCE_DIR}/../build/_deps/cpp-base64-src
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
else()
    message(WARNING "cpp-base64 not found. Some features may not work.")
    message(WARNING "To install: enable AUTO_DOWNLOAD_DEPENDENCIES or manually download from https://github.com/ReneNyffenegger/cpp-base64")
endif()

# ============================================
# Find CVEDIX SDK
# ============================================
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

# Check if CVEDIX SDK is installed in /opt/cvedix (non-standard location)
if(EXISTS "/opt/cvedix/lib/cmake/cvedix/cvedix-config.cmake")
    message(STATUS "Found CVEDIX SDK in /opt/cvedix, adding to CMAKE_PREFIX_PATH")
    list(APPEND CMAKE_PREFIX_PATH "/opt/cvedix")
    include_directories("/opt/cvedix/include")
    message(STATUS "✓ Added /opt/cvedix/include to include directories")
endif()

find_package(cvedix QUIET)

if(NOT cvedix_FOUND)
    message(WARNING "CVEDIX SDK not found. Sample may not compile if cvedix_mqtt_json_receiver is part of SDK.")
    message(WARNING "  CVEDIX SDK should be installed system-wide or in /opt/cvedix")
else()
    message(STATUS "✓ Found CVEDIX SDK")
endif()

# ============================================
# Include directories
# ============================================
# Add parent project include directory (if building from samples directory)
if(EXISTS "${CMAKE_SOURCE_DIR}/../include")
    include_directories("${CMAKE_SOURCE_DIR}/../include")
endif()

# Add CVEDIX SDK include paths
if(EXISTS "/opt/cvedix/include")
    include_directories("/opt/cvedix/include")
endif()
if(EXISTS "/usr/include/cvedix")
    include_directories("/usr/include/cvedix")
endif()

# Add nlohmann/json include
include_directories(${NLOHMANN_JSON_INCLUDE_DIR})

# Add cpp-base64 include if found
if(CPP_BASE64_INCLUDE_DIR)
    include_directories(${CPP_BASE64_INCLUDE_DIR})
    # Also add parent directory to allow cpp_base64/base64.h include style
    get_filename_component(CPP_BASE64_PARENT_DIR ${CPP_BASE64_INCLUDE_DIR} DIRECTORY)
    if(CPP_BASE64_PARENT_DIR)
        include_directories(${CPP_BASE64_PARENT_DIR})
    endif()
endif()

# ============================================
# Build MQTT JSON Receiver Sample
# ============================================
add_executable(mqtt_json_receiver_sample
    mqtt_json_receiver_sample.cpp
    cvedix_mqtt_json_receiver_impl.cpp
    cvedix_mqtt_client_impl.cpp
)

# Set C++ standard
set_target_properties(mqtt_json_receiver_sample PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(mqtt_json_receiver_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${NLOHMANN_JSON_INCLUDE_DIR}
)

# Add CVEDIX SDK include paths if available
if(EXISTS "/opt/cvedix/include")
    target_include_directories(mqtt_json_receiver_sample PRIVATE
        /opt/cvedix/include
    )
endif()
if(EXISTS "/usr/include/cvedix")
    target_include_directories(mqtt_json_receiver_sample PRIVATE
        /usr/include/cvedix
    )
endif()

# Link libraries
target_link_libraries(mqtt_json_receiver_sample PRIVATE
    ${MOSQUITTO_LIBRARY}
)

# Add CVEDIX SDK library if available
if(TARGET cvedix::cvedix_instance_sdk)
    target_link_libraries(mqtt_json_receiver_sample PRIVATE
        cvedix::cvedix_instance_sdk
    )
    message(STATUS "✓ Linked CVEDIX SDK (via target)")
elseif(cvedix_FOUND)
    # Try to link using variables if target doesn't exist
    if(cvedix_LIBRARIES)
        target_link_libraries(mqtt_json_receiver_sample PRIVATE
            ${cvedix_LIBRARIES}
        )
        message(STATUS "✓ Linked CVEDIX SDK libraries (via variables)")
    endif()
endif()

# Always try to link CVEDIX SDK library directly (fallback)
if(EXISTS "/opt/cvedix/lib/libcvedix_instance_sdk.so")
    target_link_libraries(mqtt_json_receiver_sample PRIVATE
        /opt/cvedix/lib/libcvedix_instance_sdk.so
    )
    message(STATUS "✓ Linked CVEDIX SDK library directly: /opt/cvedix/lib/libcvedix_instance_sdk.so")
endif()

# Compile definitions
target_compile_definitions(mqtt_json_receiver_sample PRIVATE
    CVEDIX_WITH_MQTT
)

# ============================================
# Build Simple RTMP + MQTT Sample
# ============================================
add_executable(simple_rtmp_mqtt_sample
    simple_rtmp_mqtt_sample.cpp
)

# Set C++ standard
set_target_properties(simple_rtmp_mqtt_sample PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(simple_rtmp_mqtt_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${NLOHMANN_JSON_INCLUDE_DIR}
)

# Add cpp-base64 include if found
if(CPP_BASE64_INCLUDE_DIR)
    target_include_directories(simple_rtmp_mqtt_sample PRIVATE
        ${CPP_BASE64_INCLUDE_DIR}
    )
    # Also add parent directory to allow cpp_base64/base64.h include style
    get_filename_component(CPP_BASE64_PARENT_DIR ${CPP_BASE64_INCLUDE_DIR} DIRECTORY)
    if(CPP_BASE64_PARENT_DIR)
        target_include_directories(simple_rtmp_mqtt_sample PRIVATE
            ${CPP_BASE64_PARENT_DIR}
        )
    endif()
endif()

# Add CVEDIX SDK include paths if available
if(EXISTS "/opt/cvedix/include")
    target_include_directories(simple_rtmp_mqtt_sample PRIVATE
        /opt/cvedix/include
    )
endif()
if(EXISTS "/usr/include/cvedix")
    target_include_directories(simple_rtmp_mqtt_sample PRIVATE
        /usr/include/cvedix
    )
endif()

# Link libraries
target_link_libraries(simple_rtmp_mqtt_sample PRIVATE
    ${MOSQUITTO_LIBRARY}
)

# Add CVEDIX SDK library if available
if(TARGET cvedix::cvedix_instance_sdk)
    target_link_libraries(simple_rtmp_mqtt_sample PRIVATE
        cvedix::cvedix_instance_sdk
    )
elseif(cvedix_FOUND)
    # Try to link using variables if target doesn't exist
    if(cvedix_LIBRARIES)
        target_link_libraries(simple_rtmp_mqtt_sample PRIVATE
            ${cvedix_LIBRARIES}
        )
    endif()
endif()

# Always try to link CVEDIX SDK library directly (fallback)
if(EXISTS "/opt/cvedix/lib/libcvedix_instance_sdk.so")
    target_link_libraries(simple_rtmp_mqtt_sample PRIVATE
        /opt/cvedix/lib/libcvedix_instance_sdk.so
    )
endif()

# Compile definitions
target_compile_definitions(simple_rtmp_mqtt_sample PRIVATE
    CVEDIX_WITH_MQTT
)

# ============================================
# Build Face Tracking Sample with MQTT
# ============================================
add_executable(face_tracking_sample
    face_tracking_sample.cpp
)

# Set C++ standard
set_target_properties(face_tracking_sample PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(face_tracking_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${NLOHMANN_JSON_INCLUDE_DIR}
)

# Add CVEDIX SDK include paths if available
if(EXISTS "/opt/cvedix/include")
    target_include_directories(face_tracking_sample PRIVATE
        /opt/cvedix/include
    )
endif()
if(EXISTS "/usr/include/cvedix")
    target_include_directories(face_tracking_sample PRIVATE
        /usr/include/cvedix
    )
endif()

# Link libraries
target_link_libraries(face_tracking_sample PRIVATE
    ${MOSQUITTO_LIBRARY}
)

# Add CVEDIX SDK library if available
if(TARGET cvedix::cvedix_instance_sdk)
    target_link_libraries(face_tracking_sample PRIVATE
        cvedix::cvedix_instance_sdk
    )
elseif(cvedix_FOUND)
    # Try to link using variables if target doesn't exist
    if(cvedix_LIBRARIES)
        target_link_libraries(face_tracking_sample PRIVATE
            ${cvedix_LIBRARIES}
        )
    endif()
endif()

# Always try to link CVEDIX SDK library directly (fallback)
if(EXISTS "/opt/cvedix/lib/libcvedix_instance_sdk.so")
    target_link_libraries(face_tracking_sample PRIVATE
        /opt/cvedix/lib/libcvedix_instance_sdk.so
    )
endif()

# Compile definitions
target_compile_definitions(face_tracking_sample PRIVATE
    CVEDIX_WITH_MQTT
)

# Build Simple MQTT Test (no video processing)
# ============================================
add_executable(test_mqtt_simple
    test_mqtt_simple.cpp
)

# Set C++ standard
set_target_properties(test_mqtt_simple PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Link libraries (only mosquitto, no CVEDIX SDK needed)
target_link_libraries(test_mqtt_simple PRIVATE
    ${MOSQUITTO_LIBRARY}
)

# Try pkg-config for mosquitto if library not found
if(NOT MOSQUITTO_LIBRARY)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MOSQUITTO QUIET mosquitto)
        if(MOSQUITTO_FOUND)
            target_link_libraries(test_mqtt_simple PRIVATE ${MOSQUITTO_LIBRARIES})
            target_include_directories(test_mqtt_simple PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
        endif()
    endif()
endif()

message(STATUS "")
message(STATUS "==========================================")
message(STATUS "MQTT JSON Receiver Sample Configuration")
message(STATUS "==========================================")
message(STATUS "✓ nlohmann/json: ${NLOHMANN_JSON_INCLUDE_DIR}")
message(STATUS "✓ mosquitto: ${MOSQUITTO_LIBRARY}")
if(cvedix_FOUND)
    message(STATUS "✓ CVEDIX SDK: Found")
else()
    message(STATUS "⚠ CVEDIX SDK: Not found (may cause compilation errors)")
endif()
message(STATUS "==========================================")
message(STATUS "")
