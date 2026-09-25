cmake_minimum_required(VERSION 3.10)
project(QQSmtpTest LANGUAGES CXX)

# 示例使用 C++17，与 ChatAI 项目的语言标准保持一致。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置构建类型
set(CMAKE_BUILD_TYPE "Debug")
# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build)

# 通过 pkg-config 查找 libcurl，并同时获得它需要的 TLS 等链接参数。
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl)
# 验证码使用 OpenSSL 的安全随机数、HMAC 和恒定时间比较能力。
find_package(OpenSSL REQUIRED)

# 构建独立的 QQ SMTP 测试程序，不会链接或修改现有 ChatAI 后端。
add_executable(qq_smtp_test QQSmtpTest.cpp VerificationCode.cpp)
target_link_libraries(qq_smtp_test PRIVATE PkgConfig::LIBCURL OpenSSL::Crypto)

# 构建独立的验证码生成与验证测试程序。
add_executable(verification_code_test
    VerificationCode.cpp
    VerificationCodeTest.cpp
)
target_link_libraries(verification_code_test PRIVATE OpenSSL::Crypto)
