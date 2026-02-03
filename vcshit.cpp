#include "commit.cpp"

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

void vcshit_init(const std::string &dir)
{
    if (fs::exists(shit_dir))
    {
        std::cerr << ".shit already exists!\n";
        return;
    }

    fs::create_directories(versions_dir);
    fs::create_directories(objects_dir);
    fs::create_directories(commits_dir);
    fs::create_directories(diffs_dir);

    std::ofstream(head_file) << "";
    std::ofstream(index_file) << "";
}

void vcshit_add_file(const std::string &filename)
{
    std::string buffer = "";
    std::fstream index = std::fstream(index_file, std::ios::in | std::ios::binary);
    if (!index)
    {
        std::cerr << "Error while opening index" << index_file << std::endl;
        return;
    }

    while (std::getline(index, buffer))
    {
        if (buffer == filename)
        {
            std::cout << "File already added" << std::endl;
            return;
        }
    }

    index = std::fstream(index_file, std::ios::out | std::ios::app);
    if (index)
    {
        index << filename << std::endl;
    }

    return;
}

void vcshit_del_file(const std::string &filename)
{
    std::ifstream in(index_file, std::ios::binary);
    if (!in)
    {
        std::cerr << "Cannot open index\n";
        return;
    }

    std::vector<std::string> lines;
    std::string buffer;
    bool found = false;
    size_t temp;
    while (std::getline(in, buffer))
    {
        // убираем \r и пробелы
        temp = buffer.find_last_not_of(" \t\r\n");
        if (temp == std::string::npos)
        {
            std::cerr << "Error: found wrong file name inside index: " << buffer << std::endl;
            continue;
        }
        buffer.erase(temp + 1);

        if (buffer == filename)
        {
            found = true;
            continue;
        }

        if (!buffer.empty())
            lines.push_back(buffer);
    }

    if (!found)
    {
        std::cerr << "File not tracked: " << filename << std::endl;
        return;
    }

    std::ofstream out(index_file, std::ios::trunc | std::ios::binary);
    if (!out)
    {
        std::cerr << "Error with file" << index_file << std::endl;
        return;
    }
    for (const auto &line : lines)
    {
        out << line << std::endl;
    }
}

inline void vcshit_commit(const std::string &author, const std::string &msg)
{
    gen_commit(author, msg);
}
inline void vcshit_print_commit(const std::string &id)
{
    Commit commit;
    commit.deserialize(commits_dir / (id + ".bin"));
    commit.print();
}
/// @brief TODO DELETE
/// @param version
void vcshit_compress_version(const std::string &version)
{
    fs::path version_folder = versions_dir / version;
    fs::create_directories(version_folder);

    std::string buffer = "";
    std::ifstream index = std::ifstream(index_file);
    size_t temp;
    fs::path from;

    while (std::getline(index, buffer))
    {
        std::cout << buffer << '\n';
        temp = buffer.find_last_not_of(" \t\r\n");
        if (temp == std::string::npos)
        {
            std::cerr << "Error: found wrong file name inside index: " << buffer << std::endl;
            continue;
        }
        buffer.erase(temp + 1);
        from = current_path / buffer;
        // считаем хэш файла
        std::string fhash = xxhash_file(from);
        std::cout << "Hash for " << buffer << " is " << fhash << '\n';
        fs::path compressed_file_path = (objects_dir / fhash);
        std::cout << "Compressing " << from << " to " << compressed_file_path << '\n';
        // если путь до compressed_file_path не существует, значит сжатый файл еще не существует
        // если же путь есть, то файл с данным хэшэм уже есть -> данные те же -> записывать не надо
        if (!fs::exists(compressed_file_path))
        {
            // при ошибке функция сама пишет в cerr
            compress_file(from, objects_dir / fhash);
        }
        else
        {
            std::cerr << "Compressed file " << compressed_file_path << "already exists\n";
        }
    }
    std::cerr << std::endl;
    std::cout << std::endl;
}

