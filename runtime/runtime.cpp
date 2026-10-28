// The GO2 runtime, linked into every compiled program. Compiled code calls these C-named functions.
// Output goes through C's stdio, so it interleaves correctly with C functions a program calls.

#include <cstdint>
#include <cstdio>

extern "C" {

void g2_print_str(const char* bytes, int64_t len) {
    std::fwrite(bytes, 1, (size_t)len, stdout);
}

}
