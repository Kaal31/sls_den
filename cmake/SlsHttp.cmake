# This adapter uses Playden's native HTTP implementation; no separate curl stack.
add_library(sls_source_http STATIC "${CMAKE_SOURCE_DIR}/src/sls/native_source_fetch.cpp")
target_include_directories(sls_source_http PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(sls_source_http PUBLIC cxx_std_17)
target_link_libraries(sls_source_http PUBLIC sls_manifest playden_steam_http)
if(PLAYDEN_PLATFORM_PS5)
  target_compile_options(sls_source_http PRIVATE -fno-exceptions -fno-rtti)
endif()
