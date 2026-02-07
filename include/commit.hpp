#include "diff2.hpp"
#include "utils copy.hpp"
#include <ctime>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

const fs::path current_path = fs::current_path();
const fs::path shit_dir = current_path / ".shit";
const fs::path objects_dir = shit_dir / "objects";
const fs::path versions_dir = shit_dir / "versions";
const fs::path commits_dir = shit_dir / "commits";
const fs::path diffs_dir = shit_dir / "diffs";
const fs::path head_file = shit_dir / "HEAD.txt";
const fs::path index_file = shit_dir / "index.txt";

struct CommitFile {
    std::string path;
    std::string hash;

    static bool _elem_writer(const CommitFile &cf, std::ofstream &file) {
        return write_string_to_file(cf.path, file) &&
               write_string_to_file(cf.hash, file) && !file.fail();
    }
    static bool _elem_reader(CommitFile &cf, std::ifstream &file) {
        return read_string_from_file(cf.path, file) &&
               read_string_from_file(cf.hash, file) && !file.fail();
    }
};

struct CommitFileChange {
    std::string path;
    std::string hash_before;
    std::string hash_after;
    std::vector<DiffOp> diff_ops;

    static bool _elem_writer(const CommitFileChange &cfc, std::ofstream &file) {
        return (write_string_to_file(cfc.path, file) &&
                write_string_to_file(cfc.hash_before, file) &&
                write_string_to_file(cfc.hash_after, file) &&

                write_vector_to_file(cfc.diff_ops, file, DiffOp::_elem_writer));
    }

    static bool _elem_reader(CommitFileChange &cfc, std::ifstream &file) {
        return (read_string_from_file(cfc.path, file) &&
                read_string_from_file(cfc.hash_before, file) &&
                read_string_from_file(cfc.hash_after, file) &&

                read_vector_from_file(cfc.diff_ops, file, DiffOp::_elem_reader));
    }
};

struct Commit {
    std::string id;        // Хэш содержимого коммита (пока хэш только автора и сообщения)
    std::string parent_id; // id предыдущего коммита (пустой для первого)
    std::string author;    // Автор
    std::string message;   // Сообщение
    std::time_t timestamp; // UNIX-время создания

    std::vector<CommitFile> tracked_files; // Все отслеживаемые файлы

    std::vector<std::string> added_files;   // Файлы, добавленные в текущем коммите
    std::vector<std::string> deleted_files; // Файлы, удалённые в текущем коммите
    std::vector<CommitFileChange>
        modified_files; // Файлы, модифицированные в текущем коммите

    bool write_to_file(fs::path path) {
        std::ofstream file(path, std::ios::binary);
        if (!file || !file.is_open()) {
            std::cerr << "Error: cannot open file " << path << std::endl;
            return false;
        }

        if (!write_string_to_file(parent_id, file)) {
            std::cerr << "Error: cannot write string " << parent_id << " to file "
                      << std::endl;
            return false;
        }
        if (!write_string_to_file(author, file)) {
            std::cerr << "Error: cannot write string " << author << " to file "
                      << std::endl;
            return false;
        }
        if (!write_string_to_file(message, file)) {
            std::cerr << "Error: cannot write string " << message << " to file "
                      << std::endl;
            return false;
        }

        if (!file.write(reinterpret_cast<const char *>(&timestamp), sizeof(time_t))) {
            std::cerr << "Error: cannot write time_t " << timestamp << " to file "
                      << std::endl;
            return false;
        }

        try {
            if (!write_vector_to_file(tracked_files, file, CommitFile::_elem_writer)) {
                return false;
            }
        } catch (...) {
            std::cerr << "Error: cannot write vector tracked_files" << std::endl;
            return false;
        }

        try {
            if (!write_vector_to_file(added_files, file, write_string_to_file)) {
                return false;
            }
        } catch (...) {
            std::cerr << "Error: cannot write vector added_files" << std::endl;
            return false;
        }
        try {
            if (!write_vector_to_file(deleted_files, file, write_string_to_file)) {
                return false;
            }
        } catch (...) {
            std::cerr << "Error: cannot write vector deleted_files" << std::endl;
            return false;
        }

        try {
            if (!write_vector_to_file(modified_files, file,
                                      CommitFileChange::_elem_writer)) {
                return false;
            }
        } catch (...) {
            std::cerr << "Error: cannot write vector modified_files" << std::endl;
            return false;
        }

        return true;
    }

