# GO2: G2C2 (the bootstrap compiler) and the runtime every compiled program links.
cmake_minimum_required(VERSION 3.28)

# Homebrew clang 23 and its libc++, against Homebrew LLVM 23.
set(G2_LLVM_ROOT "/opt/homebrew/opt/llvm" CACHE PATH "Homebrew LLVM 23")
if(NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "${G2_LLVM_ROOT}/bin/clang++")
endif()
# LLVM's CMake package probes headers with C, so C is enabled too.
if(NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "${G2_LLVM_ROOT}/bin/clang")
endif()
if(NOT LLVM_DIR)
  set(LLVM_DIR "${G2_LLVM_ROOT}/lib/cmake/llvm")
endif()

project(GO2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(G2_LIBCXX_DIR "${G2_LLVM_ROOT}/lib/c++")
add_link_options("-L${G2_LIBCXX_DIR}" "-Wl,-rpath,${G2_LIBCXX_DIR}")

find_package(LLVM REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL 23)
  message(FATAL_ERROR "G2C2 builds against LLVM 23; found ${LLVM_PACKAGE_VERSION}")
endif()

add_subdirectory(runtime)
add_subdirectory(g2c2)

# One command runs every example and every reject case: cmake --build build --target check
add_custom_target(check
  COMMAND sh "${CMAKE_SOURCE_DIR}/g2c2/tests/run.sh" "$<TARGET_FILE:g2c2>"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  DEPENDS g2c2 g2rt
  USES_TERMINAL)
