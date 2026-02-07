#include "commit.hpp"
#include <set>
#include <unordered_map>
/*
.shit
    HEAD <- id текущего коммита
    index.txt <- список отслеживаемых файлов
    diffs <- файлы диффов
        ...
    objects <- сжатые версии файлов
        ...
    versions <-
        ...
    commits <- сборник бинарников коммитов
        ...
*/

// const fs::path current_path = fs::current_path();
// const fs::path shit_dir = current_path / ".shit";
// const fs::path objects_dir = shit_dir / "objects";
// const fs::path versions_dir = shit_dir / "versions";
// const fs::path commits_dir = shit_dir / "commits";
// const fs::path diffs_dir = shit_dir / "diffs";
// const fs::path head_file = shit_dir / "HEAD.txt";
// const fs::path index_file = shit_dir / "index.txt";

void vcshit_init(const std::string &dir) {
    if (fs::exists(shit_dir)) {
        std::cerr << ".shit already exists!\n";
        return;
    }

    // fs::create_directories(versions_dir);
    fs::create_directories(objects_dir);
    fs::create_directories(commits_dir);
    fs::create_directories(diffs_dir);

    std::ofstream(head_file) << "";
    std::ofstream(index_file) << "";
}

void vcshit_add_file(const std::string &filename) {
    std::string buffer = "";
    std::fstream index = std::fstream(index_file, std::ios::in | std::ios::binary);
    if (!index) {
        std::cerr << "Error while opening index" << index_file << std::endl;
        return;
    }

    while (std::getline(index, buffer)) {
        if (buffer == filename) {
            std::cout << "File already added" << std::endl;
            return;
        }
    }

    index = std::fstream(index_file, std::ios::out | std::ios::app);
    if (index) {
        index << filename << std::endl;
    }

    return;
}

void vcshit_del_file(const std::string &filename) {
    std::ifstream in(index_file, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open index\n";
        return;
    }

    std::vector<std::string> lines;
    std::string buffer;
    bool found = false;
    size_t temp;
    while (std::getline(in, buffer)) {
        // убираем \r и пробелы
        temp = buffer.find_last_not_of(" \t\r\n");
        if (temp == std::string::npos) {
            std::cerr << "Error: found wrong file name inside index: " << buffer
                      << std::endl;
            continue;
        }
        buffer.erase(temp + 1);

        if (buffer == filename) {
            found = true;
            continue;
        }

        if (!buffer.empty())
            lines.push_back(buffer);
    }

    if (!found) {
        std::cerr << "File not tracked: " << filename << std::endl;
        return;
    }

    std::ofstream out(index_file, std::ios::trunc | std::ios::binary);
    if (!out) {
        std::cerr << "Error with file" << index_file << std::endl;
        return;
    }
    for (const auto &line : lines) {
        out << line << std::endl;
    }
}

inline void vcshit_commit(const std::string &author, const std::string &msg) {
    gen_commit(author, msg);
}
inline void vcshit_print_commit(const std::string &id) {
    Commit commit;
    commit.load_from_file(commits_dir / (id + ".bin"));
    commit.print();
}

void vcshit_revert(const std::string &target_commit_id) {

    fs::path target_commit_path = commits_dir / (target_commit_id + ".bin");
    if (!fs::exists(target_commit_path)) {
        std::cerr << "Error: Commit " << target_commit_id << " not found" << std::endl;
        return;
    }

    Commit target;
    if (!target.load_from_file(target_commit_path)) {
        std::cerr << "Error: cannot load commit from file " << target_commit_path
                  << std::endl;
        return;
    }

    /* удалить все файлы из текущей директории */
    for (auto &path : fs::recursive_directory_iterator(current_path)) {
        if (path.path().string().find(".shit") != std::string::npos)
            continue;
        try {
            fs::remove_all(path);
        } catch (const fs::filesystem_error &e) {
            std::cerr << "Cannot remove " << path << ": " << e.what() << "\n";
        }
    }

    /* восстановить файлы из коммита target_commit */
    for (auto &f : target.tracked_files) {
        fs::path dest_path = current_path / f.path;
        fs::create_directories(dest_path.parent_path());

        fs::path obj_path = objects_dir / f.hash;
        if (!fs::exists(obj_path)) {
            std::cerr << "Missing object for file " << f.path << " (" << f.hash << ")"
                      << std::endl;
            continue;
        }

        if (!decompress_file(obj_path, dest_path)) {
            std::cerr << "Failed to restore file " << f.path << std::endl;
        }
    }

    /* Обновляем HEAD */
    std::ofstream(head_file, std::ios::trunc) << target_commit_id;

    /* Пересобираем index.txt */
    std::ofstream(index_file, std::ios::trunc).close();
    std::ofstream idx(index_file);
    for (auto &f : target.tracked_files)
        idx << f.path << "\n";

    std::cout << "Reverted to commit " << target_commit_id << std::endl;
}

void resolve_add(const std::string &filename) {
    // shit add .
    if (filename == ".") {
        for (const auto &entry : fs::recursive_directory_iterator(current_path)) {
            // пропускаем .shit
            if (entry.path().string().find(".shit") != std::string::npos)
                continue;

            if (entry.is_regular_file()) {
                fs::path rel = fs::relative(entry.path(), current_path);
                vcshit_add_file(rel.string());
            }
        }
        return;
    }

    fs::path base = fs::current_path() / filename;
    // директория
    if (fs::exists(base) && fs::is_directory(base)) {
        for (const auto &entry : fs::recursive_directory_iterator(base)) {
            if (entry.is_regular_file()) {
                fs::path rel = fs::relative(entry.path(), current_path);
                vcshit_add_file(rel.string());
            }
        }
        return;
    }

    // обычный файл
    if (fs::exists(base) && fs::is_regular_file(base)) {
        vcshit_add_file(filename);
        return;
    }

    std::cerr << "Path not found: " << filename << '\n';
}