    bool load_from_file(fs::path path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        id = path.filename().string();

        if (!read_string_from_file(parent_id, file)) {
            return false;
        }
        if (!read_string_from_file(author, file)) {
            return false;
        }
        if (!read_string_from_file(message, file)) {
            return false;
        }

        if (!file.read(reinterpret_cast<char *>(&timestamp), sizeof(time_t))) {
            return false;
        }

        if (!read_vector_from_file(tracked_files, file, CommitFile::_elem_reader)) {
            return false;
        }

        if (!read_vector_from_file(added_files, file, read_string_from_file)) {
            return false;
        }
        if (!read_vector_from_file(deleted_files, file, read_string_from_file)) {
            return false;
        }

        if (!read_vector_from_file(modified_files, file,
                                   CommitFileChange::_elem_reader)) {
            return false;
        }

        return true;
    }

    void print() const {
        std::cout << "commit " << id << '\n'
                  << '\t' << "parent: " << parent_id << '\n'
                  << '\t' << "author: " << author << '\n'
                  << '\t' << "message: " << message << '\n'
                  << '\t' << "time: " << timestamp << '\n';
        std::cout << "\tadded files: " << added_files.size() << '\n';
        for (const auto &file : added_files) {
            std::cout << "\t  " << file << '\n';
        }
        std::cout << "\tdeleted files: " << deleted_files.size() << '\n';
        for (const auto &file : deleted_files) {
            std::cout << "\t  " << file << '\n';
        }
        std::cout << "\tmodified files: " << modified_files.size() << '\n';
        for (const auto &fileinfo : modified_files) {
            std::cout << "\t  " << fileinfo.path << '\n'
                      << "\t\thash before: " << fileinfo.hash_before << '\n'
                      << "\t\thash after: " << fileinfo.hash_after << '\n';
        }
        std::cout << std::endl;
    }
};

std::string get_current_commit_id() {
    std::fstream head(head_file);
    if (!head) {
        std::cerr << "No HEAD found at " << head_file << std::endl;
        // return "";
    }
    std::string line;
    std::getline(head, line);
    return line;
}
std::vector<std::string> get_tracked_files() {
    std::ifstream idx(index_file);
    if (!idx) {
        std::cerr << "No index.txt found at " << index_file << std::endl;
        return {};
    }
    std::vector<std::string> result;
    std::string line;
    while (std::getline(idx, line)) {
        if (line.empty())
            continue;
        fs::path file_path = current_path / line;
        if (!fs::exists(file_path)) {
            std::cerr << "Tracked file " << file_path << " not found" << std::endl;
            continue;
        }
        result.push_back(line);
    }
    return result;
}

