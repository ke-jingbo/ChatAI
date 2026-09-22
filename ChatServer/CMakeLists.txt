cmake_minimum_required(VERSION 3.10)
project(AIChatServer LANGUAGES CXX)

# ChatServer 使用了 std::filesystem 等 C++17 特性。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 禁止编译器在不支持 C++17 时自动降级到较旧标准。
set(CMAKE_CXX_EXTENSIONS OFF)

# 模式
set(CMAKE_BUILD_TYPE "DEBUG")

# 查找 ChatServer 及 SDK 所需的第三方依赖。
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(gflags REQUIRED)
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)

# 查找已经安装到 /usr/local 的聊天 SDK
find_path(AI_CHAT_SDK_INCLUDE_DIR
    NAMES ChatSDK.h
    HINTS /usr/local/include/ai_chat_sdk
)
find_library(AI_CHAT_SDK_LIBRARY
    NAMES ai_chat_sdk
    HINTS /usr/local/lib
)
if(NOT AI_CHAT_SDK_INCLUDE_DIR OR NOT AI_CHAT_SDK_LIBRARY)
    message(FATAL_ERROR
        "ai_chat_sdk was not found. Install its headers and library under /usr/local first."
    )
endif()

# server.cpp 提供 main 函数，ChatServer.cpp 实现 HTTP 服务。
add_executable(AIChatServer server.cpp ChatServer.cpp)
# build 目录保存构建中间文件，最终可执行程序单独输出到 bin 目录。
set_target_properties(AIChatServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
# 再对目标显式声明 C++17，避免其标准依赖仅由全局变量隐式传递。
target_compile_features(AIChatServer PRIVATE cxx_std_17)
# cpp-httplib 访问 HTTPS Provider 时需要启用 OpenSSL 支持。
target_compile_definitions(AIChatServer PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
# 使用已安装 SDK 的公开头文件。
target_include_directories(AIChatServer PRIVATE ${AI_CHAT_SDK_INCLUDE_DIR})
# 链接 SDK、命令行解析、日志、JSON、数据库和网络加密依赖。
target_link_libraries(AIChatServer PRIVATE
    ${AI_CHAT_SDK_LIBRARY}
    gflags
    spdlog::spdlog
    PkgConfig::JSONCPP
    sqlite3
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
