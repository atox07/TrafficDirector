cmake_minimum_required(VERSION 3.20)
project(TrafficDirector LANGUAGES CXX)
if(NOT MSVC)
  message(FATAL_ERROR "MSVC is required for the Script Hook V C++ ABI")
endif()
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Windows x64 is required")
endif()
add_library(TrafficDirector SHARED src/main.cpp)
set(SHV_IMPORT_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/shv/ScriptHookV.lib")
add_custom_command(OUTPUT "${SHV_IMPORT_LIBRARY}"
 COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shv"
 COMMAND "${CMAKE_AR}" /nologo /machine:x64
         "/def:${CMAKE_CURRENT_SOURCE_DIR}/src/shv_imports.def"
         "/out:${SHV_IMPORT_LIBRARY}"
 DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/shv_imports.def"
 VERBATIM)
add_custom_target(shv_imports DEPENDS "${SHV_IMPORT_LIBRARY}")
add_dependencies(TrafficDirector shv_imports)
target_link_libraries(TrafficDirector PRIVATE "${SHV_IMPORT_LIBRARY}")
target_compile_features(TrafficDirector PRIVATE cxx_std_17)
target_compile_definitions(TrafficDirector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(TrafficDirector PROPERTIES PREFIX "" SUFFIX ".asi")
if(MSVC)
 target_compile_options(TrafficDirector PRIVATE /W4 /EHsc)
 set_property(TARGET TrafficDirector PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
