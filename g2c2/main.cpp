// g2c2 build [--release] <file or project>
// g2c2 run [--release] <file or project>
//
// A .go2 file is built alone, as package main. A folder is a project: G2C2 walks up from it to the folder holding
// project.go2, whose root files are package main, and whose other packages are folders named by import.

#include "g2c2.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#ifdef G2_LLD
#include <lld/Common/Driver.h>
LLD_HAS_DRIVER(elf)
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

static const char* g_argv0;

static void usage() {
    std::fprintf(stderr, "usage: g2c2 build [--release] <file or project>\n"
                         "       g2c2 run [--release] <file or project>\n");
    std::exit(2);
}

[[noreturn]] static void fatal(const std::string& message) {
    std::fprintf(stderr, "g2c2: %s\n", message.c_str());
    std::exit(1);
}

static int add_file(const std::string& path, const std::string& shown) {
    std::ifstream in(path, std::ios::binary);
    if (!in) fatal("cannot read " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    SourceFile sf;
    sf.path = shown;
    sf.text = ss.str();
    g_files.push_back(sf);
    return (int)g_files.size() - 1;
}

// Every .go2 file in a folder, but project.go2, sorted so builds repeat.
static std::vector<fs::path> go2_files(const fs::path& dir) {
    std::vector<fs::path> files;
    for (const fs::directory_entry& e : fs::directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".go2" && e.path().filename() != "project.go2") {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

static Package* load_package(Program* prog, const fs::path& root, const std::string& rel, const std::string& name) {
    for (Package* p : prog->packages) {
        if (p->dir == rel) return p;
    }
    Package* pkg = new Package();
    pkg->name = name;
    pkg->dir = rel;
    prog->packages.push_back(pkg);
    fs::path dir = rel.empty() ? root : root / rel;
    for (const fs::path& file : go2_files(dir)) {
        std::string shown = rel.empty() ? file.filename().string() : rel + "/" + file.filename().string();
        int id = add_file(file.string(), shown);
        FileAst* f = parse_file(id);
        if (f->pkg_name != name) {
            error_at(f->pkg_pos, "this file is in folder `" + (rel.empty() ? std::string("(the project root)") : rel) +
                                     "`, so it starts `pkg " + name + ";`");
        }
        pkg->files.push_back(f);
    }
    if (pkg->files.empty()) fatal("package `" + rel + "` has no .go2 files");
    for (FileAst* f : pkg->files) {
        for (Import& imp : f->imports) {
            if (imp.path == "fmt" || imp.path == "os") continue;
            if (!fs::is_directory(root / imp.path)) {
                error_at(imp.pos, "there is no package `" + imp.path + "`: no such folder in the project");
            }
            imp.pkg = load_package(prog, root, imp.path, imp.name);
        }
    }
    return pkg;
}

// project.go2 holds Project[name = "…"];
static std::string read_project_name(const fs::path& root) {
    int id = add_file((root / "project.go2").string(), "project.go2");
    Expr* e = parse_project_file(id);
    if (e->kind != ExprKind::Bracket || e->kids[0]->kind != ExprKind::Name || e->kids[0]->text != "Project") {
        error_at(e->pos, "project.go2 holds `Project[name = \"…\"];`");
    }
    std::string name;
    for (size_t i = 1; i < e->kids.size(); i++) {
        const std::string& field = e->names[i - 1];
        if (field != "name") {
            error_at(e->name_pos[i - 1], field.empty() ? "a `Project` field is given by name: `name = \"…\"`"
                                                       : "`Project` has no field `" + field + "`");
        }
        if (e->kids[i]->kind != ExprKind::Str) error_at(e->kids[i]->pos, "a project's `name` is a string literal");
        if (!name.empty()) error_at(e->name_pos[i - 1], "`name` is given twice");
        name = e->kids[i]->text;
    }
    if (name.empty()) error_at(e->pos, "`Project` is missing its field `name`");
    return name;
}

static Program* load(const std::string& arg) {
    Program* prog = new Program();
    fs::path path(arg);
    if (fs::is_regular_file(path)) {
        if (path.extension() != ".go2") fatal(arg + " is not a .go2 file");
        Package* pkg = new Package();
        pkg->name = "main";
        prog->packages.push_back(pkg);
        int id = add_file(arg, path.filename().string());
        FileAst* f = parse_file(id);
        if (f->pkg_name != "main") error_at(f->pkg_pos, "a file built alone is `pkg main;`");
        for (Import& imp : f->imports) {
            if (imp.path != "fmt" && imp.path != "os") {
                error_at(imp.pos, "importing `" + imp.path + "` needs a project: a folder holding project.go2");
            }
        }
        pkg->files.push_back(f);
        prog->name = path.stem().string();
        return prog;
    }
    if (!fs::is_directory(path)) fatal("no such file or folder: " + arg);
    fs::path root = fs::absolute(path).lexically_normal();
    while (!fs::exists(root / "project.go2")) {
        if (root == root.parent_path()) fatal("no project.go2 in " + arg + " or any folder above it");
        root = root.parent_path();
    }
    prog->name = read_project_name(root);
    load_package(prog, root, "", "main");
    return prog;
}

static std::string quote(const std::string& s) {
    std::string q = "'";
    for (char c : s) {
        if (c == '\'') q += "'\\''";
        else q += c;
    }
    return q + "'";
}

#ifdef G2_LLD
// A release links with the LLD inside G2C2, never a linker on the machine, against the machine's C library, as clang
// does on x86_64 Linux. The runtime needs only the C library.
static void link(const std::string& obj, const std::string& runtime, const std::string& exe) {
    std::string libdir;
    for (std::string dir : {"/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib"}) {
        if (fs::exists(dir + "/Scrt1.o")) {
            libdir = dir;
            break;
        }
    }
    if (libdir.empty()) fatal("cannot find the C library's Scrt1.o; it comes with the C library's development files");
    std::vector<std::string> args = {"ld.lld", "-pie", "--eh-frame-hdr", "-m", "elf_x86_64",
                                     "-dynamic-linker", "/lib64/ld-linux-x86-64.so.2", "-o", exe,
                                     libdir + "/Scrt1.o", libdir + "/crti.o", obj, runtime,
                                     "-L" + libdir, "-lc", libdir + "/crtn.o"};
    std::vector<const char*> argv;
    for (const std::string& a : args) argv.push_back(a.c_str());
    std::string err;
    llvm::raw_string_ostream err_os(err);
    lld::Result r = lld::lldMain(argv, llvm::outs(), err_os, {{lld::Gnu, &lld::elf::link}});
    if (r.retCode != 0) fatal("linking failed:\n" + err);
}
#else
// The runtime needs only the C library, so the program links as C does.
static void link(const std::string& obj, const std::string& runtime, const std::string& exe) {
    std::string cmd = quote(G2_CLANG) + " " + quote(obj) + " " + quote(runtime) + " -o " + quote(exe);
    if (std::system(cmd.c_str()) != 0) fatal("linking failed: " + cmd);
}
#endif

// Builds the program into a temporary folder; returns the path of the executable.
static std::string build(const std::string& arg, bool release, std::string& tmp_dir) {
    Program* prog = load(arg);
    check_program(prog);

    std::string tmpl = (fs::temp_directory_path() / "g2c2-XXXXXX").string();
    if (!mkdtemp(tmpl.data())) fatal("cannot make a temporary folder");
    tmp_dir = tmpl;
    std::string obj = tmp_dir + "/" + prog->name + ".o";
    std::string exe = tmp_dir + "/" + prog->name;
    std::string err;
    if (!gen_program(prog, release, obj, err)) fatal(err);

    // The runtime sits beside G2C2: bin/g2c2 and lib/libg2rt.a, in the build folder and in a release alike.
    fs::path self = llvm::sys::fs::getMainExecutable(g_argv0, (void*)&quote);
    std::string runtime = (self.parent_path().parent_path() / "lib" / "libg2rt.a").string();
    if (!fs::exists(runtime)) fatal("cannot find the runtime, " + runtime);
    link(obj, runtime, exe);
    return exe;
}

int main(int argc, char** argv) {
    g_argv0 = argv[0];
    if (argc < 2) usage();
    std::string command = argv[1];
    bool release = false;
    std::string target;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--release") release = true;
        else if (target.empty()) target = a;
        else usage();
    }
    if (target.empty()) target = ".";
    if (command == "run") {
        std::string tmp_dir;
        std::string exe = build(target, release, tmp_dir);
        int status = std::system(quote(exe).c_str());
        fs::remove_all(tmp_dir);
        if (status == -1) fatal("cannot run " + exe);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return 128 + WTERMSIG(status);
    }
    if (command == "build") {
        std::string tmp_dir;
        build(target, release, tmp_dir);
        fs::remove_all(tmp_dir);
        fatal("the program checks and builds, but where `g2c2 build` puts it is not decided yet; `g2c2 run` runs it");
    }
    usage();
}
