#include <string>
#include <vector>
#include <ctime>
#include <filesystem>
#include "diff2.hpp"
#include "utils copy.hpp"

const fs::path current_path = fs::current_path();
const fs::path shit_dir = current_path / ".shit";
const fs::path objects_dir = shit_dir / "objects";
const fs::path versions_dir = shit_dir / "versions";
const fs::path commits_dir = shit_dir / "commits";
const fs::path diffs_dir = shit_dir / "diffs";
const fs::path head_file = shit_dir / "HEAD.txt";
const fs::path index_file = shit_dir / "index.txt";

struct CommitFile
{
    std::string path;
    std::string hash;
};

struct CommitFileChange
{
    std::string path;
    std::string hash_before;
    std::string hash_after;
    std::vector<DiffOp> diff_ops;
};

struct Commit
{
    std::string id;        // Хэш содержимого коммита (пока хэш только автора и сообщения)
    std::string parent_id; // id предыдущего коммита (пустой для первого)
    std::string author;    // Автор
    std::string message;   // Сообщение
    std::time_t timestamp; // UNIX-время создания

    std::vector<CommitFile> tracked_files; // Все отслеживаемые файлы

    std::vector<std::string> added_files;         // Файлы, добавленные в текущем коммите
    std::vector<std::string> deleted_files;       // Файлы, удалённые в текущем коммите
    std::vector<CommitFileChange> modified_files; // Файлы ,модифицированные в текущем коммите

    void serialize(std::filesystem::path path)
    {
        std::ofstream out = std::ofstream(path, std::ios::binary);

        if (!out)
        {
            std::cerr << "Error while opening file " << path << std::endl;
            return;
        }

        write_string(id, out);
        write_string(parent_id, out);
        write_string(author, out);
        write_string(message, out);

        out.write(reinterpret_cast<char *>(&timestamp), sizeof(timestamp));

        size_t files_count = tracked_files.size();
        out.write(reinterpret_cast<char *>(&files_count), sizeof(size_t));
        for (auto &file : tracked_files)
        {
            write_string(file.path, out);
            write_string(file.hash, out);
        }

        size_t added_files_count = added_files.size();
        out.write(reinterpret_cast<char *>(&added_files_count), sizeof(added_files_count));
        for (auto &file : added_files)
        {
            write_string(file, out);
        }

        size_t deleted_files_count = deleted_files.size();
        out.write(reinterpret_cast<char *>(&deleted_files_count), sizeof(deleted_files_count));
        for (auto &file : deleted_files)
        {
            write_string(file, out);
        }

        size_t modified_files_count = modified_files.size();
        out.write(reinterpret_cast<char *>(&modified_files_count), sizeof(modified_files_count));
        for (auto &mod : modified_files)
        {

            write_string(mod.path, out);
            write_string(mod.hash_before, out);
            write_string(mod.hash_after, out);

            size_t diff_count = mod.diff_ops.size();
            out.write(reinterpret_cast<char *>(&diff_count), sizeof(diff_count));
            for (auto &diff : mod.diff_ops)
            {

                out.write(reinterpret_cast<char *>(&diff.type), sizeof(uint8_t));
                out.write(reinterpret_cast<char *>(&diff.pos), sizeof(uint32_t));
                out.write(reinterpret_cast<char *>(&diff.byte), sizeof(uint8_t));
            }
        }
    }

    void deserialize(std::filesystem::path path)
    {
        std::ifstream in = std::ifstream(path, std::ios::binary);
        if (!in)
        {
            std::cerr << "Error while opening file " << path << std::endl;
            return;
        }

        read_string(id, in);
        read_string(parent_id, in);
        read_string(author, in);
        read_string(message, in);

        in.read(reinterpret_cast<char *>(&timestamp), sizeof(std::time_t));

        size_t tracked_files_count;
        in.read(reinterpret_cast<char *>(&tracked_files_count), sizeof(size_t));
        tracked_files.resize(tracked_files_count);
        for (auto &file : tracked_files)
        {
            read_string(file.path, in);
            read_string(file.hash, in);
        }

        size_t added_count, deleted_count, modified_count;

        in.read(reinterpret_cast<char *>(&added_count), sizeof(size_t));
        added_files.resize(added_count);
        for (auto &file : added_files)
        {
            read_string(file, in);
        }

        in.read(reinterpret_cast<char *>(&deleted_count), sizeof(size_t));
        deleted_files.resize(deleted_count);
        for (auto &file : deleted_files)
        {
            read_string(file, in);
        }

        in.read(reinterpret_cast<char *>(&modified_count), sizeof(size_t));
        modified_files.resize(modified_count);
        for (auto &mod : modified_files)
        {
            read_string(mod.path, in);
            read_string(mod.hash_before, in);
            read_string(mod.hash_after, in);

            size_t diff_count;
            in.read(reinterpret_cast<char *>(&diff_count), sizeof(size_t));
            mod.diff_ops.resize(diff_count);
            for (auto &diff : mod.diff_ops)
            {
                in.read(reinterpret_cast<char *>(&diff.type), sizeof(uint8_t));
                in.read(reinterpret_cast<char *>(&diff.pos), sizeof(size_t));
                in.read(reinterpret_cast<char *>(&diff.byte), sizeof(uint8_t));
            }
        }
    }

