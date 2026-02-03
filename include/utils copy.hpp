#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <xxhash.c>
#include <zlib.h>

#define CHUNK_SIZE 4096
namespace fs = std::filesystem;

bool compress_file(const fs::path &src, const fs::path &dest)
{
    uint8_t inbuff[CHUNK_SIZE];
    uint8_t outbuff[CHUNK_SIZE];
    z_stream stream = {0};

    std::ifstream src_file(src, std::ios::binary);
    std::ofstream dest_file(dest, std::ios::binary);

    if (!src_file.is_open() || !dest_file.is_open())
    {
        std::cerr << "Failed to open files!\n";
        return false;
    }

    if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK)
    {
        std::cerr << "deflateInit(...) failed!\n";
        return false;
    }

    int flush;
    do
    {
        src_file.read(reinterpret_cast<char *>(inbuff), CHUNK_SIZE);

        if (src_file.bad())
        {
            std::cerr << "ifstream.read(...) failed!\n";
            deflateEnd(&stream);
            return false;
        }

        stream.avail_in = static_cast<uInt>(src_file.gcount());
        stream.next_in = inbuff;
        flush = src_file.eof() ? Z_FINISH : Z_NO_FLUSH;

        do
        {
            stream.avail_out = CHUNK_SIZE;
            stream.next_out = outbuff;

            if (deflate(&stream, flush) == Z_STREAM_ERROR)
            {
                std::cerr << "deflate error!\n";
                deflateEnd(&stream);
                return false;
            }

            uint32_t nbytes = CHUNK_SIZE - stream.avail_out;
            dest_file.write(reinterpret_cast<char *>(outbuff), nbytes);

            if (dest_file.bad())
            {
                std::cerr << "ofstream.write(...) failed!\n";
                deflateEnd(&stream);
                return false;
            }
        } while (stream.avail_out == 0);
    } while (flush != Z_FINISH);

    return deflateEnd(&stream) == Z_OK;
}

inline bool compress_file(const std::string &src, const std::string &dest)
{
    return compress_file(fs::path(src), fs::path(dest));
}
inline bool compress_file(const char *src, const char *dest)
{
    return compress_file(fs::path(src), fs::path(dest));
}

bool decompress_file(const fs::path &src, const fs::path &dest)
{
    uint8_t inbuff[CHUNK_SIZE];
    uint8_t outbuff[CHUNK_SIZE];

    z_stream stream = {0};

    std::ifstream src_file = std::ifstream(src, std::ios::binary);
    std::ofstream dest_file = std::ofstream(dest, std::ios::binary);

    if (!src_file.is_open() || !dest_file.is_open())
    {
        std::cerr << "Failed to open files!\n";
        return false;
    }

    if (inflateInit(&stream) != Z_OK)
    {
        std::cerr << "inflateInit failed!\n";
        return false;
    }

    int ret;
    do
    {
        src_file.read(reinterpret_cast<char *>(inbuff), CHUNK_SIZE);
        if (src_file.bad())
        {
            std::cerr << "ifstream.read(...) error!\n";
            inflateEnd(&stream);
            return false;
        }
        stream.avail_in = src_file.gcount();
        stream.next_in = inbuff;

        do
        {
            stream.avail_out = CHUNK_SIZE;
            stream.next_out = outbuff;
            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                std::cerr << "inflate failed with " << ret << std::endl;
                inflateEnd(&stream);
                return false;
            }
            dest_file.write(reinterpret_cast<char *>(outbuff), CHUNK_SIZE - stream.avail_out);
        } while (stream.avail_out == 0);

    } while (ret != Z_STREAM_END);
    inflateEnd(&stream);
    return true;
}
inline bool decompress_file(const std::string &src, const std::string &dest)
{
    return decompress_file(fs::path(src), fs::path(dest));
}
inline bool decompress_file(const char *src, const char *dest)
{
    return decompress_file(fs::path(src), fs::path(dest));
}

