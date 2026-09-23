cmake_minimum_required(VERSION 3.22)

# 项目名称
project(testLLM)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型
set(CMAKE_BUILD_TYPE DEBUG)

# 添加可执行文件
add_executable(testLLM TestLLM.cpp 
../SDK/src/DeepseekProvider.cpp
../SDK/src/MimoProvider.cpp
../SDK/src/util/mylog.cpp
../SDK/src/KimiProvider.cpp
../SDK/src/OllamaLLMProvider.cpp
../SDK/src/LLMManager.cpp
../SDK/src/SessionManager.cpp
../SDK/src/DataManager.cpp
../SDK/src/ChatSDK.cpp)

# 设置输出根目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}../SDK/include)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# 添加CPPHTTPLIB_OPENSSL_SUPPORT定义
target_compile_definitions(testLLM PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

# 链接库
target_link_libraries(testLLM jsoncpp fmt spdlog gtest sqlite3 OpenSSL::SSL OpenSSL::Crypto)