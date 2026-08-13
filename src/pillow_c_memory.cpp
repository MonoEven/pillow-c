#include "pillow_c_internal.h"

#include <cstdio>
#include <new>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

std::uint16_t read_le16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint32_t read_le32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::int32_t read_le_i32(const std::uint8_t* data)
{
    return static_cast<std::int32_t>(read_le32(data));
}

std::uint32_t read_be32(const std::uint8_t* data)
{
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::uint16_t read_be16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                      static_cast<std::uint16_t>(data[1]));
}

void append_le16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_be16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void append_le32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

void append_be32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

bool utf8_path_to_wide(const char* path, std::vector<wchar_t>* out)
{
    if (!path || !out) {
        return false;
    }
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    if (required <= 0) {
        return false;
    }
    out->assign(static_cast<std::size_t>(required), wchar_t{0});
    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out->data(), required);
    return written == required;
}

bool read_binary_file(const char* path, std::vector<std::uint8_t>* out)
{
    if (!out) {
        return false;
    }
    std::vector<wchar_t> wide_path;
    if (!utf8_path_to_wide(path, &wide_path)) {
        return false;
    }
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, wide_path.data(), L"rb") != 0 || !file) {
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long length = std::ftell(file);
    if (length < 0) {
        std::fclose(file);
        return false;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    try {
        out->assign(static_cast<std::size_t>(length), std::uint8_t{0});
    } catch (const std::bad_alloc&) {
        std::fclose(file);
        throw;
    }
    if (!out->empty()) {
        const std::size_t read_count = std::fread(out->data(), 1, out->size(), file);
        if (read_count != out->size()) {
            std::fclose(file);
            return false;
        }
    }
    std::fclose(file);
    return true;
}

bool write_binary_file(const char* path, const std::vector<std::uint8_t>& data)
{
    std::vector<wchar_t> wide_path;
    if (!utf8_path_to_wide(path, &wide_path)) {
        return false;
    }
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, wide_path.data(), L"wb") != 0 || !file) {
        return false;
    }
    if (!data.empty()) {
        const std::size_t written = std::fwrite(data.data(), 1, data.size(), file);
        if (written != data.size()) {
            std::fclose(file);
            return false;
        }
    }
    const bool ok = std::fclose(file) == 0;
    return ok;
}