bool inline vcshit_compress(const char *src, const char *dest) {
    if (fs::is_directory(src)) {
        return compress_dir(src, dest);
    }
    return compress_file(src, dest);
}

void vcshit_status() {
    auto all_files = std::set<fs::path>();
    for (auto &entry : fs::recursive_directory_iterator(current_path)) {
        if (fs::is_directory(entry.path())) {
            continue;
        }
        if (entry.path().string().find(".shit") != std::string::npos) {
            continue;
        }
        all_files.emplace(fs::relative(entry.path(), current_path));
    }

    auto t_v = get_tracked_files();
    std::set<fs::path> tracked_files = std::set<fs::path>();
    for (auto &f : t_v)
        tracked_files.emplace(f);

    std::vector<fs::path> deleted;
    std::vector<fs::path> added;
    std::vector<fs::path> modified;
    
    Commit latest;
    if (!latest.load_from_file(commits_dir / (get_current_commit_id() + ".bin"))) {
        std::cerr << "Error: unable to load commit " << commits_dir / (get_current_commit_id() + ".bin") << std::endl;
        return ;
    }

    std::unordered_map<fs::path, std::string> latest_file_hash;
    for (auto &cf : latest.tracked_files) {
        latest_file_hash[cf.path] = cf.hash;
    }
    fs::path rel;
    for (auto &i : all_files) {
        rel = fs::relative(i, current_path);
        if (tracked_files.count(rel) == 0) {
            added.push_back(i);
            continue;
        }
        // for (auto &file : latest.tracked_files) {
        //     if (file.path == fs::relative(i, current_path) &&
        //         file.hash != xxhash_file(i)) {
        //         modified.push_back(fs::relative(i, current_path));
        //         break;
        //     }
        // }
        if (xxhash_file(i) != latest_file_hash[rel]) {
            modified.push_back(rel);
        }
    }
    for (auto &i : tracked_files) {
        rel = fs::relative(i, current_path);
        if (all_files.count(rel) == 0) {
            deleted.push_back(rel);
        }
    }

    /* Вывести предложения в консоль */
    if (added.size() > 0) {
        std::cout << "Found " << added.size()
                  << " new files. Run command below to add them to index:\n";
        std::cout << "shit add <file>\n";
        for (auto &i : added) {
            std::cout << "  " << ANSI_RED << fs::relative(i, current_path) << ANSI_RESET
                      << '\n';
        }
        std::cout << std::endl;
    }
    if (deleted.size() > 0) {
        std::cout << "Found " << deleted.size()
                  << " files was deleted. Run command below to delete them from index:\n";
        std::cout << "shit delete <file>\n";
        for (auto &i : deleted) {
            std::cout << "  " << ANSI_RED << fs::relative(i, current_path) << ANSI_RESET
                      << '\n';
        }
        std::cout << std::endl;
    }
    if (modified.size() > 0) {
        std::cout << "Found " << modified.size() << " modified files.\n";
        std::cout << "(run \"shit commit <author> <message>\" to apply changes)\n";
        std::cout << "NOT_YET_IMPLEMENTED (run \"shit restore <filename>\" to discard "
                     "changes)\n";
        for (auto &i : modified) {
            std::cout << "  " << ANSI_RED << fs::relative(i, current_path) << ANSI_RESET
                      << '\n';
        }
        std::cout << std::endl;
    }
}

/* TODO */
void vscshit_print_commit_history() {
    auto history = commit_history();
    for (auto &c : history)
        c.print();
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    enable_vt_mode();
#endif

    if (argc > 1) {
        std::string arg1 = std::string(argv[1]);

        if (arg1 == "init") {
            vcshit_init(fs::current_path().string());
            return 0;
        }
        if (arg1 == "add") {
            if (argc > 2) {
                resolve_add(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify file";
        }
        if (arg1 == "delete") {
            if (argc > 2) {
                vcshit_del_file(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify file";
        }
        if (arg1 == "commit") {
            if (argc > 3) {
                vcshit_commit(std::string(argv[2]), std::string(argv[3]));
                return 0;
            }
            std::cerr << "Commit: shit commit <author> <message>";
        }
        if (arg1 == "revert") {
            if (argc > 2) {
                vcshit_revert(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify version";
        }
        if (arg1 == "compress") {
            if (argc > 3) {
                bool success = vcshit_compress(argv[2], argv[3]);
                if (!success) {
                    std::cerr << "Error while compressing file " << argv[2] << std::endl;
                }
                return 0;
            }
            std::cerr << "Need to specify <src> and <dest> files";
        }
        if (arg1 == "log") {
            if (argc > 2) {
                if (std::string(argv[2]) == "--full") {
                    vscshit_print_commit_history();
                    return 0;
                }
                vcshit_print_commit(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify commit id";
        }
        if (arg1 == "print_diff") {
            if (argc > 2) {
                print_diff_colored(
                    read_diff((diffs_dir / (std::string(argv[2]) + ".diff")).string()));
                return 0;
            }
            std::cerr << "Need to specify diff id";
        }

        if (arg1 == "status") {
            vcshit_status();
            return 0;
        }
        if (arg1 == "-h") {
            std::cout
                << "shit commit <version> \nshit revert <version> \nshit compress <src> "
                   "<dest> \nshit comvers <version> \ndiff <file a> <file b> \n"
                << std::endl;
        }
    }
    return 0;
}