    void print() const
    {
        std::cout << "commit " << id << '\n'
                  << '\t' << "parent: " << parent_id << '\n'
                  << '\t' << "author: " << author << '\n'
                  << '\t' << "message: " << message << '\n'
                  << '\t' << "time: " << timestamp << '\n';
        std::cout << "\tadded files: " << added_files.size() << '\n';
        for (const auto &file : added_files)
        {
            std::cout << "\t  " << file << '\n';
        }
        std::cout << "\tdeleted files: " << deleted_files.size() << '\n';
        for (const auto &file : deleted_files)
        {
            std::cout << "\t  " << file << '\n';
        }
        std::cout << "\tmodified files: " << modified_files.size() << '\n';
        for (const auto &fileinfo : modified_files)
        {
            std::cout << "\t  " << fileinfo.path << '\n'
                      << "\t\thash before: " << fileinfo.hash_before << '\n'
                      << "\t\thash after: " << fileinfo.hash_after << '\n';
        }
        std::cout << std::endl;
    }
};

std::string get_current_commit_id()
{
    std::fstream head(head_file);
    if (!head)
    {
        std::cerr << "No HEAD found at " << head_file << std::endl;
        // return "";
    }
    std::string line;
    std::getline(head, line);
    return line;
}
std::vector<std::string> get_tracked_files()
{
    std::ifstream idx(index_file);
    if (!idx)
    {
        std::cerr << "No index.txt found at " << index_file << std::endl;
        return {};
    }
    std::vector<std::string> result;
    std::string line;
    while (std::getline(idx, line))
    {
        if (line.empty())
            continue;
        fs::path file_path = current_path / line;
        if (!fs::exists(file_path))
        {
            std::cerr << "Tracked file " << file_path << " not found" << std::endl;
            continue;
        }
        result.push_back(line);
    }
    return result;
}

template <typename T>
inline bool contains(const std::vector<T> &array, const T &value)
{
    return std::find(array.begin(), array.end(), value) != array.end();
}

Commit gen_commit(const std::string &author, const std::string &msg)
{
    Commit commit;
    commit.parent_id = get_current_commit_id();
    commit.author = author;
    commit.message = msg;
    commit.timestamp = std::time(nullptr);

    std::ifstream head(head_file);
    if (head)
    {
        std::getline(head, commit.parent_id);
    }
    // Загружаем предыдущий коммит (если есть)
    Commit prev_commit;
    if (!commit.parent_id.empty())
    {
        prev_commit.deserialize(commits_dir / (commit.parent_id + ".bin"));
    }

    // Собираем снимок текущих tracked файлов
    std::vector<std::string> tracked = get_tracked_files();
    for (auto &path : tracked)
    {
        CommitFile cf;
        cf.path = path;
        cf.hash = xxhash_file(current_path / path);

        fs::path obj_path = objects_dir / cf.hash;
        if (!fs::exists(obj_path))
        {
            compress_file(current_path / cf.path, obj_path);
        }
        commit.tracked_files.push_back(cf);
    }

    // Сравнение со старым коммитом
    for (auto &newf : commit.tracked_files)
    {
        auto it = std::find_if(prev_commit.tracked_files.begin(), prev_commit.tracked_files.end(),
                               [&](const CommitFile &pf)
                               { return pf.path == newf.path; });

        if (it == prev_commit.tracked_files.end())
        {
            commit.added_files.push_back(newf.path);
        }
        else if (it->hash != newf.hash)
        {
            CommitFileChange ch;
            ch.hash_before = it->hash;
            ch.hash_after = newf.hash;
            ch.path = newf.path;

            decompress_file(objects_dir / it->hash, objects_dir / "tmp.bin");

            std::fstream tmp(objects_dir / "tmp.bin", std::ios::binary);
            std::fstream f(newf.path, std::ios::binary);

            auto n1 = fs::file_size(objects_dir / "tmp.bin"), n2 = fs::file_size(newf.path);

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

    for (auto &oldf : prev_commit.tracked_files)
    {
        auto it = std::find_if(commit.tracked_files.begin(), commit.tracked_files.end(),
                               [&](const CommitFile &nf)
                               { return nf.path == oldf.path; });

        if (it == commit.tracked_files.end())
        {
            commit.deleted_files.push_back(oldf.path);
        }
    }

    /* новый хэш = хэш автор + мессага + timestamp */
    std::string content = commit.author + commit.message + std::to_string(commit.timestamp);
    commit.id = xxhash_bytes(content.c_str(), content.size());

    // сериализация
    fs::path commit_file = commits_dir /  (commit.id + ".bin");
    commit.serialize(commit_file);

    // обновляем HEAD
    std::ofstream(head_file, std::ios::trunc) << commit.id;

    return commit;
}
