#include "memory_mapped_file.h"

#if defined(__SWITCH__)
#   include <fstream>
#elif !defined(_WIN32)
#   include <cerrno>
#   include <cstring>
#   include <cstdio>
#   include <fcntl.h>
#   include <unistd.h>
#endif

MemoryMappedFile::MemoryMappedFile()
{
    // Default constructor.
}

MemoryMappedFile::MemoryMappedFile(const std::filesystem::path &path)
{
    open(path);
}

MemoryMappedFile::~MemoryMappedFile()
{
    close();
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile &&other)
{
#if defined(_WIN32)
    fileHandle = other.fileHandle;
    fileMappingHandle = other.fileMappingHandle;
    fileView = other.fileView;
    fileSize = other.fileSize;

    other.fileHandle = nullptr;
    other.fileMappingHandle = nullptr;
    other.fileView = nullptr;
    other.fileSize.QuadPart = 0;
#elif defined(__SWITCH__)
    fileData = std::move(other.fileData);
    opened = other.opened;

    other.opened = false;
#else
    fileHandle = other.fileHandle;
    fileView = other.fileView;
    fileSize = other.fileSize;

    other.fileHandle = -1;
    other.fileView = MAP_FAILED;
    other.fileSize = 0;
#endif
}

bool MemoryMappedFile::open(const std::filesystem::path &path)
{
#if defined(_WIN32)
    fileHandle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "CreateFileW failed with error %lu.\n", GetLastError());
        fileHandle = nullptr;
        return false;
    }

    if (!GetFileSizeEx(fileHandle, &fileSize))
    {
        fprintf(stderr, "GetFileSizeEx failed with error %lu.\n", GetLastError());
        CloseHandle(fileHandle);
        fileHandle = nullptr;
        return false;
    }

    fileMappingHandle = CreateFileMappingW(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (fileMappingHandle == nullptr)
    {
        fprintf(stderr, "CreateFileMappingW failed with error %lu.\n", GetLastError());
        CloseHandle(fileHandle);
        fileHandle = nullptr;
        return false;
    }

    fileView = MapViewOfFile(fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (fileView == nullptr)
    {
        fprintf(stderr, "MapViewOfFile failed with error %lu.\n", GetLastError());
        CloseHandle(fileMappingHandle);
        CloseHandle(fileHandle);
        fileMappingHandle = nullptr;
        fileHandle = nullptr;
        return false;
    }

    return true;
#elif defined(__SWITCH__)
    close();

    const auto pathU8 = path.u8string();
    const std::string pathString(reinterpret_cast<const char*>(pathU8.c_str()));
    std::ifstream file(pathString, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        fprintf(stderr, "open for %s failed.\n", pathString.c_str());
        return false;
    }

    const auto endPosition = file.tellg();
    if (endPosition < 0)
    {
        fprintf(stderr, "tellg failed for %s.\n", pathString.c_str());
        return false;
    }

    fileData.resize(static_cast<size_t>(endPosition));
    file.seekg(0, std::ios::beg);

    if (!fileData.empty())
    {
        file.read(reinterpret_cast<char*>(fileData.data()), fileData.size());
        if (!file)
        {
            fprintf(stderr, "read for %s failed.\n", pathString.c_str());
            fileData.clear();
            return false;
        }
    }

    opened = true;
    return true;
#else
    fileHandle = ::open(path.c_str(), O_RDONLY);
    if (fileHandle == -1)
    {
        fprintf(stderr, "open for %s failed with error %s.\n", path.c_str(), strerror(errno));
        return false;
    }

    fileSize = lseek(fileHandle, 0, SEEK_END);
    if (fileSize == (off_t)(-1))
    {
        fprintf(stderr, "lseek failed with error %s.\n", strerror(errno));
        ::close(fileHandle);
        fileHandle = -1;
        return false;
    }

    fileView = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fileHandle, 0);
    if (fileView == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed with error %s.\n", strerror(errno));
        ::close(fileHandle);
        fileHandle = -1;
        return false;
    }

    return true;
#endif
}

void MemoryMappedFile::close()
{
#if defined(_WIN32)
    if (fileView != nullptr)
    {
        UnmapViewOfFile(fileView);
    }

    if (fileMappingHandle != nullptr)
    {
        CloseHandle(fileMappingHandle);
    }

    if (fileHandle != nullptr)
    {
        CloseHandle(fileHandle);
    }
#elif defined(__SWITCH__)
    fileData.clear();
    opened = false;
#else
    if (fileView != MAP_FAILED)
    {
        munmap(fileView, fileSize);
    }

    if (fileHandle != -1)
    {
        ::close(fileHandle);
    }
#endif
}

bool MemoryMappedFile::isOpen() const
{
#if defined(_WIN32)
    return (fileView != nullptr);
#elif defined(__SWITCH__)
    return opened;
#else
    return (fileView != MAP_FAILED);
#endif
}

uint8_t *MemoryMappedFile::data() const
{
#if defined(__SWITCH__)
    return const_cast<uint8_t*>(fileData.data());
#else
    return reinterpret_cast<uint8_t *>(fileView);
#endif
}

size_t MemoryMappedFile::size() const
{
#if defined(_WIN32)
    return fileSize.QuadPart;
#elif defined(__SWITCH__)
    return fileData.size();
#else
    return static_cast<size_t>(fileSize);
#endif
}
