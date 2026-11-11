cmake_minimum_required(VERSION 3.24)
project(CanalisNative VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include(FetchContent)
include(CTest)

option(CANALIS_WARNINGS_AS_ERRORS "Treat compiler warnings in Canalis sources as errors" ON)

include(cmake/CanalisDependencies.cmake)
include(cmake/CanalisWarnings.cmake)

# base: 文字列・JSON・暗号・ログ・環境・ファイル・URL・子プロセス (Boost 非依存)。
file(GLOB CANALIS_BASE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/canalis/base/*.cpp)
add_library(canalis_base STATIC ${CANALIS_BASE_SOURCES})
add_library(canalis::base ALIAS canalis_base)
target_include_directories(canalis_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(canalis_base PUBLIC Lorica::boundary nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto
  Threads::Threads)
canalis_target_warnings(canalis_base)
lorica_harden_target(canalis_base)

# infra: 外部への HTTP/HTTPS クライアント。Boost.Beast を含む翻訳単位はここだけに閉じ込める (重いので)。
file(GLOB CANALIS_INFRA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/canalis/infra/*.cpp)
add_library(canalis_infra STATIC ${CANALIS_INFRA_SOURCES} ${CMAKE_BINARY_DIR}/generated/beast_src.cpp)
add_library(canalis::infra ALIAS canalis_infra)
target_link_libraries(canalis_infra PUBLIC canalis_base PRIVATE canalis_boost)
if(WIN32)
  target_link_libraries(canalis_infra PRIVATE crypt32)
endif()
canalis_target_warnings(canalis_infra)
lorica_harden_target(canalis_infra)

# config: 暗号化 config (src/config の移植)。
file(GLOB CANALIS_CONFIG_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/canalis/config/*.cpp)
add_library(canalis_config STATIC ${CANALIS_CONFIG_SOURCES})
add_library(canalis::config ALIAS canalis_config)
target_link_libraries(canalis_config PUBLIC canalis_base)
canalis_target_warnings(canalis_config)
lorica_harden_target(canalis_config)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
