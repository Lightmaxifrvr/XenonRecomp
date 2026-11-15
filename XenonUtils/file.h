#pragma once

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <vector>

inline std::vector<uint8_t> LoadFile(const std::filesystem::path& path)
{
#if defined(__SWITCH__)
    FILE* file = fopen(path.string().c_str(), "rb");
    if (file == nullptr)
        return {};

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return {};
    }

    const long fileSize = ftell(file);
    if (fileSize < 0)
    {
        fclose(file);
        return {};
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return {};
    }

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    size_t offset = 0;

    while (offset < data.size())
    {
        const size_t bytesRead = fread(data.data() + offset, 1, data.size() - offset, file);
        if (bytesRead == 0)
        {
            if (ferror(file))
            {
                fclose(file);
                return {};
            }

            break;
        }

        offset += bytesRead;
    }

    fclose(file);

    if (offset != data.size())
        return {};

    return data;
#else
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        return {};
    }

    stream.seekg(0, std::ios::end);
    std::streampos size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> data;
    data.resize(size);
    stream.read((char *)(data.data()), size);
    if (stream.bad())
    {
        return {};
    }

    return data;
#endif
}
