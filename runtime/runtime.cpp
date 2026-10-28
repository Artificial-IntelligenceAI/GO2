// The GO2 runtime, linked into every compiled program. Compiled code calls these C-named functions.
// Output goes through C's stdio, so it interleaves correctly with C functions a program calls.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {

void g2_print_str(const char* bytes, int64_t len) {
    std::fwrite(bytes, 1, (size_t)len, stdout);
}

void g2_print_i64(int64_t v) {
    std::fprintf(stdout, "%lld", (long long)v);
}

void g2_print_u64(uint64_t v) {
    std::fprintf(stdout, "%llu", (unsigned long long)v);
}

// Why a program stops; kept in step with StopKind in g2c2/gen.cpp.
[[noreturn]] void g2_stop(int32_t kind, const char* file, int64_t line) {
    const char* what = "stopped";
    switch (kind) {
    case 1: what = "overflow: the result does not fit its type"; break;
    case 2: what = "division by zero"; break;
    case 3: what = "index out of range"; break;
    case 4: what = "conversion out of range: the value does not fit its new type"; break;
    case 5: what = "a float that does not fit an integer, in a release build: what happens is not decided yet"; break;
    }
    std::fflush(stdout);
    std::fprintf(stderr, "%s:%lld: %s\n", file, (long long)line, what);
    std::exit(2);
}

[[noreturn]] void g2_exit(int32_t code) {
    std::exit(code);
}

bool g2_str_eq(const char* a, int64_t alen, const char* b, int64_t blen) {
    return alen == blen && std::memcmp(a, b, (size_t)alen) == 0;
}

// Byte by byte: below 0 when a sorts first, 0 when equal, above 0 when b sorts first.
int32_t g2_str_cmp(const char* a, int64_t alen, const char* b, int64_t blen) {
    int64_t n = alen < blen ? alen : blen;
    int c = std::memcmp(a, b, (size_t)n);
    if (c != 0) return c < 0 ? -1 : 1;
    if (alen == blen) return 0;
    return alen < blen ? -1 : 1;
}

// Text to an integer of the given width and sign: an optional - and digits.
// Returns 0 and sets *out, or 1 for not_a_number, 2 for out_of_range.
int32_t g2_text_to_int(const char* s, int64_t len, int32_t bits, bool is_signed, int64_t* out) {
    int64_t i = 0;
    bool negative = false;
    if (i < len && s[i] == '-') {
        negative = true;
        i++;
    }
    if (i == len) return 1;
    uint64_t limit;  // the largest magnitude allowed
    if (is_signed) limit = negative ? (uint64_t)1 << (bits - 1) : ((uint64_t)1 << (bits - 1)) - 1;
    else limit = bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
    uint64_t v = 0;
    bool too_big = false;
    for (; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return 1;
        uint64_t d = (uint64_t)(s[i] - '0');
        if (v > (UINT64_MAX - d) / 10) too_big = true;
        else v = v * 10 + d;
    }
    if (!is_signed && negative && v != 0) too_big = true;
    if (too_big || v > limit) return 2;
    *out = negative ? (int64_t)(0 - v) : (int64_t)v;
    return 0;
}

// The character starting at s[*at], a code point; moves *at past it. A str is always valid UTF-8.
uint32_t g2_utf8_next(const char* s, int64_t* at) {
    const unsigned char* p = (const unsigned char*)s + *at;
    uint32_t c = p[0];
    int n = 0;
    if (c >= 0xF0) { c &= 0x07; n = 3; }
    else if (c >= 0xE0) { c &= 0x0F; n = 2; }
    else if (c >= 0xC0) { c &= 0x1F; n = 1; }
    for (int k = 1; k <= n; k++) c = (c << 6) | (p[k] & 0x3F);
    *at += n + 1;
    return c;
}

}