bool compress_dir(const fs::path &src_dir, const fs::path &dest_dir)
{
    if (!fs::exists(src_dir) || !fs::is_directory(src_dir))
    {
        std::cerr << "Given src_dir does not exist or is not a directory: " << src_dir << std::endl;
        return false;
    }

    // Ensure destination directory exists
    fs::create_directories(dest_dir);

    for (auto &iter : fs::recursive_directory_iterator(src_dir))
    {
        if (iter.is_directory())
            continue;

        fs::path relative = fs::relative(iter.path(), src_dir);
        fs::path out_path = dest_dir / relative;
        out_path += ".zc";

        fs::create_directories(out_path.parent_path());

        if (!compress_file(iter.path(), out_path))
        {
            std::cerr << "Failed to compress file:\n"
                      << "  Source: " << iter.path() << "\n"
                      << "  Destination: " << out_path << std::endl;
            return false;
        }
    }

    return true;
}

bool decompress_dir(const fs::path &src_dir, const fs::path &dest_dir)
{
    if (!fs::exists(src_dir) || !fs::is_directory(src_dir))
    {
        std::cerr << "Given src_dir does not exist or is not a directory: " << src_dir << std::endl;
        return false;
    }

    fs::create_directories(dest_dir);

    for (auto &iter : fs::recursive_directory_iterator(src_dir))
    {
        if (iter.is_directory())
            continue;

        fs::path relative = fs::relative(iter.path(), src_dir);

        fs::path out_path = dest_dir / relative;
        if (out_path.extension() == ".zc")
            out_path.replace_extension();

        fs::create_directories(out_path.parent_path());

        if (!decompress_file(iter.path(), out_path))
        {
            std::cerr << "Failed to decompress file:\n"
                      << "  Source: " << iter.path() << "\n"
                      << "  Destination: " << out_path << std::endl;
            return false;
        }
    }

    return true;
}

std::string xxhash_file(const fs::path &src)
{
    std::ifstream src_file(src, std::ios::binary);
    if (!src_file)
    {
        std::cerr << "Fail while opening " << src << std::endl;
        return "";
    }

    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        std::cerr << "Error while creating XXH3_state_t!" << std::endl;
        return "";
    }

    if (XXH3_128bits_reset(state) != XXH_OK)
    {
        std::cerr << "Error while resetting hash state!" << std::endl;
        XXH3_freeState(state);
        return "";
    }

    uint8_t buffer[CHUNK_SIZE];
    while (true)
    {
        src_file.read(reinterpret_cast<char *>(buffer), CHUNK_SIZE);
        std::streamsize read_count = src_file.gcount();
        if (read_count <= 0)
            break;

        XXH3_128bits_update(state, buffer, static_cast<size_t>(read_count));

        if (src_file.eof())
            break;
    }

    XXH128_hash_t hash = XXH3_128bits_digest(state);
    XXH3_freeState(state);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << hash.high64
        << std::setw(16) << hash.low64;

    return oss.str();
}

std::string xxhash_bytes(const char* bytes, size_t len)
{
    if(len == 0){
        std::cerr << "Cant hash empty sequence! " << std::endl;
        return "";
    }
    XXH3_state_t *state = XXH3_createState();
    if (!state)
    {
        std::cerr << "Error while creating XXH3_state_t!" << std::endl;
        XXH3_freeState(state);
        return "";
    }
    if (XXH3_128bits_reset(state) != XXH_OK)
    {
        std::cerr << "Error while reset hash state!" << std::endl;
        XXH3_freeState(state);
        return "";
    }

    XXH3_128bits_update(state, bytes, len);

    XXH128_hash_t hash = XXH3_128bits_digest(state);

    XXH3_freeState(state);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << hash.high64
        << std::setw(16) << hash.low64;
    return oss.str();
}

inline void write_string(const std::string& str, std::ofstream& file){
    size_t len = str.size();
    file.write(reinterpret_cast<char*>(&len), sizeof(size_t));
    file.write(str.data(), len);
}
inline void read_string(std::string& str, std::ifstream& file){
    size_t len;
    file.read(reinterpret_cast<char*>(&len), sizeof(size_t));
    str.resize(len);
    file.read(str.data(), len);
}

template<typename T>
inline void read(T& var, std::ifstream& file){
    file.read(reinterpret_cast<char*>(var), sizeof(T));
}