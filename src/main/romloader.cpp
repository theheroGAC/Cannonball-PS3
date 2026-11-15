#include <cstdio>
#include <fstream>
#include <cstring>
#include <unordered_map>
#include <sys/stat.h>
#include <dirent.h>
#include <zlib.h>

#include "stdint.hpp"
#include "romloader.hpp"
#include "frontend/config.hpp"
#include "../../ps3/ps3_platform.hpp"

static std::unordered_map<int, std::string> map;
static bool map_created;

RomLoader::RomLoader()
{
    loaded = false;
    map_created = false;
    rom = nullptr;
}

RomLoader::~RomLoader()
{
    unload();
}

void RomLoader::init(uint32_t length)
{
    this->length = length;
    rom = new uint8_t[length];
    memset(rom, 0, length);

    load = config.data.crc32 ? &RomLoader::load_crc32 : &RomLoader::load_rom;
}

void RomLoader::unload(void)
{
    if (rom != nullptr)
    {
        delete[] rom;
        rom = nullptr;
    }
    loaded = false;
}

int RomLoader::load_rom(const char* filename, const int offset, const int length, const int expected_crc, const uint8_t interleave, const bool verbose)
{
    if (ps3_roms_load_by_crc(static_cast<uint32_t>(expected_crc), filename, length, rom, interleave, offset))
    {
        loaded = true;
        return 0;
    }

    std::string path = config.data.rom_path + std::string(filename);
    std::ifstream src(path, std::ios::in | std::ios::binary);
    if (!src)
    {
        if (verbose) fprintf(stderr, "cannot open rom: %s\n", path.c_str());
        loaded = false;
        return 1;
    }

    char* buffer = new char[length];
    src.read(buffer, length);

    uLong c = crc32(0L, reinterpret_cast<const Bytef*>(buffer), static_cast<uInt>(length));

    if (expected_crc != static_cast<int>(c))
    {
        if (verbose) fprintf(stderr, "%s has incorrect checksum\n", filename);
        delete[] buffer;
        src.close();
        loaded = false;
        return 1;
    }

    for (int i = 0; i < length; i++)
        rom[(i * interleave) + offset] = static_cast<uint8_t>(buffer[i]);

    delete[] buffer;
    src.close();
    loaded = true;
    return 0;
}

int RomLoader::create_map()
{
    map_created = true;
    DIR* dir;
    struct dirent* ent;
    struct stat st;

    dir = opendir(config.data.rom_path.c_str());

    if (dir == nullptr)
        return 1;

    while ((ent = readdir(dir)) != nullptr)
    {
        const std::string file_name = ent->d_name;
        const std::string full_file_name = config.data.rom_path + "/" + file_name;

        if (file_name[0] == '.')
            continue;

        if (stat(full_file_name.c_str(), &st) == -1)
            continue;

        if (S_ISDIR(st.st_mode))
            continue;

        if (st.st_size > 0x200000)
            continue;

        std::ifstream src(full_file_name, std::ios::in | std::ios::binary);
        if (!src)
            continue;

        int flen = static_cast<int>(st.st_size);
        char* buffer = new char[flen];
        src.read(buffer, flen);
        uLong c = crc32(0L, reinterpret_cast<const Bytef*>(buffer), static_cast<uInt>(flen));
        map.insert(std::pair<int, std::string>(static_cast<int>(c), full_file_name));

        delete[] buffer;
        src.close();
    }

    closedir(dir);
    return 0;
}

int RomLoader::load_crc32(const char* debug, const int offset, const int length, const int expected_crc, const uint8_t interleave, const bool verbose)
{
    if (ps3_roms_load_by_crc(static_cast<uint32_t>(expected_crc), debug, length, rom, interleave, offset))
    {
        loaded = true;
        return 0;
    }

    if (!map_created)
        create_map();

    if (map.empty())
        return 1;

    auto search = map.find(expected_crc);

    if (search == map.end())
    {
        if (verbose) fprintf(stderr, "Unable to locate rom in path: %s possible name: %s crc32: 0x%x\n", config.data.rom_path.c_str(), debug, static_cast<unsigned int>(expected_crc));
        loaded = false;
        return 1;
    }

    std::string file = search->second;

    std::ifstream src(file, std::ios::in | std::ios::binary);
    if (!src)
    {
        if (verbose) fprintf(stderr, "cannot open rom: %s\n", file.c_str());
        loaded = false;
        return 1;
    }

    char* buffer = new char[length];
    src.read(buffer, length);

    for (int i = 0; i < length; i++)
        rom[(i * interleave) + offset] = static_cast<uint8_t>(buffer[i]);

    delete[] buffer;
    src.close();
    loaded = true;
    return 0;
}

int RomLoader::load_binary(const char* filename)
{
    std::ifstream src(filename, std::ios::in | std::ios::binary);
    if (!src)
    {
        fprintf(stderr, "cannot open file: %s\n", filename);
        loaded = false;
        return 1;
    }

    length = filesize(filename);

    char* buffer = new char[length];
    src.read(buffer, length);
    rom = reinterpret_cast<uint8_t*>(buffer);

    src.close();

    loaded = true;
    return 0;
}

int RomLoader::filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::in | std::ifstream::binary);
    in.seekg(0, std::ifstream::end);
    int size = static_cast<int>(in.tellg());
    in.close();
    return size;
}
