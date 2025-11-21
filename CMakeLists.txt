cmake_minimum_required(VERSION 3.10)
project(example_using_sdk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# ============================================
# Cách 1: Sử dụng find_package (Khuyến nghị)
# ============================================
# Nếu SDK được cài đặt hệ thống hoặc trong CMAKE_PREFIX_PATH
find_package(cvedix REQUIRED)

# Hoặc chỉ định đường dẫn trực tiếp
# set(CMAKE_PREFIX_PATH "/path/to/cvedix/sdk/install" ${CMAKE_PREFIX_PATH})
# find_package(cvedix REQUIRED)

# ============================================
# Cách 2: Sử dụng trực tiếp (Manual)
# ============================================
# Uncomment các dòng sau nếu không dùng find_package
# set(CVEDIX_SDK_ROOT "/path/to/cvedix/sdk/install")
# include_directories(${CVEDIX_SDK_ROOT}/include)
# link_directories(${CVEDIX_SDK_ROOT}/lib)

# Tạo executable
add_executable(example_using_sdk main.cpp)

# Liên kết với SDK
# Cách 1: Sử dụng CMake target (khuyến nghị)
target_link_libraries(example_using_sdk PRIVATE cvedix::cvedix_instance_sdk)

# Cách 2: Sử dụng trực tiếp (nếu không dùng find_package)
# target_link_libraries(example_using_sdk PRIVATE cvedix_instance_sdk)

# Thêm các dependencies khác nếu cần
# find_package(OpenCV REQUIRED)
# target_link_libraries(example_using_sdk PRIVATE ${OpenCV_LIBS})

