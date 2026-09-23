# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置SDK名称
set(SDK_NAME "ai_chat_sdk")

# 设置C++版本
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型
set(CMAKE_BUILD_TYPE "Release")

# 收集所有源文件
file(GLOB_RECURSE SDK_SOURCES "src/*.cpp")
file(GLOB_RECURSE SDK_HEADERS "include/*.h")

# 生成静态库
add_library(${SDK_NAME} STATIC ${SDK_SOURCES} ${SDK_HEADERS})

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build)

# 定义httplib中的CPPHTTPLIB_OPENSSL_SUPPORT宏
target_compile_definitions(${SDK_NAME} PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

# 设置静态库的目录
target_link_directories(${SDK_NAME} PUBLIC /usr/local/lib)

# 添加头文件目录
target_include_directories(${SDK_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 找到OpenSSL库
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# 安装规则 拷贝静态库到系统目录下  /usr/local/lib
install(TARGETS ${SDK_NAME} ARCHIVE DESTINATION lib)

# 安装规则 拷贝头文件到系统目录下  /usr/local/include
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include/ai_chat_sdk
        FILES_MATCHING PATTERN "*.h"
)