// TODO
// Добавить валидацию версии (чтобы не удалить файлы, которые удалять не надо)
void vcshit_revert(const std::string &version)
{
    /* TODO */
    /* предложить сохранить изменения, если есть */

    /* удалить все файлы из текущей директории */

    for (auto &path : fs::recursive_directory_iterator(current_path))
    {
        if (path.path().string().find(".shit") != std::string::npos)
            continue;
        try
        {
            fs::remove_all(path);
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Cannot remove " << path << ": " << e.what() << "\n";
        }
    }

    /* восстановить файлы из коммита version */

    Commit target;
    target.deserialize(commits_dir / (version + ".bin"));
     for (auto &f : target.tracked_files)
    {
        fs::path dest_path = current_path / f.path;
        fs::create_directories(dest_path.parent_path());

        fs::path obj_path = objects_dir / f.hash;
        if (!fs::exists(obj_path))
        {
            std::cerr << "Missing object for " << f.path << " (" << f.hash << ")" << std::endl;
            continue;
        }

        if (!decompress_file(obj_path, dest_path))
        {
            std::cerr << "Failed to restore " << f.path << std::endl;
        }
    }

    /* Обновляем HEAD */
    std::ofstream(head_file, std::ios::trunc) << version;

    /* Пересобираем index.txt */
    std::ofstream(index_file, std::ios::trunc).close();
    std::ofstream idx(index_file);
    for (auto &f : target.tracked_files)
        idx << f.path << "\n";

    std::cout << "Reverted to commit " << version << std::endl;
}

void resolve_add(const std::string &filename)
{
    // shit add .
    if (filename == ".")
    {
        for (const auto &entry : fs::recursive_directory_iterator(current_path))
        {
            // пропускаем .shit
            if (entry.path().string().find(".shit") != std::string::npos)
                continue;

            if (entry.is_regular_file())
            {
                fs::path rel = fs::relative(entry.path(), current_path);
                vcshit_add_file(rel.string());
            }
        }
        return;
    }

    fs::path base = fs::current_path() / filename;
    // директория
    if (fs::exists(base) && fs::is_directory(base))
    {
        for (const auto &entry : fs::recursive_directory_iterator(base))
        {
            if (entry.is_regular_file())
            {
                fs::path rel = fs::relative(entry.path(), current_path);
                vcshit_add_file(rel.string());
            }
        }
        return;
    }

    // обычный файл
    if (fs::exists(base) && fs::is_regular_file(base))
    {
        vcshit_add_file(filename);
        return;
    }

    std::cerr << "Path not found: " << filename << '\n';
}

bool inline vcshit_compress(const char *src, const char *dest)
{
    if (fs::is_directory(src))
    {
        return compress_dir(src, dest);
    }
    return compress_file(src, dest);
}


int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        std::string arg1 = std::string(argv[1]);

        if (arg1 == "init")
        {
            vcshit_init(fs::current_path().string());
            return 0;
        }
        if (arg1 == "add")
        {
            if (argc > 2)
            {
                resolve_add(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify file";
        }
        if (arg1 == "delete")
        {
            if (argc > 2)
            {
                vcshit_del_file(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify file";
        }
        if (arg1 == "commit")
        {
            if (argc > 3)
            {
                vcshit_commit(std::string(argv[2]), std::string(argv[3]));
                return 0;
            }
            std::cerr << "Commit: shit commit <author> <message>";
        }
        if (arg1 == "revert")
        {
            if (argc > 2)
            {
                vcshit_revert(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify version";
        }
        if (arg1 == "compress")
        {
            if (argc > 3)
            {
                bool success = vcshit_compress(argv[2], argv[3]);
                if (!success)
                {
                    std::cerr << "Error while compressing file " << argv[2] << std::endl;
                }
                return 0;
            }
            std::cerr << "Need to specify <src> and <dest> files";
        }
        if (arg1 == "comvers")
        {
            if (argc > 2)
            {
                vcshit_compress_version(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify version name";
        }
        if (arg1 == "log")
        {
            if (argc > 2)
            {
                vcshit_print_commit(std::string(argv[2]));
                return 0;
            }
            std::cerr << "Need to specify commit id";
        }
        if (arg1 == "print_diff")
        {
            if (argc > 2)
            {
                enable_vt_mode();
                print_diff_colored(read_diff((diffs_dir / (std::string(argv[2]) + ".diff")).string()));
                return 0;
            }
            std::cerr << "Need to specify diff id";
        }
        if (arg1 == "-h")
        {
            std::cout << "shit commit <version> \nshit revert <version> \nshit compress <src> <dest> \nshit comvers <version> \ndiff <file a> <file b> \n"
                      << std::endl;
        }
    }
    return 0;
}
