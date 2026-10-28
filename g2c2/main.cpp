// g2c2 build [--release] <file or project>
// g2c2 run [--release] <file or project>
//
// A .go2 file is built alone, as package main. A folder is a project: G2C2 walks up from it to the folder holding
// project.go2, whose root files are package main, and whose other packages are folders named by import.

#include "g2c2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

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

    std::string cmd = quote(G2_CLANG) + " " + quote(obj) + " " + quote(G2_RUNTIME_LIB) + " -o " + quote(exe) +
                      " -L" + quote(G2_LIBCXX_DIR) + " -Wl,-rpath," + quote(G2_LIBCXX_DIR);
    if (std::system(cmd.c_str()) != 0) fatal("linking failed: " + cmd);
    return exe;
}

int main(int argc, char** argv) {
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
