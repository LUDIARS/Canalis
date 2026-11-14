# tests/**/<name>_tests.cpp を 1 本ずつ実行ファイルにして ctest に登録する。
# テスト名は <ディレクトリ>.<name> (例 base.json)。
file(GLOB_RECURSE CANALIS_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_tests.cpp)
foreach(source ${CANALIS_TEST_SOURCES})
  file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${source})
  get_filename_component(dir ${rel} DIRECTORY)
  get_filename_component(stem ${rel} NAME_WE)
  string(REGEX REPLACE "_tests$" "" stem ${stem})
  string(REPLACE "/" "." group "${dir}")
  if(group STREQUAL "")
    set(test_name ${stem})
  else()
    set(test_name ${group}.${stem})
  endif()
  string(REPLACE "." "_" target_name "canalis_test_${test_name}")
  add_executable(${target_name} ${source})
  add_dependencies(${target_name} canalis)  # CLI のテストが実行ファイルを起動する
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  # テストは Beast で偽のサーバを立てることがあるので canalis_boost も使える。
  target_link_libraries(${target_name} PRIVATE canalis_cli canalis_runner canalis_llm canalis_boost)
  canalis_target_warnings(${target_name})
  lorica_harden_target(${target_name})
  add_test(NAME ${test_name} COMMAND ${target_name})
  set_tests_properties(${test_name} PROPERTIES
    ENVIRONMENT "CI_TEST_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures;CI_REPO_ROOT=${PROJECT_SOURCE_DIR}/..;CI_CANALIS_EXE=$<TARGET_FILE:canalis>"
    TIMEOUT 120)
endforeach()
