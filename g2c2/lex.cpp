// The lexer: a file's text into tokens.

#include "g2c2.h"

#include <cstdio>
#include <cstdlib>

std::vector<SourceFile> g_files;

void error_at(Pos pos, const std::string& message) {
    std::fflush(stdout);
    if (pos.line > 0) {
        std::fprintf(stderr, "%s:%d:%d: %s\n", g_files[pos.file].path.c_str(), pos.line, pos.col, message.c_str());
    } else {
        std::fprintf(stderr, "%s: %s\n", g_files[pos.file].path.c_str(), message.c_str());
    }
    std::exit(1);
}

struct Keyword {
    const char* text;
    Tok kind;
};

static const Keyword keywords[] = {
    {"pkg", Tok::KwPkg}, {"import", Tok::KwImport}, {"pub", Tok::KwPub}, {"fn", Tok::KwFn},
    {"ret", Tok::KwRet}, {"let", Tok::KwLet}, {"var", Tok::KwVar}, {"const", Tok::KwConst},
    {"struct", Tok::KwStruct}, {"enum", Tok::KwEnum}, {"match", Tok::KwMatch}, {"case", Tok::KwCase},
    {"else", Tok::KwElse}, {"if", Tok::KwIf}, {"for", Tok::KwFor}, {"in", Tok::KwIn},
    {"stop", Tok::KwStop}, {"next", Tok::KwNext}, {"fail", Tok::KwFail}, {"coll", Tok::KwColl},
    {"member", Tok::KwMember}, {"extern", Tok::KwExtern}, {"self", Tok::KwSelf}, {"true", Tok::KwTrue},
    {"false", Tok::KwFalse}, {"and", Tok::KwAnd}, {"or", Tok::KwOr},
};

static const char* type_names[] = {
    "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "str", "ls", "map",
};

std::string tok_name(Tok kind) {
    switch (kind) {
    case Tok::End: return "the end of the file";
    case Tok::Name: return "a name";
    case Tok::Int: return "a number";
    case Tok::Float: return "a number";
    case Tok::String: return "a string";
    case Tok::KwType: return "a type name";
    case Tok::LParen: return "`(`";
    case Tok::RParen: return "`)`";
    case Tok::LBracket: return "`[`";
    case Tok::RBracket: return "`]`";
    case Tok::LBrace: return "`{`";
    case Tok::RBrace: return "`}`";
    case Tok::Semi: return "`;`";
    case Tok::Colon: return "`:`";
    case Tok::Comma: return "`,`";
    case Tok::Dot: return "`.`";
    case Tok::DotDot: return "`..`";
    case Tok::Arrow: return "`->`";
    case Tok::Assign: return "`=`";
    case Tok::QQ: return "`??`";
    case Tok::Plus: return "`+`";
    case Tok::Minus: return "`-`";
    case Tok::Star: return "`*`";
    case Tok::Slash: return "`/`";
    case Tok::Percent: return "`%`";
    case Tok::Eq: return "`==`";
    case Tok::Ne: return "`!=`";
    case Tok::Lt: return "`<`";
    case Tok::Le: return "`<=`";
    case Tok::Gt: return "`>`";
    case Tok::Ge: return "`>=`";
    case Tok::AndAnd: return "`&&`";
    case Tok::OrOr: return "`||`";
    case Tok::Bang: return "`!`";
    default: break;
    }
    for (const Keyword& k : keywords) {
        if (k.kind == kind) return std::string("`") + k.text + "`";
    }
    return "a token";
}

static bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_hex(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

// Checks that s is valid UTF-8: no overlong forms, no surrogates, nothing past U+10FFFF.
static bool valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        int n = 0;
        uint32_t cp = 0;
        if (c < 0x80) { i++; continue; }
        if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
        else return false;
        for (int k = 1; k <= n; k++) {
            if (i + k >= s.size()) return false;
            unsigned char d = s[i + k];
            if ((d & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (d & 0x3F);
        }
        if (n == 1 && cp < 0x80) return false;
        if (n == 2 && cp < 0x800) return false;
        if (n == 3 && cp < 0x10000) return false;
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += n + 1;
    }
    return true;
}

std::vector<Token> lex(int file) {
    const std::string& src = g_files[file].text;
    std::vector<Token> out;
    size_t i = 0;
    int line = 1;
    size_t line_start = 0;

    while (true) {
        // whitespace and comments
        while (i < src.size()) {
            char c = src[i];
            if (c == '\n') {
                i++;
                line++;
                line_start = i;
            } else if (c == ' ' || c == '\t' || c == '\r') {
                i++;
            } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                while (i < src.size() && src[i] != '\n') i++;
            } else {
                break;
            }
        }
        Token t;
        t.pos = Pos{file, line, (int)(i - line_start) + 1};
        if (i >= src.size()) {
            t.kind = Tok::End;
            out.push_back(t);
            return out;
        }
        char c = src[i];
        size_t start = i;

        if (is_letter(c)) {
            while (i < src.size() && (is_letter(src[i]) || is_digit(src[i]))) i++;
            t.text = src.substr(start, i - start);
            t.kind = Tok::Name;
            for (const Keyword& k : keywords) {
                if (t.text == k.text) t.kind = k.kind;
            }
            for (const char* name : type_names) {
                if (t.text == name) t.kind = Tok::KwType;
            }
            out.push_back(t);
            continue;
        }

        if (is_digit(c)) {
            bool hex = c == '0' && i + 1 < src.size() && src[i + 1] == 'x';
            if (hex) i += 2;
            // digits, with a single _ allowed between two digits
            auto digits = [&](bool want_hex) {
                size_t first = i;
                while (i < src.size()) {
                    char d = src[i];
                    if (want_hex ? is_hex(d) : is_digit(d)) {
                        i++;
                    } else if (d == '_' && i > first && i + 1 < src.size() &&
                               (want_hex ? is_hex(src[i + 1]) : is_digit(src[i + 1])) &&
                               src[i - 1] != '_') {
                        i++;
                    } else {
                        break;
                    }
                }
                return i > first;
            };
            if (!digits(hex)) {
                error_at(t.pos, "a hex number needs digits after `0x`");
            }
            bool is_float = false;
            if (!hex && i < src.size() && src[i] == '.' && (i + 1 >= src.size() || (!is_digit(src[i + 1]) && src[i + 1] != '.'))) {
                error_at(t.pos, "a float has digits on both sides of its point, as in `" + src.substr(start, i - start) + ".0`");
            }
            if (!hex && i + 1 < src.size() && src[i] == '.' && is_digit(src[i + 1])) {
                i++;
                digits(false);
                is_float = true;
            }
            if (!hex && i < src.size() && src[i] == 'e') {
                size_t save = i;
                i++;
                if (i < src.size() && (src[i] == '-' || src[i] == '+')) i++;
                if (!digits(false)) {
                    i = save;
                } else {
                    is_float = true;
                }
            }
            if (i < src.size() && (is_letter(src[i]) || is_digit(src[i]))) {
                Pos p = Pos{file, line, (int)(i - line_start) + 1};
                error_at(p, std::string("unexpected `") + src[i] + "` in a number");
            }
            t.text = src.substr(start, i - start);
            if (is_float) {
                t.kind = Tok::Float;
            } else {
                t.kind = Tok::Int;
                uint64_t v = 0;
                size_t k = hex ? start + 2 : start;
                for (; k < i; k++) {
                    char d = src[k];
                    if (d == '_') continue;
                    uint64_t digit = is_digit(d) ? (uint64_t)(d - '0')
                                   : (d >= 'a' ? (uint64_t)(d - 'a' + 10) : (uint64_t)(d - 'A' + 10));
                    uint64_t base = hex ? 16 : 10;
                    if (v > (UINT64_MAX - digit) / base) {
                        error_at(t.pos, "the number " + t.text + " is too big for any integer type");
                    }
                    v = v * base + digit;
                }
                t.value = v;
            }
            out.push_back(t);
            continue;
        }

        if (c == '"') {
            i++;
            std::string bytes;
            while (true) {
                if (i >= src.size()) error_at(t.pos, "this string has no closing `\"`");
                char d = src[i];
                if (d == '"') { i++; break; }
                if (d == '\n') {
                    error_at(t.pos, "a line break inside a string literal is not decided yet");
                }
                if (d == '\\') {
                    Pos ep = Pos{file, line, (int)(i - line_start) + 1};
                    if (i + 1 >= src.size()) error_at(t.pos, "this string has no closing `\"`");
                    char e = src[i + 1];
                    if (e == 'n') bytes += '\n';
                    else if (e == 't') bytes += '\t';
                    else if (e == '"') bytes += '"';
                    else if (e == '\\') bytes += '\\';
                    else error_at(ep, std::string("the escape `\\") + e + "` is not decided yet; strings have `\\n`, `\\t`, `\\\"` and `\\\\`");
                    i += 2;
                    continue;
                }
                bytes += d;
                i++;
            }
            if (!valid_utf8(bytes)) error_at(t.pos, "this string is not valid UTF-8");
            t.kind = Tok::String;
            t.text = bytes;
            out.push_back(t);
            continue;
        }

        auto two = [&](char a, char b) { return c == a && i + 1 < src.size() && src[i + 1] == b; };
        Tok kind = Tok::End;
        int len = 1;
        if (two('.', '.')) { kind = Tok::DotDot; len = 2; }
        else if (two('-', '>')) { kind = Tok::Arrow; len = 2; }
        else if (two('=', '=')) { kind = Tok::Eq; len = 2; }
        else if (two('!', '=')) { kind = Tok::Ne; len = 2; }
        else if (two('<', '=')) { kind = Tok::Le; len = 2; }
        else if (two('>', '=')) { kind = Tok::Ge; len = 2; }
        else if (two('&', '&')) { kind = Tok::AndAnd; len = 2; }
        else if (two('|', '|')) { kind = Tok::OrOr; len = 2; }
        else if (two('?', '?')) { kind = Tok::QQ; len = 2; }
        else if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '%') && i + 1 < src.size() && src[i + 1] == '=') {
            error_at(t.pos, std::string("GO2 has no `") + c + "=`; a change is written in full, as in `n = n " + c + " 1;`");
        } else {
            switch (c) {
            case '(': kind = Tok::LParen; break;
            case ')': kind = Tok::RParen; break;
            case '[': kind = Tok::LBracket; break;
            case ']': kind = Tok::RBracket; break;
            case '{': kind = Tok::LBrace; break;
            case '}': kind = Tok::RBrace; break;
            case ';': kind = Tok::Semi; break;
            case ':': kind = Tok::Colon; break;
            case ',': kind = Tok::Comma; break;
            case '.': kind = Tok::Dot; break;
            case '=': kind = Tok::Assign; break;
            case '+': kind = Tok::Plus; break;
            case '-': kind = Tok::Minus; break;
            case '*': kind = Tok::Star; break;
            case '/': kind = Tok::Slash; break;
            case '%': kind = Tok::Percent; break;
            case '<': kind = Tok::Lt; break;
            case '>': kind = Tok::Gt; break;
            case '!': kind = Tok::Bang; break;
            case '&': case '|': case '^':
                error_at(t.pos, std::string("GO2 has no bit operators; `") + c + "` is not an operator");
            case '?':
                error_at(t.pos, "GO2 has no `?`; a failure is handled with `??`");
            default:
                error_at(t.pos, std::string("unexpected character `") + c + "`");
            }
        }
        t.kind = kind;
        t.text = src.substr(i, len);
        i += len;
        out.push_back(t);
    }
}
