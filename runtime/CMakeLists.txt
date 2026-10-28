add_library(g2rt STATIC runtime.cpp)
target_compile_options(g2rt PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
install(TARGETS g2rt DESTINATION lib)