Commit gen_commit(const std::string &author, const std::string &msg) {
    Commit commit;
    commit.parent_id = get_current_commit_id();
    commit.author = author;
    commit.message = msg;
    commit.timestamp = std::time(nullptr);

    std::cout << ANSI_RED << "debug: before loading prev_commit\n";
    commit.print();
    std::cout << ANSI_RESET << '\n';

    // Загружаем предыдущий коммит (если есть)
    Commit prev_commit;
    if (!commit.parent_id.empty()) {
        if (!prev_commit.load_from_file(commits_dir / (commit.parent_id + ".bin"))) {
            std::cerr << "Error: cannot load commit "
                      << commits_dir / (commit.parent_id + ".bin") << std::endl;
        }
    }

    // Собираем снимок текущих tracked файлов
    std::vector<std::string> tracked;
    try {
        tracked = get_tracked_files();
    } catch (...) {
        std::cerr << "Error:  get_tracked_files failed!" << std::endl;
    }
    for (auto &path : tracked) {
        std::cout << "Tracked file: " << path << '\n';
        CommitFile cf;
        cf.path = path;
        cf.hash = xxhash_file(current_path / path);

        fs::path obj_path = objects_dir / cf.hash;
        if (!fs::exists(obj_path)) {
            if (!compress_file(current_path / cf.path, obj_path)) {
                std::cerr << "Error: cannor compress file " << current_path / cf.path
                          << " to " << obj_path << std::endl;
            }
        }
        commit.tracked_files.push_back(cf);
    }
    commit.print();
    // Сравнение со старым коммитом
    for (auto &newf : commit.tracked_files) {
        auto it = std::find_if(
            prev_commit.tracked_files.begin(), prev_commit.tracked_files.end(),
            [&](const CommitFile &pf) { return pf.path == newf.path; });

        if (it == prev_commit.tracked_files.end()) {
            commit.added_files.push_back(newf.path);
        } else if (it->hash != newf.hash) {
            CommitFileChange ch;
            ch.hash_before = it->hash;
            ch.hash_after = newf.hash;
            ch.path = newf.path;

            decompress_file(objects_dir / it->hash, objects_dir / "tmp.bin");

            std::fstream tmp(objects_dir / "tmp.bin", std::ios::binary);
            std::fstream f(newf.path, std::ios::binary);

            auto n1 = fs::file_size(objects_dir / "tmp.bin"),
                 n2 = fs::file_size(newf.path);

            std::vector<char> old_file(n1);
            tmp.read(old_file.data(), n1);

            std::vector<char> new_file(n2);
            f.read(new_file.data(), n2);

            ch.diff_ops = myers_diff(old_file, new_file);

            /* сериализуем диифф */
            write_diff((diffs_dir / (ch.hash_before + ".diff")).string(), ch.diff_ops);

            commit.modified_files.push_back(ch);
        }
    }
    commit.print();
    for (auto &oldf : prev_commit.tracked_files) {
        auto it =
            std::find_if(commit.tracked_files.begin(), commit.tracked_files.end(),
                         [&](const CommitFile &nf) { return nf.path == oldf.path; });

        if (it == commit.tracked_files.end()) {
            commit.deleted_files.push_back(oldf.path);
        }
    }
    commit.print();
    /* новый хэш = хэш автор + мессага + timestamp */
    std::string content =
        commit.author + commit.message + std::to_string(commit.timestamp);
    commit.id = xxhash_bytes(content.c_str(), content.size());
    commit.print();
    // сериализация
    fs::path commit_file = commits_dir / (commit.id + ".bin");
    std::cout << commit_file << std::endl;

    commit.write_to_file(commit_file);
    bool done = commit.write_to_file(commit_file);

    if (!done) {
        std::cerr << "write_to_file returned " << ANSI_RED << "false" << ANSI_RESET
                  << std::endl;
        std::cerr << "Error: cannot write commit to file: " << commit_file << std::endl;
    } else {
        std::cerr << "write_to_file returned " << ANSI_GREEN << "true" << ANSI_RESET
                  << std::endl;
    }

    // обновляем HEAD
    std::ofstream(head_file, std::ios::trunc) << commit.id;

    return commit;
}

bool compare_commits(const Commit &a, const Commit &b) {
    return a.timestamp < b.timestamp;
}
std::set<Commit, bool (*)(const Commit &, const Commit &)> commit_history() {
    std::set<Commit, bool (*)(const Commit &, const Commit &)> history(compare_commits);
    Commit c;
    for (auto &entry : fs::directory_iterator(commits_dir)) {
        if (c.load_from_file(entry.path())) {
            history.emplace(c);
        } else {
            std::cerr << "Error: unable to load commit from file " << entry.path()
                      << std::endl;
        }
    }
    return history;
}