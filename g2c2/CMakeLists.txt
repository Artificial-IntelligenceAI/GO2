add_executable(g2c2 main.cpp lex.cpp parse.cpp check.cpp gen.cpp)
target_include_directories(g2c2 SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(g2c2 PRIVATE ${LLVM_DEFINITIONS}
  G2_RUNTIME_LIB="$<TARGET_FILE:g2rt>"
  G2_CLANG="${CMAKE_CXX_COMPILER}"
  G2_LIBCXX_DIR="${G2_LIBCXX_DIR}")
target_compile_options(g2c2 PRIVATE -Wall -Wextra -Wno-unused-parameter -fno-rtti)
target_link_libraries(g2c2 PRIVATE LLVM)
add_dependencies(g2c2 g2rt)
