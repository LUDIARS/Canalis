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

# core: 受け渡しの契約 (RawRecord・Source)。
file(GLOB CANALIS_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/canalis/core/*.cpp)
add_library(canalis_core STATIC ${CANALIS_CORE_SOURCES})
add_library(canalis::core ALIAS canalis_core)
target_link_libraries(canalis_core PUBLIC canalis_base)
canalis_target_warnings(canalis_core)
lorica_harden_target(canalis_core)

# crawl: ①Crawl adapter (src/crawl の移植)。
file(GLOB_RECURSE CANALIS_CRAWL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/canalis/crawl/*.cpp)
add_library(canalis_crawl STATIC ${CANALIS_CRAWL_SOURCES})
add_library(canalis::crawl ALIAS canalis_crawl)
target_link_libraries(canalis_crawl PUBLIC canalis_core canalis_infra canalis_config)
canalis_target_warnings(canalis_crawl)
lorica_harden_target(canalis_crawl)

# save: ③Save の raw sink (JSONL)。ほかの sink は段階 5。
file(GLOB CANALIS_SAVE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/canalis/save/*.cpp)
add_library(canalis_save STATIC ${CANALIS_SAVE_SOURCES})
add_library(canalis::save ALIAS canalis_save)
target_link_libraries(canalis_save PUBLIC canalis_core)
canalis_target_warnings(canalis_save)
lorica_harden_target(canalis_save)

# cli: 実行ファイル canalis (src/cli.ts の移植)。コマンドはテストから呼べるようにライブラリにする。
add_library(canalis_cli STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/commands.cpp)
target_link_libraries(canalis_cli PUBLIC canalis_crawl canalis_save canalis_config)
canalis_target_warnings(canalis_cli)
lorica_harden_target(canalis_cli)

add_executable(canalis ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/main.cpp)
target_link_libraries(canalis PRIVATE canalis_cli)
if(WIN32)
  target_link_libraries(canalis PRIVATE shell32)
endif()
canalis_target_warnings(canalis)
lorica_harden_target(canalis)
set_target_properties(canalis PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin" RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
install(TARGETS canalis RUNTIME DESTINATION bin)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
