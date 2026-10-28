# GO2: G2C2 (the bootstrap compiler) and the runtime every compiled program links.
cmake_minimum_required(VERSION 3.28)

# On a Mac: Homebrew clang 23 and its libc++, against Homebrew LLVM 23. Elsewhere, pass the compilers and LLVM_DIR.
if(CMAKE_HOST_APPLE)
  set(G2_LLVM_ROOT "/opt/homebrew/opt/llvm" CACHE PATH "Homebrew LLVM 23")
  if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${G2_LLVM_ROOT}/bin/clang++")
  endif()
  # LLVM's CMake package probes headers with C, so C is enabled too; programs are linked with it.
  if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "${G2_LLVM_ROOT}/bin/clang")
  endif()
  if(NOT LLVM_DIR)
    set(LLVM_DIR "${G2_LLVM_ROOT}/lib/cmake/llvm")
  endif()
endif()

# A Linux release carries our LLVM 23 inside g2c2: LLVM, LLD (which links each program) and the C++ library, so it
# needs no LLVM on the machine.
option(G2C2_BUNDLE "Link LLVM, LLD and the C++ library into g2c2 (x86_64 Linux)" OFF)

project(GO2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_HOST_APPLE)
  set(G2_LIBCXX_DIR "${G2_LLVM_ROOT}/lib/c++")
  add_link_options("-L${G2_LIBCXX_DIR}" "-Wl,-rpath,${G2_LIBCXX_DIR}")
endif()

# bin/g2c2 and lib/libg2rt.a, as in a release, since g2c2 finds the runtime beside it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(LLVM REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL 23)
  message(FATAL_ERROR "G2C2 builds against LLVM 23; found ${LLVM_PACKAGE_VERSION}")
endif()
if(G2C2_BUNDLE)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "G2C2_BUNDLE is for x86_64 Linux")
  endif()
  # LLD's own CMake package links the shared LLVM, so its static libraries are named directly.
  set(G2_LLD_LIBS "${LLVM_LIBRARY_DIR}/liblldELF.a" "${LLVM_LIBRARY_DIR}/liblldCommon.a")
  foreach(lib ${G2_LLD_LIBS})
    if(NOT EXISTS "${lib}")
      message(FATAL_ERROR "G2C2_BUNDLE needs LLD 23's static libraries; ${lib} is missing")
    endif()
  endforeach()
endif()

add_subdirectory(runtime)
add_subdirectory(g2c2)

# One command runs every example and every reject case: cmake --build build --target check
add_custom_target(check
  COMMAND sh "${CMAKE_SOURCE_DIR}/g2c2/tests/run.sh" "$<TARGET_FILE:g2c2>"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  DEPENDS g2c2 g2rt
  USES_TERMINAL)
