#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "pillow_c_internal.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include "pillow_c_wic_internal.h"

namespace {

constexpr std::size_t PNG_MAX_TEXT_CHUNK = 1024u * 1024u;

struct PngHeaderInfo {
    int bit_depth;
    int color_type;
};

bool read_png_header_info(const char* path, PngHeaderInfo* info)
{
    if (!path || !info) {
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
    std::uint8_t header[33] = {};
    const std::size_t read_count = std::fread(header, 1, sizeof(header), file);
    std::fclose(file);
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (read_count != sizeof(header) || std::memcmp(header, signature, sizeof(signature)) != 0 ||
        read_be32(header + 8) != 13u || std::memcmp(header + 12, "IHDR", 4) != 0) {
        return false;
    }
    info->bit_depth = header[24];
    info->color_type = header[25];
    return true;
}

bool read_png_dpi_metadata(const char* path, double* out_dpi_x, double* out_dpi_y)
{
    if (!path || !out_dpi_x || !out_dpi_y) {
        return false;
    }
    *out_dpi_x = 0.0;
    *out_dpi_y = 0.0;
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (length == 9u && std::memcmp(type, "pHYs", 4u) == 0) {
            if (payload[8] != 1u) {
                return false;
            }
            *out_dpi_x = static_cast<double>(read_be32(payload)) * 0.0254;
            *out_dpi_y = static_cast<double>(read_be32(payload + 4u)) * 0.0254;
            return true;
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_gamma_metadata(const char* path, double* out_gamma)
{
    if (!path || !out_gamma) {
        return false;
    }
    *out_gamma = 0.0;
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (length == 4u && std::memcmp(type, "gAMA", 4u) == 0) {
            *out_gamma = static_cast<double>(read_be32(payload)) / 100000.0;
            return true;
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_srgb_metadata(const char* path, int* out_srgb)
{
    if (!path || !out_srgb) {
        return false;
    }
    *out_srgb = 0;
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (length == 1u && std::memcmp(type, "sRGB", 4u) == 0) {
            *out_srgb = static_cast<int>(payload[0]);
            return true;
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_chromaticity_metadata(const char* path, double* out_values, std::size_t value_count)
{
    if (!path || !out_values || value_count < 8u) {
        return false;
    }
    for (std::size_t i = 0; i < 8u; ++i) {
        out_values[i] = 0.0;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (length == 32u && std::memcmp(type, "cHRM", 4u) == 0) {
            for (std::size_t i = 0; i < 8u; ++i) {
                out_values[i] = static_cast<double>(read_be32(payload + i * 4u)) / 100000.0;
            }
            return true;
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

struct DeflateBitReader {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t byte_pos = 0;
    std::uint32_t bit_buffer = 0;
    int bit_count = 0;

    bool read_bits(int count, std::uint32_t* out_value)
    {
        if (!out_value || count < 0 || count > 16) {
            return false;
        }
        while (bit_count < count) {
            if (byte_pos >= size) {
                return false;
            }
            bit_buffer |= static_cast<std::uint32_t>(data[byte_pos++]) << bit_count;
            bit_count += 8;
        }
        const std::uint32_t mask = count == 32 ? 0xffffffffu : ((1u << count) - 1u);
        *out_value = bit_buffer & mask;
        bit_buffer >>= count;
        bit_count -= count;
        return true;
    }

    void align_to_byte()
    {
        bit_buffer = 0;
        bit_count = 0;
    }
};

struct DeflateHuffmanEntry {
    std::uint16_t symbol = 0;
    std::uint8_t bit_length = 0;
};

std::uint32_t adler32_bytes(const std::uint8_t* data, std::size_t size)
{
    constexpr std::uint32_t mod = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % mod;
        b = (b + a) % mod;
    }
    return (b << 16) | a;
}

std::uint16_t reverse_bits(std::uint16_t value, int bit_count)
{
    std::uint16_t reversed = 0;
    for (int i = 0; i < bit_count; ++i) {
        reversed = static_cast<std::uint16_t>((reversed << 1) | (value & 1u));
        value = static_cast<std::uint16_t>(value >> 1);
    }
    return reversed;
}

bool build_deflate_huffman_table(
    const std::vector<std::uint8_t>& lengths,
    int max_bits,
    std::vector<DeflateHuffmanEntry>* out_table)
{
    if (!out_table || max_bits <= 0 || max_bits > 15) {
        return false;
    }
    const std::size_t table_size = static_cast<std::size_t>(1ull << max_bits);
    out_table->assign(table_size, {});
    int bl_count[16] = {};
    for (std::uint8_t length : lengths) {
        if (length > max_bits) {
            return false;
        }
        if (length != 0u) {
            ++bl_count[length];
        }
    }
    int code = 0;
    int next_code[16] = {};
    for (int bits = 1; bits <= max_bits; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        const int length = lengths[symbol];
        if (length == 0) {
            continue;
        }
        const std::uint16_t reversed = reverse_bits(static_cast<std::uint16_t>(next_code[length]), length);
        ++next_code[length];
        const std::size_t fill_step = static_cast<std::size_t>(1ull << length);
        for (std::size_t index = reversed; index < table_size; index += fill_step) {
            (*out_table)[index].symbol = static_cast<std::uint16_t>(symbol);
            (*out_table)[index].bit_length = static_cast<std::uint8_t>(length);
        }
    }
    return true;
}

bool read_deflate_symbol(
    DeflateBitReader* reader,
    const std::vector<DeflateHuffmanEntry>& table,
    int max_bits,
    std::uint16_t* out_symbol)
{
    if (!reader || !out_symbol || table.empty() || max_bits <= 0 || max_bits > 15) {
        return false;
    }
    while (reader->bit_count < max_bits && reader->byte_pos < reader->size) {
        reader->bit_buffer |= static_cast<std::uint32_t>(reader->data[reader->byte_pos++]) << reader->bit_count;
        reader->bit_count += 8;
    }
    if (reader->bit_count == 0) {
        return false;
    }
    const DeflateHuffmanEntry entry = table[reader->bit_buffer & ((1u << max_bits) - 1u)];
    if (entry.bit_length == 0 || reader->bit_count < entry.bit_length) {
        return false;
    }
    reader->bit_buffer >>= entry.bit_length;
    reader->bit_count -= entry.bit_length;
    *out_symbol = entry.symbol;
    return true;
}

bool inflate_deflate_huffman_block(
    DeflateBitReader* reader,
    const std::vector<DeflateHuffmanEntry>& literal_table,
    int literal_max_bits,
    const std::vector<DeflateHuffmanEntry>& distance_table,
    int distance_max_bits,
    std::vector<std::uint8_t>* out,
    std::size_t expected_max,
    bool* out_exceeded_max)
{
    if (!reader || !out) {
        return false;
    }
    auto exceeds_max = [&](std::size_t add_size) -> bool {
        if (out->size() > expected_max || add_size > expected_max - out->size()) {
            if (out_exceeded_max) {
                *out_exceeded_max = true;
            }
            return true;
        }
        return false;
    };
    static constexpr int length_base[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
    };
    static constexpr int length_extra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
    };
    static constexpr int distance_base[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
        193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
        6145, 8193, 12289, 16385, 24577,
    };
    static constexpr int distance_extra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
    };

    for (;;) {
        std::uint16_t symbol = 0;
        if (!read_deflate_symbol(reader, literal_table, literal_max_bits, &symbol)) {
            return false;
        }
        if (symbol < 256u) {
            if (exceeds_max(1u)) {
                return false;
            }
            out->push_back(static_cast<std::uint8_t>(symbol));
        } else if (symbol == 256u) {
            return true;
        } else if (symbol <= 285u) {
            const int length_index = static_cast<int>(symbol - 257u);
            int match_length = length_base[length_index];
            if (length_extra[length_index] > 0) {
                std::uint32_t extra = 0;
                if (!reader->read_bits(length_extra[length_index], &extra)) {
                    return false;
                }
                match_length += static_cast<int>(extra);
            }
            std::uint16_t distance_symbol = 0;
            if (!read_deflate_symbol(reader, distance_table, distance_max_bits, &distance_symbol) ||
                distance_symbol >= 30u) {
                return false;
            }
            int distance = distance_base[distance_symbol];
            if (distance_extra[distance_symbol] > 0) {
                std::uint32_t extra = 0;
                if (!reader->read_bits(distance_extra[distance_symbol], &extra)) {
                    return false;
                }
                distance += static_cast<int>(extra);
            }
            if (distance <= 0 || static_cast<std::size_t>(distance) > out->size()) {
                return false;
            }
            if (exceeds_max(static_cast<std::size_t>(match_length))) {
                return false;
            }
            for (int i = 0; i < match_length; ++i) {
                out->push_back((*out)[out->size() - static_cast<std::size_t>(distance)]);
            }
        } else {
            return false;
        }
    }
}

bool read_dynamic_deflate_tables(
    DeflateBitReader* reader,
    std::vector<DeflateHuffmanEntry>* out_literal_table,
    std::vector<DeflateHuffmanEntry>* out_distance_table)
{
    if (!reader || !out_literal_table || !out_distance_table) {
        return false;
    }
    std::uint32_t hlit_bits = 0;
    std::uint32_t hdist_bits = 0;
    std::uint32_t hclen_bits = 0;
    if (!reader->read_bits(5, &hlit_bits) ||
        !reader->read_bits(5, &hdist_bits) ||
        !reader->read_bits(4, &hclen_bits)) {
        return false;
    }
    const std::size_t literal_count = static_cast<std::size_t>(hlit_bits) + 257u;
    const std::size_t distance_count = static_cast<std::size_t>(hdist_bits) + 1u;
    const std::size_t code_length_count = static_cast<std::size_t>(hclen_bits) + 4u;
    if (literal_count > 286u || distance_count > 32u || code_length_count > 19u) {
        return false;
    }

    static constexpr std::uint8_t code_length_order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
    };
    std::vector<std::uint8_t> code_length_lengths(19, 0);
    for (std::size_t i = 0; i < code_length_count; ++i) {
        std::uint32_t length = 0;
        if (!reader->read_bits(3, &length)) {
            return false;
        }
        code_length_lengths[code_length_order[i]] = static_cast<std::uint8_t>(length);
    }
    std::vector<DeflateHuffmanEntry> code_length_table;
    if (!build_deflate_huffman_table(code_length_lengths, 7, &code_length_table)) {
        return false;
    }

    const std::size_t total_lengths = literal_count + distance_count;
    std::vector<std::uint8_t> lengths;
    lengths.reserve(total_lengths);
    while (lengths.size() < total_lengths) {
        std::uint16_t symbol = 0;
        if (!read_deflate_symbol(reader, code_length_table, 7, &symbol)) {
            return false;
        }
        if (symbol <= 15u) {
            lengths.push_back(static_cast<std::uint8_t>(symbol));
        } else if (symbol == 16u) {
            if (lengths.empty()) {
                return false;
            }
            std::uint32_t extra = 0;
            if (!reader->read_bits(2, &extra)) {
                return false;
            }
            const std::size_t repeat = static_cast<std::size_t>(extra) + 3u;
            if (lengths.size() + repeat > total_lengths) {
                return false;
            }
            const std::uint8_t previous = lengths.back();
            for (std::size_t i = 0; i < repeat; ++i) {
                lengths.push_back(previous);
            }
        } else if (symbol == 17u) {
            std::uint32_t extra = 0;
            if (!reader->read_bits(3, &extra)) {
                return false;
            }
            const std::size_t repeat = static_cast<std::size_t>(extra) + 3u;
            if (lengths.size() + repeat > total_lengths) {
                return false;
            }
            lengths.insert(lengths.end(), repeat, 0);
        } else if (symbol == 18u) {
            std::uint32_t extra = 0;
            if (!reader->read_bits(7, &extra)) {
                return false;
            }
            const std::size_t repeat = static_cast<std::size_t>(extra) + 11u;
            if (lengths.size() + repeat > total_lengths) {
                return false;
            }
            lengths.insert(lengths.end(), repeat, 0);
        } else {
            return false;
        }
    }

    std::vector<std::uint8_t> literal_lengths(lengths.begin(), lengths.begin() + static_cast<std::ptrdiff_t>(literal_count));
    std::vector<std::uint8_t> distance_lengths(lengths.begin() + static_cast<std::ptrdiff_t>(literal_count), lengths.end());
    if (literal_lengths.size() <= 256u || literal_lengths[256] == 0u) {
        return false;
    }
    return build_deflate_huffman_table(literal_lengths, 15, out_literal_table) &&
        build_deflate_huffman_table(distance_lengths, 15, out_distance_table);
}

bool inflate_zlib_deflate(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<std::uint8_t>* out,
    std::size_t expected_max,
    bool* out_exceeded_max = nullptr)
{
    if (out_exceeded_max) {
        *out_exceeded_max = false;
    }
    if (!data || !out || size < 6u) {
        return false;
    }
    out->clear();
    const std::uint8_t cmf = data[0];
    const std::uint8_t flg = data[1];
    if ((cmf & 0x0fu) != 8u || (static_cast<unsigned>(cmf) * 256u + flg) % 31u != 0u || (flg & 0x20u) != 0u) {
        return false;
    }
    const std::size_t deflate_size = size - 6u;
    const std::uint32_t expected_adler = read_be32(data + size - 4u);
    DeflateBitReader reader;
    reader.data = data + 2u;
    reader.size = deflate_size;

    std::vector<std::uint8_t> literal_lengths(288, 0);
    for (int i = 0; i <= 143; ++i) {
        literal_lengths[static_cast<std::size_t>(i)] = 8;
    }
    for (int i = 144; i <= 255; ++i) {
        literal_lengths[static_cast<std::size_t>(i)] = 9;
    }
    for (int i = 256; i <= 279; ++i) {
        literal_lengths[static_cast<std::size_t>(i)] = 7;
    }
    for (int i = 280; i <= 287; ++i) {
        literal_lengths[static_cast<std::size_t>(i)] = 8;
    }
    std::vector<std::uint8_t> distance_lengths(32, 5);
    std::vector<DeflateHuffmanEntry> literal_table;
    std::vector<DeflateHuffmanEntry> distance_table;
    if (!build_deflate_huffman_table(literal_lengths, 9, &literal_table) ||
        !build_deflate_huffman_table(distance_lengths, 5, &distance_table)) {
        return false;
    }

    bool final_block = false;
    while (!final_block) {
        std::uint32_t final_bit = 0;
        std::uint32_t block_type = 0;
        if (!reader.read_bits(1, &final_bit) || !reader.read_bits(2, &block_type)) {
            return false;
        }
        final_block = final_bit != 0u;
        if (block_type == 0u) {
            reader.align_to_byte();
            if (reader.byte_pos + 4u > reader.size) {
                return false;
            }
            const std::uint16_t len = static_cast<std::uint16_t>(reader.data[reader.byte_pos] | (reader.data[reader.byte_pos + 1u] << 8));
            const std::uint16_t nlen = static_cast<std::uint16_t>(reader.data[reader.byte_pos + 2u] | (reader.data[reader.byte_pos + 3u] << 8));
            reader.byte_pos += 4u;
            if (static_cast<std::uint16_t>(~len) != nlen || reader.byte_pos + len > reader.size) {
                return false;
            }
            if (out->size() > expected_max || static_cast<std::size_t>(len) > expected_max - out->size()) {
                if (out_exceeded_max) {
                    *out_exceeded_max = true;
                }
                return false;
            }
            out->insert(out->end(), reader.data + reader.byte_pos, reader.data + reader.byte_pos + len);
            reader.byte_pos += len;
        } else if (block_type == 1u) {
            if (!inflate_deflate_huffman_block(
                    &reader,
                    literal_table,
                    9,
                    distance_table,
                    5,
                    out,
                    expected_max,
                    out_exceeded_max)) {
                return false;
            }
        } else if (block_type == 2u) {
            std::vector<DeflateHuffmanEntry> dynamic_literal_table;
            std::vector<DeflateHuffmanEntry> dynamic_distance_table;
            if (!read_dynamic_deflate_tables(&reader, &dynamic_literal_table, &dynamic_distance_table) ||
                !inflate_deflate_huffman_block(
                    &reader,
                    dynamic_literal_table,
                    15,
                    dynamic_distance_table,
                    15,
                    out,
                    expected_max,
                    out_exceeded_max)) {
                return false;
            }
        } else {
            return false;
        }
    }
    return adler32_bytes(out->empty() ? nullptr : out->data(), out->size()) == expected_adler;
}

std::string latin1_to_utf8(const std::uint8_t* data, std::size_t size)
{
    std::string out;
    out.reserve(size * 2u);
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint8_t ch = data[i];
        if (ch < 0x80u) {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back(static_cast<char>(0xC0u | (ch >> 6)));
            out.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
        }
    }
    return out;
}

bool read_png_text_metadata(const char* path, std::vector<std::pair<std::string, std::string>>* out_text)
{
    if (!path || !out_text) {
        return false;
    }
    out_text->clear();
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            out_text->clear();
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "tEXt", 4u) == 0) {
            const std::uint8_t* end = payload + length;
            const std::uint8_t* separator = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (separator && separator != payload) {
                out_text->push_back({
                    latin1_to_utf8(payload, static_cast<std::size_t>(separator - payload)),
                    latin1_to_utf8(separator + 1, static_cast<std::size_t>(end - (separator + 1))),
                });
            }
        } else if (std::memcmp(type, "zTXt", 4u) == 0) {
            const std::uint8_t* end = payload + length;
            const std::uint8_t* separator = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (separator && separator != payload && separator + 2u <= end && separator[1] == 0u) {
                std::vector<std::uint8_t> value_bytes;
                if (inflate_zlib_deflate(
                        separator + 2u,
                        static_cast<std::size_t>(end - (separator + 2u)),
                        &value_bytes,
                        PNG_MAX_TEXT_CHUNK)) {
                    out_text->push_back({
                        latin1_to_utf8(payload, static_cast<std::size_t>(separator - payload)),
                        latin1_to_utf8(value_bytes.data(), value_bytes.size()),
                    });
                }
            }
        } else if (std::memcmp(type, "iTXt", 4u) == 0) {
            const std::uint8_t* end = payload + length;
            const std::uint8_t* key_end = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (key_end && key_end != payload && key_end + 5u <= end) {
                const std::uint8_t compression_flag = key_end[1];
                const std::uint8_t compression_method = key_end[2];
                if ((compression_flag == 0u || compression_flag == 1u) && compression_method == 0u) {
                    const std::uint8_t* language = key_end + 3u;
                    const std::uint8_t* language_end = static_cast<const std::uint8_t*>(
                        std::memchr(language, 0, static_cast<std::size_t>(end - language)));
                    if (language_end) {
                        const std::uint8_t* translated = language_end + 1u;
                        const std::uint8_t* translated_end = static_cast<const std::uint8_t*>(
                            std::memchr(translated, 0, static_cast<std::size_t>(end - translated)));
                        if (translated_end) {
                            const std::uint8_t* value = translated_end + 1u;
                            std::string value_text;
                            if (compression_flag == 0u) {
                                value_text.assign(
                                    reinterpret_cast<const char*>(value),
                                    static_cast<std::size_t>(end - value));
                            } else if (compression_flag == 1u) {
                                std::vector<std::uint8_t> value_bytes;
                                if (!inflate_zlib_deflate(
                                        value,
                                        static_cast<std::size_t>(end - value),
                                        &value_bytes,
                                        PNG_MAX_TEXT_CHUNK)) {
                                    continue;
                                }
                                value_text.assign(
                                    reinterpret_cast<const char*>(value_bytes.data()),
                                    value_bytes.size());
                            } else {
                                continue;
                            }
                            out_text->push_back({
                                latin1_to_utf8(payload, static_cast<std::size_t>(key_end - payload)),
                                value_text,
                            });
                        }
                    }
                }
            }
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return !out_text->empty();
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return !out_text->empty();
}

bool read_png_xmp_metadata(const char* path, std::vector<std::uint8_t>* out_xmp)
{
    if (!path || !out_xmp) {
        return false;
    }
    out_xmp->clear();
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    static constexpr char xmp_key[] = "XML:com.adobe.xmp";
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            out_xmp->clear();
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "iTXt", 4u) == 0) {
            const std::uint8_t* end = payload + length;
            const std::uint8_t* key_end = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (key_end && key_end != payload &&
                static_cast<std::size_t>(key_end - payload) == sizeof(xmp_key) - 1u &&
                std::memcmp(payload, xmp_key, sizeof(xmp_key) - 1u) == 0 &&
                key_end + 5u <= end) {
                const std::uint8_t compression_flag = key_end[1];
                const std::uint8_t compression_method = key_end[2];
                if ((compression_flag == 0u || compression_flag == 1u) && compression_method == 0u) {
                    const std::uint8_t* language = key_end + 3u;
                    const std::uint8_t* language_end = static_cast<const std::uint8_t*>(
                        std::memchr(language, 0, static_cast<std::size_t>(end - language)));
                    if (language_end) {
                        const std::uint8_t* translated = language_end + 1u;
                        const std::uint8_t* translated_end = static_cast<const std::uint8_t*>(
                            std::memchr(translated, 0, static_cast<std::size_t>(end - translated)));
                        if (translated_end) {
                            const std::uint8_t* value = translated_end + 1u;
                            if (compression_flag == 0u) {
                                out_xmp->assign(value, end);
                                return !out_xmp->empty();
                            }
                            std::vector<std::uint8_t> value_bytes;
                            if (inflate_zlib_deflate(
                                    value,
                                    static_cast<std::size_t>(end - value),
                                    &value_bytes,
                                    PNG_MAX_TEXT_CHUNK) &&
                                !value_bytes.empty()) {
                                *out_xmp = std::move(value_bytes);
                                return true;
                            }
                        }
                    }
                }
            }
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            break;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_icc_profile(const char* path, std::vector<std::uint8_t>* out_profile)
{
    if (!path || !out_profile) {
        return false;
    }
    out_profile->clear();
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            out_profile->clear();
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "iCCP", 4u) == 0) {
            const std::uint8_t* end = payload + length;
            const std::uint8_t* separator = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (separator && separator != payload && separator + 2u <= end && separator[1] == 0u) {
                std::vector<std::uint8_t> profile;
                if (inflate_zlib_deflate(
                        separator + 2u,
                        static_cast<std::size_t>(end - (separator + 2u)),
                        &profile,
                        PNG_MAX_TEXT_CHUNK) &&
                    !profile.empty()) {
                    *out_profile = std::move(profile);
                    return true;
                }
            }
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool png_compressed_payload_exceeds_text_chunk_limit(const std::uint8_t* data, std::size_t size)
{
    std::vector<std::uint8_t> ignored;
    bool exceeded = false;
    (void)inflate_zlib_deflate(data, size, &ignored, PNG_MAX_TEXT_CHUNK, &exceeded);
    return exceeded;
}

bool png_has_oversized_compressed_metadata(const char* path)
{
    if (!path) {
        return false;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        const std::uint8_t* end = payload + length;
        if (std::memcmp(type, "zTXt", 4u) == 0) {
            const std::uint8_t* separator = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (separator && separator != payload && separator + 2u <= end && separator[1] == 0u &&
                png_compressed_payload_exceeds_text_chunk_limit(
                    separator + 2u,
                    static_cast<std::size_t>(end - (separator + 2u)))) {
                return true;
            }
        } else if (std::memcmp(type, "iTXt", 4u) == 0) {
            const std::uint8_t* key_end = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (key_end && key_end != payload && key_end + 5u <= end &&
                key_end[1] == 1u && key_end[2] == 0u) {
                const std::uint8_t* language = key_end + 3u;
                const std::uint8_t* language_end = static_cast<const std::uint8_t*>(
                    std::memchr(language, 0, static_cast<std::size_t>(end - language)));
                if (language_end) {
                    const std::uint8_t* translated = language_end + 1u;
                    const std::uint8_t* translated_end = static_cast<const std::uint8_t*>(
                        std::memchr(translated, 0, static_cast<std::size_t>(end - translated)));
                    if (translated_end) {
                        const std::uint8_t* value = translated_end + 1u;
                        if (png_compressed_payload_exceeds_text_chunk_limit(
                                value,
                                static_cast<std::size_t>(end - value))) {
                            return true;
                        }
                    }
                }
            }
        } else if (std::memcmp(type, "iCCP", 4u) == 0) {
            const std::uint8_t* separator = static_cast<const std::uint8_t*>(
                std::memchr(payload, 0, length));
            if (separator && separator != payload && separator + 2u <= end && separator[1] == 0u &&
                png_compressed_payload_exceeds_text_chunk_limit(
                    separator + 2u,
                    static_cast<std::size_t>(end - (separator + 2u)))) {
                return true;
            }
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            break;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_exif_metadata(const char* path, std::vector<std::uint8_t>* out_exif)
{
    if (!path || !out_exif) {
        return false;
    }
    out_exif->clear();
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            out_exif->clear();
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "eXIf", 4u) == 0) {
            if (length == 0u) {
                return false;
            }
            static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
            try {
                out_exif->assign(exif_header, exif_header + sizeof(exif_header));
                out_exif->insert(out_exif->end(), payload, payload + length);
            } catch (const std::bad_alloc&) {
                out_exif->clear();
                throw;
            }
            return true;
        }
        if (std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_palette_transparency(
    const char* path,
    std::vector<std::uint8_t>* out_palette_rgb,
    std::vector<std::uint8_t>* out_palette_alpha,
    std::vector<std::uint8_t>* out_transparency_table,
    int* out_transparency)
{
    if (!path || !out_palette_rgb || !out_palette_alpha || !out_transparency_table || !out_transparency) {
        return false;
    }
    out_palette_rgb->clear();
    out_palette_alpha->clear();
    out_transparency_table->clear();
    *out_transparency = -1;
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }

    std::vector<std::uint8_t> palette_rgb;
    std::vector<std::uint8_t> transparency;
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "PLTE", 4u) == 0) {
            if (length == 0u || length % 3u != 0u || length > 768u) {
                return false;
            }
            palette_rgb.assign(payload, payload + length);
        } else if (std::memcmp(type, "tRNS", 4u) == 0) {
            if (length == 0u || length > 256u) {
                return false;
            }
            transparency.assign(payload, payload + length);
        } else if (std::memcmp(type, "IEND", 4u) == 0) {
            break;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    if (palette_rgb.empty() || transparency.empty()) {
        return false;
    }

    const std::size_t entries = palette_rgb.size() / 3u;
    out_palette_rgb->assign(palette_rgb.begin(), palette_rgb.end());
    out_palette_alpha->assign(entries, std::uint8_t{255});
    const std::size_t alpha_count = std::min(entries, transparency.size());
    for (std::size_t index = 0; index < alpha_count; ++index) {
        (*out_palette_alpha)[index] = transparency[index];
    }
    int zero_index = -1;
    int zero_count = 0;
    bool has_partial_alpha = false;
    for (std::size_t index = 0; index < alpha_count; ++index) {
        if (transparency[index] == 0u) {
            zero_index = static_cast<int>(index);
            ++zero_count;
        } else if (transparency[index] != 255u) {
            has_partial_alpha = true;
        }
    }
    if (zero_count == 1 && !has_partial_alpha) {
        *out_transparency = zero_index;
    } else {
        out_transparency_table->assign(transparency.begin(), transparency.begin() + static_cast<std::ptrdiff_t>(alpha_count));
    }
    return true;
}

bool read_png_rgb_transparency(const char* path, std::uint8_t out_rgb[3])
{
    if (!path || !out_rgb) {
        return false;
    }
    out_rgb[0] = 0;
    out_rgb[1] = 0;
    out_rgb[2] = 0;
    PngHeaderInfo header_info{};
    if (!read_png_header_info(path, &header_info) ||
        header_info.bit_depth != 8 ||
        header_info.color_type != 2) {
        return false;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "tRNS", 4u) == 0) {
            if (length != 6u) {
                return false;
            }
            for (int channel = 0; channel < 3; ++channel) {
                const std::uint16_t value = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(payload[channel * 2]) << 8) |
                    static_cast<std::uint16_t>(payload[channel * 2 + 1]));
                if (value > 255u) {
                    return false;
                }
                out_rgb[channel] = static_cast<std::uint8_t>(value);
            }
            return true;
        }
        if (std::memcmp(type, "IDAT", 4u) == 0 || std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_grayscale_transparency(const char* path, std::uint8_t* out_value)
{
    if (!path || !out_value) {
        return false;
    }
    *out_value = 0;
    PngHeaderInfo header_info{};
    if (!read_png_header_info(path, &header_info) ||
        header_info.bit_depth != 8 ||
        header_info.color_type != 0) {
        return false;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        if (std::memcmp(type, "tRNS", 4u) == 0) {
            if (length != 2u || payload[0] != 0u) {
                return false;
            }
            *out_value = payload[1];
            return true;
        }
        if (std::memcmp(type, "IDAT", 4u) == 0 || std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool png_chunk_is_compressed_text_for_wic(
    const std::uint8_t* type,
    const std::uint8_t* payload,
    std::uint32_t length)
{
    if (!type || !payload || length == 0u) {
        return false;
    }
    const std::uint8_t* end = payload + length;
    if (std::memcmp(type, "zTXt", 4u) == 0) {
        const std::uint8_t* separator = static_cast<const std::uint8_t*>(
            std::memchr(payload, 0, length));
        return separator && separator != payload && separator + 2u <= end && separator[1] == 0u;
    }
    if (std::memcmp(type, "iTXt", 4u) == 0) {
        const std::uint8_t* key_end = static_cast<const std::uint8_t*>(
            std::memchr(payload, 0, length));
        return key_end && key_end != payload && key_end + 3u <= end &&
            key_end[1] == 1u && key_end[2] == 0u;
    }
    return false;
}

bool copy_png_without_wic_sensitive_chunks(
    const char* path,
    bool remove_trns,
    bool remove_compressed_text,
    std::vector<std::uint8_t>* out_png)
{
    if (!path || !out_png) {
        return false;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < sizeof(signature) || std::memcmp(data.data(), signature, sizeof(signature)) != 0) {
        return false;
    }
    std::vector<std::uint8_t> copy;
    copy.insert(copy.end(), signature, signature + sizeof(signature));
    bool removed_chunk = false;
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= data.size()) {
        const std::uint32_t length = read_be32(data.data() + pos);
        if (length > data.size() - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data.data() + pos + 4u;
        const std::uint8_t* payload = data.data() + pos + 8u;
        const std::size_t chunk_size = 12u + static_cast<std::size_t>(length);
        if ((remove_trns && std::memcmp(type, "tRNS", 4u) == 0) ||
            (remove_compressed_text && png_chunk_is_compressed_text_for_wic(type, payload, length))) {
            removed_chunk = true;
        } else {
            copy.insert(
                copy.end(),
                data.begin() + static_cast<std::ptrdiff_t>(pos),
                data.begin() + static_cast<std::ptrdiff_t>(pos + chunk_size));
        }
        pos += chunk_size;
        if (std::memcmp(type, "IEND", 4u) == 0) {
            break;
        }
    }
    if (!removed_chunk) {
        return false;
    }
    *out_png = std::move(copy);
    return true;
}

bool copy_png_without_trns(const char* path, std::vector<std::uint8_t>* out_png)
{
    return copy_png_without_wic_sensitive_chunks(path, true, false, out_png);
}

int remap_png_rgba_to_palette_indices(
    const std::vector<std::uint8_t>& rgba,
    int width,
    int height,
    std::size_t rgba_stride,
    const std::vector<std::uint8_t>& palette_rgb,
    const std::vector<std::uint8_t>& palette_alpha,
    PillowCImage* image)
{
    if (!image || width <= 0 || height <= 0 || palette_rgb.empty() || palette_rgb.size() % 3u != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t entries = palette_rgb.size() / 3u;
    if (entries > 256u || rgba_stride < static_cast<std::size_t>(width) * 4u ||
        rgba.size() < rgba_stride * static_cast<std::size_t>(height)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src_row = rgba.data() + static_cast<std::size_t>(y) * rgba_stride;
        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* src = src_row + static_cast<std::size_t>(x) * 4u;
            int found = -1;
            for (std::size_t index = 0; index < entries; ++index) {
                const std::size_t rgb = index * 3u;
                const std::uint8_t alpha =
                    index < palette_alpha.size() ? palette_alpha[index] : std::uint8_t{255};
                if (palette_rgb[rgb] == src[0] && palette_rgb[rgb + 1u] == src[1] &&
                    palette_rgb[rgb + 2u] == src[2] && alpha == src[3]) {
                    found = static_cast<int>(index);
                    break;
                }
            }
            if (found < 0 && src[3] == 0u) {
                for (std::size_t index = 0; index < entries; ++index) {
                    const std::uint8_t alpha =
                        index < palette_alpha.size() ? palette_alpha[index] : std::uint8_t{255};
                    if (alpha == 0u) {
                        found = static_cast<int>(index);
                        break;
                    }
                }
            }
            if (found < 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            dst_row[x] = static_cast<std::uint8_t>(found);
        }
    }
    return PILLOW_C_OK;
}


int open_png_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        PngHeaderInfo header_info = {};
        if (!read_png_header_info(path, &header_info)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (png_has_oversized_compressed_metadata(path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        std::uint8_t png_grayscale_transparency = 0;
        const bool has_grayscale_transparency =
            header_info.color_type == 0 &&
            read_png_grayscale_transparency(path, &png_grayscale_transparency);
        std::vector<std::uint8_t> png_decode_bytes;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICStream> decoder_stream;
        HRESULT hr = S_OK;
        if (copy_png_without_wic_sensitive_chunks(path, has_grayscale_transparency, true, &png_decode_bytes)) {
            hr = factory->CreateStream(decoder_stream.put());
            if (FAILED(hr) || png_decode_bytes.size() > static_cast<std::size_t>(UINT_MAX)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = decoder_stream->InitializeFromMemory(
                png_decode_bytes.data(),
                static_cast<DWORD>(png_decode_bytes.size()));
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = factory->CreateDecoderFromStream(
                decoder_stream.get(),
                nullptr,
                WICDecodeMetadataCacheOnDemand,
                decoder.put());
        } else {
            hr = factory->CreateDecoderFromFilename(
                wide_path.data(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                decoder.put());
        }
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatPng)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT width_u = 0;
        UINT height_u = 0;
        hr = frame->GetSize(&width_u, &height_u);
        if (FAILED(hr) || width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        int mode = 0;
        int channels = 0;
        int decoded_channels = 0;
        WICPixelFormatGUID target_format = {};
        std::vector<std::uint8_t> png_palette_rgb;
        std::vector<std::uint8_t> png_palette_alpha;
        std::vector<std::uint8_t> png_transparency_table;
        int png_transparency = -1;
        const bool has_palette_transparency =
            header_info.color_type == 3 &&
            read_png_palette_transparency(
                path,
                &png_palette_rgb,
                &png_palette_alpha,
                &png_transparency_table,
                &png_transparency);
        std::uint8_t png_rgb_transparency[3] = {0, 0, 0};
        const bool has_rgb_transparency =
            header_info.color_type == 2 &&
            read_png_rgb_transparency(path, png_rgb_transparency);
        if (header_info.color_type == 0 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
            decoded_channels = 1;
            target_format = GUID_WICPixelFormat8bppGray;
        } else if (header_info.color_type == 4 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_LA;
            channels = 2;
            decoded_channels = 4;
            target_format = GUID_WICPixelFormat32bppRGBA;
        } else if (header_info.color_type == 2 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
            decoded_channels = 3;
            target_format = GUID_WICPixelFormat24bppRGB;
        } else if (header_info.color_type == 3 && header_info.bit_depth <= 8) {
            mode = PILLOW_C_MODE_P;
            channels = 1;
            decoded_channels = has_palette_transparency ? 4 : 1;
            target_format = has_palette_transparency ? GUID_WICPixelFormat32bppRGBA : GUID_WICPixelFormat8bppIndexed;
        } else {
            status = wic_format_to_mode(source_format, &mode, &channels, &target_format);
            if (status != PILLOW_C_OK) {
                return status;
            }
            decoded_channels = channels;
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t decoded_stride = 0;
        std::size_t decoded_size = 0;
        if (!checked_image_size(width, height, decoded_channels, &decoded_stride, &decoded_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> palette_rgb;
        ComPtr<IWICPalette> source_palette;
        IWICPalette* converter_palette = nullptr;
        if (mode == PILLOW_C_MODE_P) {
            status = copy_wic_palette_rgb(frame.get(), factory.get(), &palette_rgb);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (has_palette_transparency) {
                palette_rgb = png_palette_rgb;
            }
            HRESULT palette_hr = factory->CreatePalette(source_palette.put());
            if (FAILED(palette_hr) || FAILED(frame->CopyPalette(source_palette.get()))) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            converter_palette = source_palette.get();
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, target_format)) {
            source.reset(frame.get());
            source.get()->AddRef();
        } else {
            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                target_format,
                WICBitmapDitherTypeNone,
                converter_palette,
                0.0,
                mode == PILLOW_C_MODE_P ? WICBitmapPaletteTypeCustom : WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        std::vector<std::uint8_t> decoded;
        std::uint8_t* copy_target = image->pixels.data();
        UINT copy_stride = static_cast<UINT>(stride);
        UINT copy_size = static_cast<UINT>(image->pixels.size());
        if (decoded_channels != channels) {
            decoded.assign(decoded_size, std::uint8_t{0});
            copy_target = decoded.data();
            copy_stride = static_cast<UINT>(decoded_stride);
            copy_size = static_cast<UINT>(decoded.size());
        }
        hr = source->CopyPixels(
            nullptr,
            copy_stride,
            copy_size,
            copy_target);
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (mode == PILLOW_C_MODE_LA) {
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = decoded.data() + static_cast<std::size_t>(y) * decoded_stride;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < width; ++x) {
                    const std::size_t src = static_cast<std::size_t>(x) * 4u;
                    const std::size_t dst = static_cast<std::size_t>(x) * 2u;
                    dst_row[dst + 0u] = src_row[src + 0u];
                    dst_row[dst + 1u] = src_row[src + 3u];
                }
            }
        } else if (mode == PILLOW_C_MODE_P && has_palette_transparency) {
            status = remap_png_rgba_to_palette_indices(
                decoded,
                width,
                height,
                decoded_stride,
                png_palette_rgb,
                png_palette_alpha,
                image);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
        }
        if (mode == PILLOW_C_MODE_P) {
            image->palette_rgb = std::move(palette_rgb);
            if (has_palette_transparency) {
                image->palette_alpha = std::move(png_palette_alpha);
                image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_RGBA;
                if (png_transparency >= 0) {
                    image->has_png_transparency = true;
                    image->png_transparency = png_transparency;
                } else if (!png_transparency_table.empty()) {
                    image->png_transparency_table = std::move(png_transparency_table);
                }
            } else {
                image->palette_alpha.clear();
                image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
            }
        }
        double dpi_x = 0.0;
        double dpi_y = 0.0;
        if (read_png_dpi_metadata(path, &dpi_x, &dpi_y)) {
            image->has_dpi = true;
            image->dpi_x = dpi_x;
            image->dpi_y = dpi_y;
        }
        double png_gamma = 0.0;
        if (read_png_gamma_metadata(path, &png_gamma)) {
            image->has_png_gamma = true;
            image->png_gamma = png_gamma;
        }
        int png_srgb = 0;
        if (read_png_srgb_metadata(path, &png_srgb)) {
            image->has_png_srgb = true;
            image->png_srgb = png_srgb;
        }
        double png_chromaticity[8] = {};
        if (read_png_chromaticity_metadata(path, png_chromaticity, 8u)) {
            image->has_png_chromaticity = true;
            for (std::size_t i = 0; i < 8u; ++i) {
                image->png_chromaticity[i] = png_chromaticity[i];
            }
        }
        std::vector<std::pair<std::string, std::string>> png_text;
        if (read_png_text_metadata(path, &png_text)) {
            image->png_text = std::move(png_text);
        }
        std::vector<std::uint8_t> png_xmp;
        if (read_png_xmp_metadata(path, &png_xmp)) {
            image->xmp = std::move(png_xmp);
        }
        std::vector<std::uint8_t> png_icc_profile;
        if (read_png_icc_profile(path, &png_icc_profile)) {
            image->png_icc_profile = std::move(png_icc_profile);
        }
        std::vector<std::uint8_t> png_exif;
        if (read_png_exif_metadata(path, &png_exif)) {
            image->exif_orientation = pillow_c_parse_exif_orientation(png_exif.data(), png_exif.size());
            image->png_exif = std::move(png_exif);
        }
        if (has_rgb_transparency) {
            image->has_png_rgb_transparency = true;
            image->png_rgb_transparency[0] = png_rgb_transparency[0];
            image->png_rgb_transparency[1] = png_rgb_transparency[1];
            image->png_rgb_transparency[2] = png_rgb_transparency[2];
        }
        if (has_grayscale_transparency) {
            image->has_png_transparency = true;
            image->png_transparency = png_grayscale_transparency;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

bool read_png_header_info_memory(const std::uint8_t* data, std::size_t size, PngHeaderInfo* info)
{
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!data || !info || size < 33u ||
        std::memcmp(data, signature, sizeof(signature)) != 0 ||
        read_be32(data + 8) != 13u || std::memcmp(data + 12, "IHDR", 4) != 0) {
        return false;
    }
    info->bit_depth = data[24];
    info->color_type = data[25];
    return true;
}

bool read_png_palette_transparency_memory(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<std::uint8_t>* out_palette_rgb,
    std::vector<std::uint8_t>* out_palette_alpha,
    std::vector<std::uint8_t>* out_transparency_table,
    int* out_transparency)
{
    if (!data || !out_palette_rgb || !out_palette_alpha || !out_transparency_table || !out_transparency) {
        return false;
    }
    out_palette_rgb->clear();
    out_palette_alpha->clear();
    out_transparency_table->clear();
    *out_transparency = -1;
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < sizeof(signature) || std::memcmp(data, signature, sizeof(signature)) != 0) {
        return false;
    }

    std::vector<std::uint8_t> palette_rgb;
    std::vector<std::uint8_t> transparency;
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= size) {
        const std::uint32_t length = read_be32(data + pos);
        if (length > size - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data + pos + 4u;
        const std::uint8_t* payload = data + pos + 8u;
        if (std::memcmp(type, "PLTE", 4u) == 0) {
            if (length == 0u || length % 3u != 0u || length > 768u) {
                return false;
            }
            palette_rgb.assign(payload, payload + length);
        } else if (std::memcmp(type, "tRNS", 4u) == 0) {
            if (length == 0u || length > 256u) {
                return false;
            }
            transparency.assign(payload, payload + length);
        } else if (std::memcmp(type, "IEND", 4u) == 0) {
            break;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    if (palette_rgb.empty() || transparency.empty()) {
        return false;
    }

    const std::size_t entries = palette_rgb.size() / 3u;
    out_palette_rgb->assign(palette_rgb.begin(), palette_rgb.end());
    out_palette_alpha->assign(entries, std::uint8_t{255});
    const std::size_t alpha_count = std::min(entries, transparency.size());
    for (std::size_t index = 0; index < alpha_count; ++index) {
        (*out_palette_alpha)[index] = transparency[index];
    }
    int zero_index = -1;
    int zero_count = 0;
    bool has_partial_alpha = false;
    for (std::size_t index = 0; index < alpha_count; ++index) {
        if (transparency[index] == 0u) {
            zero_index = static_cast<int>(index);
            ++zero_count;
        } else if (transparency[index] != 255u) {
            has_partial_alpha = true;
        }
    }
    if (zero_count == 1 && !has_partial_alpha) {
        *out_transparency = zero_index;
    } else {
        out_transparency_table->assign(transparency.begin(), transparency.begin() + static_cast<std::ptrdiff_t>(alpha_count));
    }
    return true;
}

bool read_png_rgb_transparency_memory(const std::uint8_t* data, std::size_t size, std::uint8_t out_rgb[3])
{
    if (!data || !out_rgb) {
        return false;
    }
    out_rgb[0] = 0;
    out_rgb[1] = 0;
    out_rgb[2] = 0;
    PngHeaderInfo header_info{};
    if (!read_png_header_info_memory(data, size, &header_info) ||
        header_info.bit_depth != 8 ||
        header_info.color_type != 2) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < sizeof(signature) || std::memcmp(data, signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= size) {
        const std::uint32_t length = read_be32(data + pos);
        if (length > size - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data + pos + 4u;
        const std::uint8_t* payload = data + pos + 8u;
        if (std::memcmp(type, "tRNS", 4u) == 0) {
            if (length != 6u) {
                return false;
            }
            for (int channel = 0; channel < 3; ++channel) {
                const std::uint16_t value = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(payload[channel * 2]) << 8) |
                    static_cast<std::uint16_t>(payload[channel * 2 + 1]));
                if (value > 255u) {
                    return false;
                }
                out_rgb[channel] = static_cast<std::uint8_t>(value);
            }
            return true;
        }
        if (std::memcmp(type, "IDAT", 4u) == 0 || std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool read_png_grayscale_transparency_memory(const std::uint8_t* data, std::size_t size, std::uint8_t* out_value)
{
    if (!data || !out_value) {
        return false;
    }
    *out_value = 0;
    PngHeaderInfo header_info{};
    if (!read_png_header_info_memory(data, size, &header_info) ||
        header_info.bit_depth != 8 ||
        header_info.color_type != 0) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < sizeof(signature) || std::memcmp(data, signature, sizeof(signature)) != 0) {
        return false;
    }
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= size) {
        const std::uint32_t length = read_be32(data + pos);
        if (length > size - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data + pos + 4u;
        const std::uint8_t* payload = data + pos + 8u;
        if (std::memcmp(type, "tRNS", 4u) == 0) {
            if (length != 2u) {
                return false;
            }
            const std::uint16_t value = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(payload[0]) << 8) |
                static_cast<std::uint16_t>(payload[1]));
            if (value > 255u) {
                return false;
            }
            *out_value = static_cast<std::uint8_t>(value);
            return true;
        }
        if (std::memcmp(type, "IDAT", 4u) == 0 || std::memcmp(type, "IEND", 4u) == 0) {
            return false;
        }
        pos += 12u + static_cast<std::size_t>(length);
    }
    return false;
}

bool copy_png_without_wic_sensitive_chunks_memory(
    const std::uint8_t* data,
    std::size_t size,
    bool remove_trns,
    bool remove_compressed_text,
    std::vector<std::uint8_t>* out_png)
{
    if (!data || !out_png) {
        return false;
    }
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < sizeof(signature) || std::memcmp(data, signature, sizeof(signature)) != 0) {
        return false;
    }
    std::vector<std::uint8_t> copy;
    copy.insert(copy.end(), signature, signature + sizeof(signature));
    bool removed_chunk = false;
    std::size_t pos = sizeof(signature);
    while (pos + 12u <= size) {
        const std::uint32_t length = read_be32(data + pos);
        if (length > size - pos - 12u) {
            return false;
        }
        const std::uint8_t* type = data + pos + 4u;
        const std::uint8_t* payload = data + pos + 8u;
        const std::size_t chunk_size = 12u + static_cast<std::size_t>(length);
        if ((remove_trns && std::memcmp(type, "tRNS", 4u) == 0) ||
            (remove_compressed_text && png_chunk_is_compressed_text_for_wic(type, payload, length))) {
            removed_chunk = true;
        } else {
            copy.insert(
                copy.end(),
                data + static_cast<std::ptrdiff_t>(pos),
                data + static_cast<std::ptrdiff_t>(pos + chunk_size));
        }
        pos += chunk_size;
        if (std::memcmp(type, "IEND", 4u) == 0) {
            break;
        }
    }
    if (!removed_chunk) {
        return false;
    }
    *out_png = std::move(copy);
    return true;
}

int png_mode_format(const PillowCImage* image, WICPixelFormatGUID* format)
{
    if (!image || !format) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        *format = GUID_WICPixelFormat8bppGray;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        *format = GUID_WICPixelFormat24bppBGR;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        *format = GUID_WICPixelFormat32bppBGRA;
        return PILLOW_C_OK;
    }
    return PILLOW_C_INVALID_ARGUMENT;
}

int png_custom_mode_spec(const PillowCImage* image, int* color_type, int* payload_channels)
{
    if (!image || !color_type || !payload_channels) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        *color_type = 0;
        *payload_channels = 1;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        *color_type = 2;
        *payload_channels = 3;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_P && image->channels == 1) {
        *color_type = 3;
        *payload_channels = 1;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_LA && image->channels == 2) {
        *color_type = 4;
        *payload_channels = 2;
        return PILLOW_C_OK;
    }
    if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        *color_type = 6;
        *payload_channels = 4;
        return PILLOW_C_OK;
    }
    return PILLOW_C_INVALID_ARGUMENT;
}

std::uint32_t crc32_bytes(const std::uint8_t* data, std::size_t size)
{
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return crc ^ 0xffffffffu;
}

void append_png_chunk(std::vector<std::uint8_t>& out, const char type[4], const std::vector<std::uint8_t>& data)
{
    append_be32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t type_offset = out.size();
    out.push_back(static_cast<std::uint8_t>(type[0]));
    out.push_back(static_cast<std::uint8_t>(type[1]));
    out.push_back(static_cast<std::uint8_t>(type[2]));
    out.push_back(static_cast<std::uint8_t>(type[3]));
    out.insert(out.end(), data.begin(), data.end());
    append_be32(out, crc32_bytes(out.data() + type_offset, out.size() - type_offset));
}

int append_zlib_stored(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& raw, std::uint8_t flags = 0x01u)
{
    out.push_back(0x78u);
    out.push_back(flags);
    std::size_t offset = 0;
    do {
        const std::size_t remaining = raw.size() - offset;
        const std::uint16_t block_size = static_cast<std::uint16_t>(std::min<std::size_t>(remaining, 65535u));
        const bool final_block = offset + block_size == raw.size();
        out.push_back(final_block ? 0x01u : 0x00u);
        append_le16(out, block_size);
        append_le16(out, static_cast<std::uint16_t>(~block_size));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + block_size));
        offset += block_size;
    } while (offset < raw.size());
    append_be32(out, adler32_bytes(raw.empty() ? nullptr : raw.data(), raw.size()));
    return PILLOW_C_OK;
}

std::uint8_t png_zlib_header_flags_for_level(int compress_level)
{
    if (compress_level >= 7) {
        return 0xDAu;
    }
    if (compress_level >= 6) {
        return 0x9Cu;
    }
    if (compress_level >= 2) {
        return 0x5Eu;
    }
    return 0x01u;
}

bool png_dpi_to_pixels_per_meter(double dpi, std::uint32_t* out_value)
{
    if (!out_value || !std::isfinite(dpi) || dpi <= 0.0) {
        return false;
    }
    const double pixels_per_meter = dpi / 0.0254 + 0.5;
    if (pixels_per_meter < 0.0 || pixels_per_meter > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    *out_value = static_cast<std::uint32_t>(pixels_per_meter);
    return true;
}

int append_png_phys_chunk(std::vector<std::uint8_t>& png, double dpi_x, double dpi_y)
{
    std::uint32_t x_pixels_per_meter = 0;
    std::uint32_t y_pixels_per_meter = 0;
    if (!png_dpi_to_pixels_per_meter(dpi_x, &x_pixels_per_meter) ||
        !png_dpi_to_pixels_per_meter(dpi_y, &y_pixels_per_meter)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> phys;
    append_be32(phys, x_pixels_per_meter);
    append_be32(phys, y_pixels_per_meter);
    phys.push_back(1);
    append_png_chunk(png, "pHYs", phys);
    return PILLOW_C_OK;
}

enum PngTextEntryKind {
    PNG_TEXT_ENTRY_TEXT = 0,
    PNG_TEXT_ENTRY_ITXT = 1,
};

enum PngMetadataSaveFlags : std::uint32_t {
    PNG_METADATA_HAS_GAMA = 0x01u,
    PNG_METADATA_CHUNK_AFTER_ICC = 0x02u,
    PNG_METADATA_TEXT_BEFORE_EXIF = 0x04u,
    PNG_METADATA_OPTIMIZE = 0x08u,
    PNG_METADATA_HAS_TRANSPARENCY = 0x10u,
    PNG_METADATA_HAS_RGB_TRANSPARENCY = 0x20u,
    PNG_METADATA_CHUNK_AFTER_IDAT = 0x40u,
};

constexpr std::uint32_t PNG_METADATA_KNOWN_FLAGS =
    PNG_METADATA_HAS_GAMA |
    PNG_METADATA_CHUNK_AFTER_ICC |
    PNG_METADATA_TEXT_BEFORE_EXIF |
    PNG_METADATA_OPTIMIZE |
    PNG_METADATA_HAS_TRANSPARENCY |
    PNG_METADATA_HAS_RGB_TRANSPARENCY |
    PNG_METADATA_CHUNK_AFTER_IDAT;

bool is_png_text_ascii(const char* value, bool is_key)
{
    if (!value) {
        return false;
    }
    const std::size_t length = std::strlen(value);
    if (is_key && (length == 0u || length > 79u)) {
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (ch >= 128u) {
            return false;
        }
        if (is_key && (ch < 32u || ch == 127u)) {
            return false;
        }
    }
    return true;
}

int append_png_text_chunk_bytes(
    std::vector<std::uint8_t>& png,
    const char* key,
    const std::uint8_t* value,
    std::size_t value_length)
{
    if (!is_png_text_ascii(key, true) || (!value && value_length > 0u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t key_length = std::strlen(key);
    if (key_length + 1u > std::numeric_limits<std::uint32_t>::max() - value_length) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> text;
    text.reserve(key_length + 1u + value_length);
    text.insert(text.end(), reinterpret_cast<const std::uint8_t*>(key), reinterpret_cast<const std::uint8_t*>(key) + key_length);
    text.push_back(0u);
    if (value_length > 0u) {
        text.insert(text.end(), value, value + value_length);
    }
    append_png_chunk(png, "tEXt", text);
    return PILLOW_C_OK;
}

int append_png_text_chunk(std::vector<std::uint8_t>& png, const char* key, const char* value)
{
    if (!value) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return append_png_text_chunk_bytes(
        png,
        key,
        reinterpret_cast<const std::uint8_t*>(value),
        std::strlen(value));
}

int append_png_ztxt_chunk_bytes(
    std::vector<std::uint8_t>& png,
    const char* key,
    const std::uint8_t* value,
    std::size_t value_length)
{
    if (!is_png_text_ascii(key, true) || (!value && value_length > 0u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t key_length = std::strlen(key);
    std::vector<std::uint8_t> compressed;
    compressed.reserve(value_length + 16u);
    if (value_length > 0u) {
        compressed.insert(compressed.end(), value, value + value_length);
    }
    std::vector<std::uint8_t> zlib;
    append_zlib_stored(zlib, compressed);
    if (key_length + 2u > std::numeric_limits<std::uint32_t>::max() - zlib.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> text;
    text.reserve(key_length + 2u + zlib.size());
    text.insert(text.end(), reinterpret_cast<const std::uint8_t*>(key), reinterpret_cast<const std::uint8_t*>(key) + key_length);
    text.push_back(0u);
    text.push_back(0u);
    text.insert(text.end(), zlib.begin(), zlib.end());
    append_png_chunk(png, "zTXt", text);
    return PILLOW_C_OK;
}

int append_png_ztxt_chunk(std::vector<std::uint8_t>& png, const char* key, const char* value)
{
    if (!value) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return append_png_ztxt_chunk_bytes(
        png,
        key,
        reinterpret_cast<const std::uint8_t*>(value),
        std::strlen(value));
}

int append_png_itxt_chunk(
    std::vector<std::uint8_t>& png,
    const char* key,
    const char* value,
    bool compressed,
    const char* lang = "",
    const char* translated_key = "")
{
    if (!is_png_text_ascii(key, true) || !value || !lang || !translated_key) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!is_png_text_ascii(lang, false)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t key_length = std::strlen(key);
    const std::size_t value_length = std::strlen(value);
    const std::size_t lang_length = std::strlen(lang);
    const std::size_t translated_key_length = std::strlen(translated_key);
    std::vector<std::uint8_t> payload;
    const std::uint8_t* text_begin = reinterpret_cast<const std::uint8_t*>(value);
    const std::uint8_t* text_end = text_begin + value_length;
    if (compressed) {
        std::vector<std::uint8_t> raw;
        try {
            raw.assign(text_begin, text_end);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
        append_zlib_stored(payload, raw);
        text_begin = payload.data();
        text_end = payload.data() + payload.size();
    }
    const std::size_t output_value_length = static_cast<std::size_t>(text_end - text_begin);
    const std::size_t prefix_length = key_length + 3u + lang_length + 1u + translated_key_length + 1u;
    if (prefix_length > std::numeric_limits<std::uint32_t>::max() - output_value_length) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> text;
    text.reserve(prefix_length + output_value_length);
    text.insert(text.end(), reinterpret_cast<const std::uint8_t*>(key), reinterpret_cast<const std::uint8_t*>(key) + key_length);
    text.push_back(0u);
    text.push_back(compressed ? 1u : 0u);
    text.push_back(0u);
    text.insert(text.end(), reinterpret_cast<const std::uint8_t*>(lang), reinterpret_cast<const std::uint8_t*>(lang) + lang_length);
    text.push_back(0u);
    text.insert(
        text.end(),
        reinterpret_cast<const std::uint8_t*>(translated_key),
        reinterpret_cast<const std::uint8_t*>(translated_key) + translated_key_length);
    text.push_back(0u);
    text.insert(text.end(), text_begin, text_end);
    append_png_chunk(png, "iTXt", text);
    return PILLOW_C_OK;
}

int append_png_iccp_chunk(std::vector<std::uint8_t>& png, const std::uint8_t* profile, std::size_t profile_size)
{
    if (!profile || profile_size == 0u) {
        return profile ? PILLOW_C_INVALID_LENGTH : PILLOW_C_NULL_POINTER;
    }
    std::vector<std::uint8_t> profile_bytes;
    try {
        profile_bytes.assign(profile, profile + profile_size);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    std::vector<std::uint8_t> zlib;
    append_zlib_stored(zlib, profile_bytes);
    static constexpr char keyword[] = "ICC Profile";
    const std::size_t key_length = sizeof(keyword) - 1u;
    if (key_length + 2u > std::numeric_limits<std::uint32_t>::max() - zlib.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> chunk;
    chunk.reserve(key_length + 2u + zlib.size());
    chunk.insert(chunk.end(), reinterpret_cast<const std::uint8_t*>(keyword), reinterpret_cast<const std::uint8_t*>(keyword) + key_length);
    chunk.push_back(0u);
    chunk.push_back(0u);
    chunk.insert(chunk.end(), zlib.begin(), zlib.end());
    append_png_chunk(png, "iCCP", chunk);
    return PILLOW_C_OK;
}

int append_png_exif_chunk(std::vector<std::uint8_t>& png, const std::uint8_t* exif, std::size_t exif_size)
{
    if (!exif || exif_size == 0u) {
        return exif ? PILLOW_C_INVALID_LENGTH : PILLOW_C_NULL_POINTER;
    }
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (exif_size <= sizeof(exif_header) ||
        std::memcmp(exif, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint8_t* payload = exif + sizeof(exif_header);
    const std::size_t payload_size = exif_size - sizeof(exif_header);
    if (payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> chunk;
    try {
        chunk.assign(payload, payload + payload_size);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    append_png_chunk(png, "eXIf", chunk);
    return PILLOW_C_OK;
}

int append_png_gama_chunk(std::vector<std::uint8_t>& png, std::uint32_t gamma_raw)
{
    std::vector<std::uint8_t> chunk;
    append_be32(chunk, gamma_raw);
    append_png_chunk(png, "gAMA", chunk);
    return PILLOW_C_OK;
}

bool is_png_private_chunk_type(const char* type)
{
    return type && type[1] >= 'a' && type[1] <= 'z';
}

bool is_supported_png_pre_idat_chunk(const char* type, std::size_t data_size)
{
    if (!type) {
        return false;
    }
    if (std::memcmp(type, "gAMA", 4u) == 0) {
        return data_size == 4u;
    }
    if (std::memcmp(type, "sRGB", 4u) == 0) {
        return data_size == 1u;
    }
    if (std::memcmp(type, "sBIT", 4u) == 0) {
        return data_size == 3u;
    }
    if (std::memcmp(type, "sPLT", 4u) == 0) {
        return true;
    }
    if (std::memcmp(type, "cHRM", 4u) == 0) {
        return data_size == 32u;
    }
    if (std::memcmp(type, "cICP", 4u) == 0) {
        return data_size == 4u;
    }
    if (std::memcmp(type, "bKGD", 4u) == 0) {
        return data_size == 1u || data_size == 6u;
    }
    if (std::memcmp(type, "hIST", 4u) == 0) {
        return data_size >= 2u && data_size <= 512u && (data_size % 2u) == 0u;
    }
    if (std::memcmp(type, "tIME", 4u) == 0) {
        return data_size == 7u;
    }
    return is_png_private_chunk_type(type);
}

std::size_t png_bkgd_payload_size_for_mode(int mode)
{
    if (mode == PILLOW_C_MODE_P) {
        return 1u;
    }
    if (mode == PILLOW_C_MODE_RGB) {
        return 6u;
    }
    return 0u;
}

std::size_t png_sbit_payload_size_for_mode(int mode)
{
    return mode == PILLOW_C_MODE_RGB ? 3u : 0u;
}

std::size_t png_hist_payload_size_for_image(const PillowCImage* image)
{
    if (!image || image->mode != PILLOW_C_MODE_P) {
        return 0u;
    }
    const std::size_t palette_size = image->palette_rgb.empty()
        ? 3u
        : image->palette_rgb.size();
    if (palette_size % 3u != 0u || palette_size > 256u * 3u) {
        return 0u;
    }
    return (palette_size / 3u) * 2u;
}

struct PngCustomChunkSpec {
    const char* type;
    const std::uint8_t* data;
    std::size_t size;
    bool after_idat;
};

int append_png_pre_idat_chunk(
    std::vector<std::uint8_t>& png,
    const char* type,
    const std::uint8_t* data,
    std::size_t data_size)
{
    if (!type || (data_size > 0u && !data)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!is_supported_png_pre_idat_chunk(type, data_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (data_size > std::numeric_limits<std::uint32_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> chunk;
    try {
        if (data_size > 0u) {
            chunk.assign(data, data + data_size);
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    append_png_chunk(png, type, chunk);
    return PILLOW_C_OK;
}

int validate_png_custom_chunks(const PngCustomChunkSpec* chunks, std::size_t chunk_count)
{
    if (chunk_count > 0u && !chunks) {
        return PILLOW_C_NULL_POINTER;
    }
    for (std::size_t i = 0; i < chunk_count; ++i) {
        const PngCustomChunkSpec& chunk = chunks[i];
        if (!chunk.type || (chunk.size > 0u && !chunk.data)) {
            return PILLOW_C_NULL_POINTER;
        }
        if (!is_png_private_chunk_type(chunk.type)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (chunk.size > std::numeric_limits<std::uint32_t>::max()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return PILLOW_C_OK;
}

int append_png_custom_chunks(
    std::vector<std::uint8_t>& png,
    const PngCustomChunkSpec* chunks,
    std::size_t chunk_count,
    bool after_idat)
{
    for (std::size_t i = 0; i < chunk_count; ++i) {
        if (chunks[i].after_idat != after_idat) {
            continue;
        }
        const int status = append_png_pre_idat_chunk(png, chunks[i].type, chunks[i].data, chunks[i].size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int encode_png_custom_image_with_text_entries(
    const PillowCImage* image,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool has_transparency,
    int transparency,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size,
    bool has_rgb_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b,
    std::vector<std::uint8_t>* out_png,
    const char* const* text_keys,
    const char* const* text_values,
    std::size_t text_count,
    const int* text_compressed = nullptr,
    const int* text_kinds = nullptr,
    const char* const* text_langs = nullptr,
    const char* const* text_translated_keys = nullptr,
    const std::uint8_t* icc_profile = nullptr,
    std::size_t icc_profile_size = 0u,
    const std::uint8_t* exif = nullptr,
    std::size_t exif_size = 0u,
    bool optimize = false,
    bool text_before_exif = false,
    bool has_gama = false,
    std::uint32_t gama_raw = 0u,
    const char* pre_idat_chunk_type = nullptr,
    const std::uint8_t* pre_idat_chunk_data = nullptr,
    std::size_t pre_idat_chunk_size = 0u,
    bool pre_idat_chunk_after_icc = false,
    bool chunk_after_idat = false,
    int compress_level = -1,
    const PngCustomChunkSpec* custom_chunks = nullptr,
    std::size_t custom_chunk_count = 0u,
    bool custom_chunks_after_text = false,
    const std::size_t* text_value_sizes = nullptr)
{
    if (!image || !out_png) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size > 0u && !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (exif_size > 0u && !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count > 0u && (!text_keys || !text_values)) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((pre_idat_chunk_type || pre_idat_chunk_data || pre_idat_chunk_size > 0u) &&
        (!pre_idat_chunk_type || (pre_idat_chunk_size > 0u && !pre_idat_chunk_data))) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool pre_idat_chunk_is_bkgd = pre_idat_chunk_type && std::memcmp(pre_idat_chunk_type, "bKGD", 4u) == 0;
    const bool pre_idat_chunk_is_hist = pre_idat_chunk_type && std::memcmp(pre_idat_chunk_type, "hIST", 4u) == 0;
    const bool pre_idat_chunk_is_sbit = pre_idat_chunk_type && std::memcmp(pre_idat_chunk_type, "sBIT", 4u) == 0;
    const std::size_t pre_idat_hist_payload_size = pre_idat_chunk_is_hist
        ? png_hist_payload_size_for_image(image)
        : 0u;
    const bool pre_idat_chunk_defer_to_palette = !chunk_after_idat &&
        ((pre_idat_chunk_is_bkgd && png_bkgd_payload_size_for_mode(image->mode) == 1u) ||
         (pre_idat_chunk_is_hist && pre_idat_hist_payload_size > 0u));
    if (pre_idat_chunk_type) {
        if (chunk_after_idat) {
            if (!is_png_private_chunk_type(pre_idat_chunk_type)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        } else if (!is_supported_png_pre_idat_chunk(pre_idat_chunk_type, pre_idat_chunk_size) ||
                   (has_gama && std::memcmp(pre_idat_chunk_type, "gAMA", 4u) == 0) ||
                   (pre_idat_chunk_is_bkgd && pre_idat_chunk_size != png_bkgd_payload_size_for_mode(image->mode)) ||
                   (pre_idat_chunk_is_sbit && pre_idat_chunk_size != png_sbit_payload_size_for_mode(image->mode)) ||
                   (pre_idat_chunk_is_hist && pre_idat_chunk_size != pre_idat_hist_payload_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (custom_chunk_count > 0u) {
        if (pre_idat_chunk_type) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int chunk_status = validate_png_custom_chunks(custom_chunks, custom_chunk_count);
        if (chunk_status != PILLOW_C_OK) {
            return chunk_status;
        }
    }
    if ((text_langs && !text_translated_keys) || (!text_langs && text_translated_keys)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (std::size_t i = 0; i < text_count; ++i) {
        if (!text_keys[i] || !text_values[i]) {
            return PILLOW_C_NULL_POINTER;
        }
        if (text_langs && (!text_langs[i] || !text_translated_keys[i])) {
            return PILLOW_C_NULL_POINTER;
        }
        const int kind = text_kinds ? text_kinds[i] : PNG_TEXT_ENTRY_TEXT;
        if (kind != PNG_TEXT_ENTRY_TEXT && kind != PNG_TEXT_ENTRY_ITXT) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (text_value_sizes && kind != PNG_TEXT_ENTRY_TEXT) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (!is_png_text_ascii(text_keys[i], true)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (kind == PNG_TEXT_ENTRY_TEXT && text_langs && (text_langs[i][0] != '\0' || text_translated_keys[i][0] != '\0')) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (kind == PNG_TEXT_ENTRY_ITXT && text_langs && !is_png_text_ascii(text_langs[i], false)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (text_compressed && text_compressed[i] != 0 && text_compressed[i] != 1) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (transparency_table_size > 0u && !transparency_table) {
        return PILLOW_C_NULL_POINTER;
    }
    if (has_transparency &&
        ((image->mode != PILLOW_C_MODE_P && image->mode != PILLOW_C_MODE_L) ||
         transparency < 0 ||
         transparency > 255)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (transparency_table_size > 0u &&
        (image->mode != PILLOW_C_MODE_P || transparency_table_size > 256u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_rgb_transparency &&
        (image->mode != PILLOW_C_MODE_RGB ||
         transparency_r < 0 ||
         transparency_r > 255 ||
         transparency_g < 0 ||
         transparency_g > 255 ||
         transparency_b < 0 ||
         transparency_b > 255)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if ((has_transparency && has_rgb_transparency) ||
        (has_transparency && transparency_table_size > 0u) ||
        (has_rgb_transparency && transparency_table_size > 0u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int color_type = 0;
    int payload_channels = 0;
    int status = png_custom_mode_spec(image, &color_type, &payload_channels);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (image->width > (std::numeric_limits<int>::max() - 1) / payload_channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t raw_stride = 1u + static_cast<std::size_t>(image->width) * static_cast<std::size_t>(payload_channels);
    if (image->height > 0 && raw_stride > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(image->height)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t raw_size = raw_stride * static_cast<std::size_t>(image->height);

    try {
        std::vector<std::uint8_t> raw(raw_size, std::uint8_t{0});
        for (int y = 0; y < image->height; ++y) {
            std::uint8_t* dst = raw.data() + static_cast<std::size_t>(y) * raw_stride;
            const std::uint8_t* src = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            dst[0] = 0;
            std::memcpy(dst + 1, src, static_cast<std::size_t>(image->width) * static_cast<std::size_t>(payload_channels));
        }

        std::vector<std::uint8_t> png;
        static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        png.insert(png.end(), signature, signature + 8);

        std::vector<std::uint8_t> ihdr;
        append_be32(ihdr, static_cast<std::uint32_t>(image->width));
        append_be32(ihdr, static_cast<std::uint32_t>(image->height));
        ihdr.push_back(8);
        ihdr.push_back(static_cast<std::uint8_t>(color_type));
        ihdr.push_back(0);
        ihdr.push_back(0);
        ihdr.push_back(0);
        append_png_chunk(png, "IHDR", ihdr);

        if (has_dpi) {
            status = append_png_phys_chunk(png, dpi_x, dpi_y);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (has_gama) {
            status = append_png_gama_chunk(png, gama_raw);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (custom_chunk_count > 0u && !custom_chunks_after_text) {
            status = append_png_custom_chunks(png, custom_chunks, custom_chunk_count, false);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (pre_idat_chunk_type && !chunk_after_idat && !pre_idat_chunk_after_icc && !pre_idat_chunk_defer_to_palette) {
            status = append_png_pre_idat_chunk(png, pre_idat_chunk_type, pre_idat_chunk_data, pre_idat_chunk_size);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (icc_profile_size > 0u) {
            status = append_png_iccp_chunk(png, icc_profile, icc_profile_size);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (pre_idat_chunk_type && !chunk_after_idat && pre_idat_chunk_after_icc && !pre_idat_chunk_defer_to_palette) {
            status = append_png_pre_idat_chunk(png, pre_idat_chunk_type, pre_idat_chunk_data, pre_idat_chunk_size);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        const auto append_text_chunks = [&]() -> int {
            for (std::size_t i = 0; i < text_count; ++i) {
                const int kind = text_kinds ? text_kinds[i] : PNG_TEXT_ENTRY_TEXT;
                if (kind == PNG_TEXT_ENTRY_ITXT) {
                    const char* lang = text_langs ? text_langs[i] : "";
                    const char* translated_key = text_translated_keys ? text_translated_keys[i] : "";
                    status = append_png_itxt_chunk(
                        png,
                        text_keys[i],
                        text_values[i],
                        text_compressed && text_compressed[i] != 0,
                        lang,
                        translated_key);
                } else {
                    if (text_value_sizes) {
                        const auto* value_bytes = reinterpret_cast<const std::uint8_t*>(text_values[i]);
                        status = text_compressed && text_compressed[i]
                            ? append_png_ztxt_chunk_bytes(png, text_keys[i], value_bytes, text_value_sizes[i])
                            : append_png_text_chunk_bytes(png, text_keys[i], value_bytes, text_value_sizes[i]);
                    } else {
                        status = text_compressed && text_compressed[i]
                            ? append_png_ztxt_chunk(png, text_keys[i], text_values[i])
                            : append_png_text_chunk(png, text_keys[i], text_values[i]);
                    }
                }
                if (status != PILLOW_C_OK) {
                    return status;
                }
            }
            return PILLOW_C_OK;
        };

        bool wrote_custom_chunks_after_text = false;
        if (text_count > 0u && text_before_exif) {
            status = append_text_chunks();
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (custom_chunk_count > 0u && custom_chunks_after_text) {
                status = append_png_custom_chunks(png, custom_chunks, custom_chunk_count, false);
                if (status != PILLOW_C_OK) {
                    return status;
                }
                wrote_custom_chunks_after_text = true;
            }
        }

        bool wrote_rgb_transparency_before_exif = false;
        if (exif_size > 0u && has_rgb_transparency && image->mode == PILLOW_C_MODE_RGB) {
            std::vector<std::uint8_t> trns;
            append_be16(trns, static_cast<std::uint16_t>(transparency_r));
            append_be16(trns, static_cast<std::uint16_t>(transparency_g));
            append_be16(trns, static_cast<std::uint16_t>(transparency_b));
            append_png_chunk(png, "tRNS", trns);
            wrote_rgb_transparency_before_exif = true;
        }

        if (exif_size > 0u) {
            status = append_png_exif_chunk(png, exif, exif_size);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (text_count > 0u && !text_before_exif) {
            status = append_text_chunks();
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (custom_chunk_count > 0u && custom_chunks_after_text && !wrote_custom_chunks_after_text) {
            status = append_png_custom_chunks(png, custom_chunks, custom_chunk_count, false);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (image->mode == PILLOW_C_MODE_P) {
            std::vector<std::uint8_t> plte = image->palette_rgb;
            if (plte.empty()) {
                plte.assign(3u, std::uint8_t{0});
            }
            if (plte.size() % 3u != 0u || plte.size() > 256u * 3u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t palette_entries = plte.size() / 3u;
            if (has_transparency && static_cast<std::size_t>(transparency) >= palette_entries) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            append_png_chunk(png, "PLTE", plte);
            if (transparency_table_size > 0u) {
                const std::size_t count = std::min(transparency_table_size, palette_entries);
                if (count > 0u) {
                    std::vector<std::uint8_t> trns(transparency_table, transparency_table + count);
                    append_png_chunk(png, "tRNS", trns);
                }
            } else if (has_transparency) {
                std::vector<std::uint8_t> trns(static_cast<std::size_t>(transparency) + 1u, std::uint8_t{255});
                trns[static_cast<std::size_t>(transparency)] = 0u;
                append_png_chunk(png, "tRNS", trns);
            }
            if (pre_idat_chunk_defer_to_palette) {
                status = append_png_pre_idat_chunk(png, pre_idat_chunk_type, pre_idat_chunk_data, pre_idat_chunk_size);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            }
        } else if (has_transparency && image->mode == PILLOW_C_MODE_L) {
            std::vector<std::uint8_t> trns;
            append_be16(trns, static_cast<std::uint16_t>(transparency));
            append_png_chunk(png, "tRNS", trns);
        } else if (has_rgb_transparency && image->mode == PILLOW_C_MODE_RGB && !wrote_rgb_transparency_before_exif) {
            std::vector<std::uint8_t> trns;
            append_be16(trns, static_cast<std::uint16_t>(transparency_r));
            append_be16(trns, static_cast<std::uint16_t>(transparency_g));
            append_be16(trns, static_cast<std::uint16_t>(transparency_b));
            append_png_chunk(png, "tRNS", trns);
        }

        std::vector<std::uint8_t> zlib;
        append_zlib_stored(zlib, raw, optimize ? 0xDAu : png_zlib_header_flags_for_level(compress_level));
        append_png_chunk(png, "IDAT", zlib);

        if (pre_idat_chunk_type && chunk_after_idat) {
            status = append_png_pre_idat_chunk(png, pre_idat_chunk_type, pre_idat_chunk_data, pre_idat_chunk_size);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (custom_chunk_count > 0u) {
            status = append_png_custom_chunks(png, custom_chunks, custom_chunk_count, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        std::vector<std::uint8_t> empty;
        append_png_chunk(png, "IEND", empty);

        *out_png = std::move(png);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int encode_png_custom_image(
    const PillowCImage* image,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool has_transparency,
    int transparency,
    bool has_rgb_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b,
    std::vector<std::uint8_t>* out_png,
    const char* text_key = nullptr,
    const char* text_value = nullptr,
    bool optimize = false)
{
    if ((text_key && !text_value) || (!text_key && text_value)) {
        return PILLOW_C_NULL_POINTER;
    }

    const char* text_keys[1] = {text_key};
    const char* text_values[1] = {text_value};
    const std::size_t text_count = text_key ? 1u : 0u;
    return encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        has_transparency,
        transparency,
        nullptr,
        0u,
        has_rgb_transparency,
        transparency_r,
        transparency_g,
        transparency_b,
        out_png,
        text_count ? text_keys : nullptr,
        text_count ? text_values : nullptr,
        text_count,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        optimize);
}

int save_png_custom_image_with_dpi(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool optimize = false)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image(image, has_dpi, dpi_x, dpi_y, false, 0, false, 0, 0, 0, &png, nullptr, nullptr, optimize);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_gama_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    std::uint32_t gama_raw)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        true,
        gama_raw);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_chunk_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size)
{
    if (!image || !path || !chunk_type || (chunk_data_size > 0u && !chunk_data)) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        chunk_type,
        chunk_data,
        chunk_data_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image(const PillowCImage* image, const char* path)
{
    return save_png_custom_image_with_dpi(image, path, false, 0.0, 0.0);
}

int save_png_custom_image_with_transparency_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool has_transparency,
    int transparency)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        has_transparency,
        transparency,
        false,
        0,
        0,
        0,
        &png);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_transparency_table_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size)
{
    if (!image || !path || (!transparency_table && transparency_table_size > 0u)) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        transparency_table,
        transparency_table_size,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        has_transparency,
        transparency_r,
        transparency_g,
        transparency_b,
        &png);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* key,
    const char* value)
{
    if (!image || !path || !key || !value) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        false,
        0,
        0,
        0,
        &png,
        key,
        value);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count)
{
    if (!image || !path || !keys || !values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_value_sizes_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compress_level,
    const char* const* keys,
    const char* const* values,
    const std::size_t* value_sizes,
    const int* compressed,
    std::size_t text_count)
{
    if (!image || !path || !keys || !values || !value_sizes || !compressed) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        nullptr,
        nullptr,
        0u,
        false,
        false,
        compress_level,
        nullptr,
        0u,
        false,
        value_sizes);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_chunk_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size)
{
    if (!image || !path || !keys || !values || !chunk_type || !chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || chunk_data_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        reinterpret_cast<const char*>(chunk_type),
        chunk_data,
        chunk_data_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_chunk_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    bool has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!image || !path || !keys || !values || !chunk_type || !chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || chunk_data_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        has_transparency,
        transparency_r,
        transparency_g,
        transparency_b,
        &png,
        keys,
        values,
        text_count,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        reinterpret_cast<const char*>(chunk_type),
        chunk_data,
        chunk_data_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_chunk_icc_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!image || !path || !chunk_type || !chunk_data || !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_data_size == 0u || icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        icc_profile,
        icc_profile_size,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        reinterpret_cast<const char*>(chunk_type),
        chunk_data,
        chunk_data_size,
        true);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_chunk_exif_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!image || !path || !chunk_type || !chunk_data || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_data_size == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        exif,
        exif_size,
        false,
        false,
        false,
        0u,
        reinterpret_cast<const char*>(chunk_type),
        chunk_data,
        chunk_data_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_chunk_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    bool has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!image || !path || !chunk_type || !chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_data_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        has_transparency,
        transparency_r,
        transparency_g,
        transparency_b,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        reinterpret_cast<const char*>(chunk_type),
        chunk_data,
        chunk_data_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_ex_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* compressed,
    std::size_t text_count)
{
    if (!image || !path || !keys || !values || !compressed) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_kind_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count)
{
    if (!image || !path || !keys || !values || !kinds || !compressed) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_itxt_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count)
{
    if (!image || !path || !keys || !values || !kinds || !compressed || !langs || !translated_keys) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        langs,
        translated_keys);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_icc_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!image || !path || !keys || !values || !kinds || !compressed || !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        nullptr,
        nullptr,
        icc_profile,
        icc_profile_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_exif_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!image || !path || !keys || !values || !kinds || !compressed || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        nullptr,
        nullptr,
        nullptr,
        0u,
        exif,
        exif_size,
        false,
        true);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_text_entries_icc_exif_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!image || !path || !keys || !values || !kinds || !compressed || !icc_profile || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || icc_profile_size == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        nullptr,
        nullptr,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size,
        false,
        true);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_icc_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!image || !path || !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        icc_profile,
        icc_profile_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_exif_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!image || !path || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        exif,
        exif_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_custom_image_with_icc_exif_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!image || !path || !icc_profile || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}


int save_png_image_with_dpi(const PillowCImage* image, const char* path, bool has_dpi, double dpi_x, double dpi_y)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return save_png_custom_image_with_dpi(image, path, true, dpi_x, dpi_y);
    }
    if (image->mode == PILLOW_C_MODE_LA || image->mode == PILLOW_C_MODE_P) {
        return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
    }

    WICPixelFormatGUID format = {};
    int status = png_mode_format(image, &format);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = stream->InitializeFromFilename(wide_path.data(), GENERIC_WRITE);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(frame.put(), nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->SetSize(static_cast<UINT>(image->width), static_cast<UINT>(image->height));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        WICPixelFormatGUID encoder_format = format;
        hr = frame->SetPixelFormat(&encoder_format);
        if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<std::uint8_t> encoded_pixels;
        const std::uint8_t* write_data = image->pixels.data();
        if (image->mode == PILLOW_C_MODE_RGB || image->mode == PILLOW_C_MODE_RGBA) {
            encoded_pixels.assign(image->pixels.size(), std::uint8_t{0});
            const std::size_t channels = static_cast<std::size_t>(image->channels);
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                std::uint8_t* dst_row = encoded_pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < image->width; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(x) * channels;
                    dst_row[offset + 0u] = src_row[offset + 2u];
                    dst_row[offset + 1u] = src_row[offset + 1u];
                    dst_row[offset + 2u] = src_row[offset + 0u];
                    if (channels == 4u) {
                        dst_row[offset + 3u] = src_row[offset + 3u];
                    }
                }
            }
            write_data = encoded_pixels.data();
        }
        hr = frame->WritePixels(
            static_cast<UINT>(image->height),
            static_cast<UINT>(image->stride),
            static_cast<UINT>(image->pixels.size()),
            const_cast<BYTE*>(write_data));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Commit();
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_png_image(const PillowCImage* image, const char* path)
{
    return save_png_image_with_dpi(image, path, false, 0.0, 0.0);
}

int save_png_image_with_compress_level(const PillowCImage* image, const char* path, int compress_level)
{
    if (compress_level == -1) {
        return save_png_image(image, path);
    }
    if (compress_level < 0 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level == 0 || compress_level == 1) {
        return save_png_custom_image(image, path);
    }
    return save_png_image(image, path);
}

int save_png_image_with_options(const PillowCImage* image, const char* path, int compress_level, double dpi_x, double dpi_y)
{
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (compress_level == 0 || compress_level == 1) {
        return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
    }
    return save_png_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
}

int save_png_image_with_interlace_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int interlace)
{
    (void)interlace;
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
}

int save_png_image_with_gamma_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    double gamma)
{
    (void)gamma;
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y);
}

int save_png_image_with_gama_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    std::uint32_t gama_raw)
{
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_gama_options(image, path, has_dpi, dpi_x, dpi_y, gama_raw);
}

int save_png_image_with_chunk_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size)
{
    if (!chunk_type || (chunk_data_size > 0u && !chunk_data)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_chunk_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        reinterpret_cast<const char*>(chunk_type),
        chunk_data,
        chunk_data_size);
}

int save_png_image_with_optimize_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int optimize)
{
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (optimize != 0) {
        return save_png_custom_image_with_dpi(image, path, has_dpi, dpi_x, dpi_y, true);
    }
    return save_png_image_with_options(image, path, compress_level, dpi_x, dpi_y);
}

int save_png_image_with_metadata_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    std::uint32_t flags,
    std::uint32_t gama_raw,
    int transparency,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((flags & ~PNG_METADATA_KNOWN_FLAGS) != 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    if (text_count > 0u && (!keys || !values || !kinds || !compressed)) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((langs && !translated_keys) || (!langs && translated_keys)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size > 0u && !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile && icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (exif_size > 0u && !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (exif && exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (chunk_data_size > 0u && (!chunk_type || !chunk_data)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!chunk_type && chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (transparency_table_size > 0u && !transparency_table) {
        return PILLOW_C_NULL_POINTER;
    }
    if (transparency_table && transparency_table_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    const bool has_transparency = (flags & PNG_METADATA_HAS_TRANSPARENCY) != 0u;
    const bool has_rgb_transparency = (flags & PNG_METADATA_HAS_RGB_TRANSPARENCY) != 0u;
    if ((has_transparency && has_rgb_transparency) ||
        (has_transparency && transparency_table_size > 0u) ||
        (has_rgb_transparency && transparency_table_size > 0u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        has_transparency,
        transparency,
        transparency_table,
        transparency_table_size,
        has_rgb_transparency,
        transparency_r,
        transparency_g,
        transparency_b,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        langs,
        translated_keys,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size,
        (flags & PNG_METADATA_OPTIMIZE) != 0u,
        (flags & PNG_METADATA_TEXT_BEFORE_EXIF) != 0u,
        (flags & PNG_METADATA_HAS_GAMA) != 0u,
        gama_raw,
        chunk_type ? reinterpret_cast<const char*>(chunk_type) : nullptr,
        chunk_data,
        chunk_data_size,
        (flags & PNG_METADATA_CHUNK_AFTER_ICC) != 0u,
        (flags & PNG_METADATA_CHUNK_AFTER_IDAT) != 0u,
        compress_level);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_image_with_metadata_custom_chunks_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count,
    std::uint32_t flags,
    std::uint32_t gama_raw,
    int transparency,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!chunk_types || !chunk_data || !chunk_data_sizes || !chunk_after_idat) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((flags & (PNG_METADATA_CHUNK_AFTER_ICC | PNG_METADATA_CHUNK_AFTER_IDAT)) != 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if ((flags & ~PNG_METADATA_KNOWN_FLAGS) != 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    if (text_count > 0u && (!keys || !values || !kinds || !compressed)) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((langs && !translated_keys) || (!langs && translated_keys)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size > 0u && !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile && icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (exif_size > 0u && !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (exif && exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (transparency_table_size > 0u && !transparency_table) {
        return PILLOW_C_NULL_POINTER;
    }
    if (transparency_table && transparency_table_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    const bool has_transparency = (flags & PNG_METADATA_HAS_TRANSPARENCY) != 0u;
    const bool has_rgb_transparency = (flags & PNG_METADATA_HAS_RGB_TRANSPARENCY) != 0u;
    if ((has_transparency && has_rgb_transparency) ||
        (has_transparency && transparency_table_size > 0u) ||
        (has_rgb_transparency && transparency_table_size > 0u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<PngCustomChunkSpec> chunks;
    try {
        chunks.reserve(chunk_count);
        for (std::size_t i = 0; i < chunk_count; ++i) {
            if (chunk_after_idat[i] != 0 && chunk_after_idat[i] != 1) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            chunks.push_back(PngCustomChunkSpec{
                reinterpret_cast<const char*>(chunk_types + i * 4u),
                chunk_data[i],
                chunk_data_sizes[i],
                chunk_after_idat[i] != 0});
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    const int validation_status = validate_png_custom_chunks(chunks.data(), chunks.size());
    if (validation_status != PILLOW_C_OK) {
        return validation_status;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        has_transparency,
        transparency,
        transparency_table,
        transparency_table_size,
        has_rgb_transparency,
        transparency_r,
        transparency_g,
        transparency_b,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        langs,
        translated_keys,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size,
        (flags & PNG_METADATA_OPTIMIZE) != 0u,
        (flags & PNG_METADATA_TEXT_BEFORE_EXIF) != 0u,
        (flags & PNG_METADATA_HAS_GAMA) != 0u,
        gama_raw,
        nullptr,
        nullptr,
        0u,
        false,
        false,
        compress_level,
        chunks.data(),
        chunks.size(),
        true);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_image_with_custom_chunks_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!chunk_types || !chunk_data || !chunk_data_sizes || !chunk_after_idat) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    std::vector<PngCustomChunkSpec> chunks;
    try {
        chunks.reserve(chunk_count);
        for (std::size_t i = 0; i < chunk_count; ++i) {
            if (chunk_after_idat[i] != 0 && chunk_after_idat[i] != 1) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            chunks.push_back(PngCustomChunkSpec{
                reinterpret_cast<const char*>(chunk_types + i * 4u),
                chunk_data[i],
                chunk_data_sizes[i],
                chunk_after_idat[i] != 0});
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    const int validation_status = validate_png_custom_chunks(chunks.data(), chunks.size());
    if (validation_status != PILLOW_C_OK) {
        return validation_status;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        nullptr,
        nullptr,
        0u,
        false,
        false,
        compress_level,
        chunks.data(),
        chunks.size());
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_image_with_text_entries_custom_chunks_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || chunk_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!keys || !values || !chunk_types || !chunk_data || !chunk_data_sizes || !chunk_after_idat) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    std::vector<PngCustomChunkSpec> chunks;
    try {
        chunks.reserve(chunk_count);
        for (std::size_t i = 0; i < chunk_count; ++i) {
            if (chunk_after_idat[i] != 0 && chunk_after_idat[i] != 1) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            chunks.push_back(PngCustomChunkSpec{
                reinterpret_cast<const char*>(chunk_types + i * 4u),
                chunk_data[i],
                chunk_data_sizes[i],
                chunk_after_idat[i] != 0});
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    const int validation_status = validate_png_custom_chunks(chunks.data(), chunks.size());
    if (validation_status != PILLOW_C_OK) {
        return validation_status;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        nullptr,
        nullptr,
        0u,
        false,
        false,
        compress_level,
        chunks.data(),
        chunks.size(),
        true);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_image_with_text_entries_custom_chunks_kind_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || chunk_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!keys || !values || !kinds || !compressed || !langs || !translated_keys ||
        !chunk_types || !chunk_data || !chunk_data_sizes || !chunk_after_idat) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }

    std::vector<PngCustomChunkSpec> chunks;
    try {
        chunks.reserve(chunk_count);
        for (std::size_t i = 0; i < chunk_count; ++i) {
            if (chunk_after_idat[i] != 0 && chunk_after_idat[i] != 1) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            chunks.push_back(PngCustomChunkSpec{
                reinterpret_cast<const char*>(chunk_types + i * 4u),
                chunk_data[i],
                chunk_data_sizes[i],
                chunk_after_idat[i] != 0});
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    const int validation_status = validate_png_custom_chunks(chunks.data(), chunks.size());
    if (validation_status != PILLOW_C_OK) {
        return validation_status;
    }

    std::vector<std::uint8_t> png;
    const int status = encode_png_custom_image_with_text_entries(
        image,
        has_dpi,
        dpi_x,
        dpi_y,
        false,
        0,
        nullptr,
        0u,
        false,
        0,
        0,
        0,
        &png,
        keys,
        values,
        text_count,
        compressed,
        kinds,
        langs,
        translated_keys,
        nullptr,
        0u,
        nullptr,
        0u,
        false,
        false,
        false,
        0u,
        nullptr,
        nullptr,
        0u,
        false,
        false,
        compress_level,
        chunks.data(),
        chunks.size(),
        true);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (!write_binary_file(path, png)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int save_png_image_with_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int has_transparency,
    int transparency)
{
    if (has_transparency != 0 && has_transparency != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (has_transparency) {
        return save_png_custom_image_with_transparency_options(
            image,
            path,
            has_dpi,
            dpi_x,
            dpi_y,
            true,
            transparency);
    }
    return save_png_image_with_options(image, path, compress_level, dpi_x, dpi_y);
}

int save_png_image_with_transparency_table_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size)
{
    if (!transparency_table && transparency_table_size > 0u) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (transparency_table_size == 0u) {
        return save_png_image_with_options(image, path, compress_level, dpi_x, dpi_y);
    }
    return save_png_custom_image_with_transparency_table_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        transparency_table,
        transparency_table_size);
}

int save_png_image_with_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (has_transparency != 0 && has_transparency != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    if (has_transparency) {
        return save_png_custom_image_with_rgb_transparency_options(
            image,
            path,
            has_dpi,
            dpi_x,
            dpi_y,
            true,
            transparency_r,
            transparency_g,
            transparency_b);
    }
    return save_png_image_with_options(image, path, compress_level, dpi_x, dpi_y);
}

int save_png_image_with_rgb_transparency_bytes_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* transparency,
    std::size_t transparency_size)
{
    if (!transparency && transparency_size > 0u) {
        return PILLOW_C_NULL_POINTER;
    }
    if (transparency_size != 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_png_image_with_rgb_transparency_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        1,
        transparency[0],
        transparency[1],
        transparency[2]);
}

int save_png_image_with_text_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* key,
    const char* value)
{
    if (!key || !value) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        key,
        value);
}

int save_png_image_with_text_entries_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count)
{
    if (!keys || !values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count);
}

int save_png_image_with_text_entries_value_sizes_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const std::size_t* value_sizes,
    const int* compressed,
    std::size_t text_count)
{
    if (!keys || !values || !value_sizes || !compressed) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_value_sizes_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        compress_level,
        keys,
        values,
        value_sizes,
        compressed,
        text_count);
}

int save_png_image_with_text_entries_chunk_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size)
{
    if (!keys || !values || !chunk_type || !chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || chunk_data_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_chunk_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count,
        chunk_type,
        chunk_data,
        chunk_data_size);
}

int save_png_image_with_text_entries_chunk_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    int has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!keys || !values || !chunk_type || !chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || chunk_data_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (has_transparency != 0 && has_transparency != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_chunk_rgb_transparency_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count,
        chunk_type,
        chunk_data,
        chunk_data_size,
        has_transparency != 0,
        transparency_r,
        transparency_g,
        transparency_b);
}

int save_png_image_with_chunk_icc_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!chunk_type || !chunk_data || !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_data_size == 0u || icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_chunk_icc_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        icc_profile,
        icc_profile_size);
}

int save_png_image_with_chunk_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!chunk_type || !chunk_data || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_data_size == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_chunk_exif_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        exif,
        exif_size);
}

int save_png_image_with_chunk_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    int has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    if (!chunk_type || !chunk_data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (chunk_data_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (has_transparency != 0 && has_transparency != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_chunk_rgb_transparency_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        has_transparency != 0,
        transparency_r,
        transparency_g,
        transparency_b);
}

int save_png_image_with_chunk_rgb_transparency_bytes_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* transparency,
    std::size_t transparency_size)
{
    if (!transparency && transparency_size > 0u) {
        return PILLOW_C_NULL_POINTER;
    }
    if (transparency_size != 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_png_image_with_chunk_rgb_transparency_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        1,
        transparency[0],
        transparency[1],
        transparency[2]);
}

int save_png_image_with_text_entries_ex_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* compressed,
    std::size_t text_count)
{
    if (!keys || !values || !compressed) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_ex_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        compressed,
        text_count);
}

int save_png_image_with_text_entries_kind_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count)
{
    if (!keys || !values || !kinds || !compressed) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_kind_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count);
}

int save_png_image_with_text_entries_itxt_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count)
{
    if (!keys || !values || !kinds || !compressed || !langs || !translated_keys) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_itxt_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        langs,
        translated_keys,
        text_count);
}

int save_png_image_with_text_entries_icc_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!keys || !values || !kinds || !compressed || !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_icc_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count,
        icc_profile,
        icc_profile_size);
}

int save_png_image_with_text_entries_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!keys || !values || !kinds || !compressed || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_exif_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count,
        exif,
        exif_size);
}

int save_png_image_with_text_entries_icc_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!keys || !values || !kinds || !compressed || !icc_profile || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (text_count == 0u || icc_profile_size == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_text_entries_icc_exif_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size);
}

int save_png_image_with_icc_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_icc_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        icc_profile,
        icc_profile_size);
}

int save_png_image_with_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_exif_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        exif,
        exif_size);
}

int save_png_image_with_icc_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    if (!icc_profile || !exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size == 0u || exif_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (compress_level < -1 || compress_level > 9) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_dpi = dpi_x > 0.0 || dpi_y > 0.0;
    if (has_dpi) {
        std::uint32_t unused = 0;
        if (!png_dpi_to_pixels_per_meter(dpi_x, &unused) ||
            !png_dpi_to_pixels_per_meter(dpi_y, &unused)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return save_png_custom_image_with_icc_exif_options(
        image,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size);
}

} // namespace

int pillow_c_png_decode_memory(
    const std::uint8_t* data,
    std::size_t size,
    PillowCImage** out_image)
{
    if (!data || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        PngHeaderInfo header_info = {};
        if (!read_png_header_info_memory(data, size, &header_info)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        std::uint8_t png_grayscale_transparency = 0;
        const bool has_grayscale_transparency =
            header_info.color_type == 0 &&
            read_png_grayscale_transparency_memory(data, size, &png_grayscale_transparency);
        std::vector<std::uint8_t> png_decode_bytes;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICStream> decoder_stream;
        HRESULT hr = S_OK;
        if (copy_png_without_wic_sensitive_chunks_memory(
                data, size, has_grayscale_transparency, true, &png_decode_bytes)) {
            if (png_decode_bytes.size() > static_cast<std::size_t>(UINT_MAX)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = factory->CreateStream(decoder_stream.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = decoder_stream->InitializeFromMemory(
                png_decode_bytes.data(),
                static_cast<DWORD>(png_decode_bytes.size()));
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = factory->CreateDecoderFromStream(
                decoder_stream.get(),
                nullptr,
                WICDecodeMetadataCacheOnDemand,
                decoder.put());
        } else {
            if (size > static_cast<std::size_t>(UINT_MAX)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = factory->CreateStream(decoder_stream.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = decoder_stream->InitializeFromMemory(
                static_cast<WICInProcPointer>(const_cast<std::uint8_t*>(data)),
                static_cast<DWORD>(size));
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = factory->CreateDecoderFromStream(
                decoder_stream.get(),
                nullptr,
                WICDecodeMetadataCacheOnDemand,
                decoder.put());
        }
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatPng)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, frame.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT width_u = 0;
        UINT height_u = 0;
        hr = frame->GetSize(&width_u, &height_u);
        if (FAILED(hr) || width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        int mode = 0;
        int channels = 0;
        int decoded_channels = 0;
        WICPixelFormatGUID target_format = {};
        std::vector<std::uint8_t> png_palette_rgb;
        std::vector<std::uint8_t> png_palette_alpha;
        std::vector<std::uint8_t> png_transparency_table;
        int png_transparency = -1;
        const bool has_palette_transparency =
            header_info.color_type == 3 &&
            read_png_palette_transparency_memory(
                data,
                size,
                &png_palette_rgb,
                &png_palette_alpha,
                &png_transparency_table,
                &png_transparency);
        std::uint8_t png_rgb_transparency[3] = {0, 0, 0};
        const bool has_rgb_transparency =
            header_info.color_type == 2 &&
            read_png_rgb_transparency_memory(data, size, png_rgb_transparency);
        if (header_info.color_type == 0 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
            decoded_channels = 1;
            target_format = GUID_WICPixelFormat8bppGray;
        } else if (header_info.color_type == 4 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_LA;
            channels = 2;
            decoded_channels = 4;
            target_format = GUID_WICPixelFormat32bppRGBA;
        } else if (header_info.color_type == 2 && header_info.bit_depth == 8) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
            decoded_channels = 3;
            target_format = GUID_WICPixelFormat24bppRGB;
        } else if (header_info.color_type == 3 && header_info.bit_depth <= 8) {
            mode = PILLOW_C_MODE_P;
            channels = 1;
            decoded_channels = has_palette_transparency ? 4 : 1;
            target_format = has_palette_transparency ? GUID_WICPixelFormat32bppRGBA : GUID_WICPixelFormat8bppIndexed;
        } else {
            status = wic_format_to_mode(source_format, &mode, &channels, &target_format);
            if (status != PILLOW_C_OK) {
                return status;
            }
            decoded_channels = channels;
        }

        std::size_t stride = 0;
        std::size_t storage_size = 0;
        if (!checked_image_size(width, height, channels, &stride, &storage_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t decoded_stride = 0;
        std::size_t decoded_size = 0;
        if (!checked_image_size(width, height, decoded_channels, &decoded_stride, &decoded_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> palette_rgb;
        ComPtr<IWICPalette> source_palette;
        IWICPalette* converter_palette = nullptr;
        if (mode == PILLOW_C_MODE_P) {
            status = copy_wic_palette_rgb(frame.get(), factory.get(), &palette_rgb);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (has_palette_transparency) {
                palette_rgb = png_palette_rgb;
            }
            HRESULT palette_hr = factory->CreatePalette(source_palette.put());
            if (FAILED(palette_hr) || FAILED(frame->CopyPalette(source_palette.get()))) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            converter_palette = source_palette.get();
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, target_format)) {
            source.reset(frame.get());
            source.get()->AddRef();
        } else {
            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                target_format,
                WICBitmapDitherTypeNone,
                converter_palette,
                0.0,
                mode == PILLOW_C_MODE_P ? WICBitmapPaletteTypeCustom : WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(storage_size)};
        std::vector<std::uint8_t> decoded;
        std::uint8_t* copy_target = image->pixels.data();
        UINT copy_stride = static_cast<UINT>(stride);
        UINT copy_size = static_cast<UINT>(image->pixels.size());
        if (decoded_channels != channels) {
            decoded.assign(decoded_size, std::uint8_t{0});
            copy_target = decoded.data();
            copy_stride = static_cast<UINT>(decoded_stride);
            copy_size = static_cast<UINT>(decoded.size());
        }
        hr = source->CopyPixels(
            nullptr,
            copy_stride,
            copy_size,
            copy_target);
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (mode == PILLOW_C_MODE_LA) {
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = decoded.data() + static_cast<std::size_t>(y) * decoded_stride;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < width; ++x) {
                    const std::size_t src = static_cast<std::size_t>(x) * 4u;
                    const std::size_t dst = static_cast<std::size_t>(x) * 2u;
                    dst_row[dst + 0u] = src_row[src + 0u];
                    dst_row[dst + 1u] = src_row[src + 3u];
                }
            }
        } else if (mode == PILLOW_C_MODE_P && has_palette_transparency) {
            status = remap_png_rgba_to_palette_indices(
                decoded,
                width,
                height,
                decoded_stride,
                png_palette_rgb,
                png_palette_alpha,
                image);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
        }
        if (mode == PILLOW_C_MODE_P) {
            image->palette_rgb = std::move(palette_rgb);
            if (has_palette_transparency) {
                image->palette_alpha = std::move(png_palette_alpha);
                image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_RGBA;
                if (png_transparency >= 0) {
                    image->has_png_transparency = true;
                    image->png_transparency = png_transparency;
                } else if (!png_transparency_table.empty()) {
                    image->png_transparency_table = std::move(png_transparency_table);
                }
            } else {
                image->palette_alpha.clear();
                image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
            }
        }
        if (has_rgb_transparency) {
            image->has_png_rgb_transparency = true;
            image->png_rgb_transparency[0] = png_rgb_transparency[0];
            image->png_rgb_transparency[1] = png_rgb_transparency[1];
            image->png_rgb_transparency[2] = png_rgb_transparency[2];
        }
        if (has_grayscale_transparency) {
            image->has_png_transparency = true;
            image->png_transparency = png_grayscale_transparency;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

bool pillow_c_inflate_zlib_deflate(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<std::uint8_t>* out,
    std::size_t max_output,
    bool* exceeded)
{
    return inflate_zlib_deflate(data, size, out, max_output, exceeded);
}

int pillow_c_append_zlib_stored(
    std::vector<std::uint8_t>& out,
    const std::vector<std::uint8_t>& raw,
    std::uint8_t flags)
{
    return append_zlib_stored(out, raw, flags);
}

int pillow_c_png_custom_mode_spec(
    const PillowCImage* image,
    int* color_type,
    int* payload_channels)
{
    return png_custom_mode_spec(image, color_type, payload_channels);
}

int pillow_c_png_encode_custom_image(
    const PillowCImage* image,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    bool has_transparency,
    int transparency,
    bool has_rgb_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b,
    std::vector<std::uint8_t>* out_png,
    const char* text_key,
    const char* text_value,
    bool optimize)
{
    return encode_png_custom_image(
        image, has_dpi, dpi_x, dpi_y, has_transparency, transparency,
        has_rgb_transparency, transparency_r, transparency_g, transparency_b,
        out_png, text_key, text_value, optimize);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_png(
    const char* path,
    PillowCImage** out_image)
{
    return open_png_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png(
    const PillowCImage* image,
    const char* path)
{
    return save_png_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_compress_level(
    const PillowCImage* image,
    const char* path,
    int compress_level)
{
    return save_png_image_with_compress_level(image, path, compress_level);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y)
{
    return save_png_image_with_options(image, path, compress_level, dpi_x, dpi_y);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int has_transparency,
    int transparency)
{
    return save_png_image_with_transparency_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        has_transparency,
        transparency);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_transparency_table_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size)
{
    return save_png_image_with_transparency_table_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        transparency_table,
        transparency_table_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    return save_png_image_with_rgb_transparency_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        has_transparency,
        transparency_r,
        transparency_g,
        transparency_b);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_rgb_transparency_bytes_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* transparency,
    std::size_t transparency_size)
{
    return save_png_image_with_rgb_transparency_bytes_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        transparency,
        transparency_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* key,
    const char* value)
{
    return save_png_image_with_text_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        key,
        value);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count)
{
    return save_png_image_with_text_entries_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_value_sizes_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const std::size_t* value_sizes,
    const int* compressed,
    std::size_t text_count)
{
    return save_png_image_with_text_entries_value_sizes_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        value_sizes,
        compressed,
        text_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_chunk_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size)
{
    return save_png_image_with_text_entries_chunk_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count,
        chunk_type,
        chunk_data,
        chunk_data_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_chunk_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    int has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    return save_png_image_with_text_entries_chunk_rgb_transparency_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count,
        chunk_type,
        chunk_data,
        chunk_data_size,
        has_transparency,
        transparency_r,
        transparency_g,
        transparency_b);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_chunk_icc_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    return save_png_image_with_chunk_icc_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        icc_profile,
        icc_profile_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_chunk_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    return save_png_image_with_chunk_exif_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        exif,
        exif_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_chunk_rgb_transparency_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    int has_transparency,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    return save_png_image_with_chunk_rgb_transparency_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        has_transparency,
        transparency_r,
        transparency_g,
        transparency_b);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_chunk_rgb_transparency_bytes_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    const std::uint8_t* transparency,
    std::size_t transparency_size)
{
    return save_png_image_with_chunk_rgb_transparency_bytes_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size,
        transparency,
        transparency_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_ex_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* compressed,
    std::size_t text_count)
{
    return save_png_image_with_text_entries_ex_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        compressed,
        text_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_kind_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count)
{
    return save_png_image_with_text_entries_kind_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_itxt_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count)
{
    return save_png_image_with_text_entries_itxt_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        langs,
        translated_keys,
        text_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_icc_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    return save_png_image_with_text_entries_icc_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count,
        icc_profile,
        icc_profile_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    return save_png_image_with_text_entries_exif_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count,
        exif,
        exif_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_icc_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    return save_png_image_with_text_entries_icc_exif_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        text_count,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_icc_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    return save_png_image_with_icc_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        icc_profile,
        icc_profile_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    return save_png_image_with_exif_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        exif,
        exif_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_icc_exif_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size)
{
    return save_png_image_with_icc_exif_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_interlace_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int interlace)
{
    return save_png_image_with_interlace_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        interlace);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_gamma_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    double gamma)
{
    return save_png_image_with_gamma_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        gamma);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_gama_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    std::uint32_t gama_raw)
{
    return save_png_image_with_gama_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        gama_raw);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_chunk_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size)
{
    return save_png_image_with_chunk_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_type,
        chunk_data,
        chunk_data_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_optimize_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    int optimize)
{
    return save_png_image_with_optimize_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        optimize);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_metadata_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* chunk_type,
    const std::uint8_t* chunk_data,
    std::size_t chunk_data_size,
    std::uint32_t flags,
    std::uint32_t gama_raw,
    int transparency,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    return save_png_image_with_metadata_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        langs,
        translated_keys,
        text_count,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size,
        chunk_type,
        chunk_data,
        chunk_data_size,
        flags,
        gama_raw,
        transparency,
        transparency_table,
        transparency_table_size,
        transparency_r,
        transparency_g,
        transparency_b);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_metadata_custom_chunks_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* exif,
    std::size_t exif_size,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count,
    std::uint32_t flags,
    std::uint32_t gama_raw,
    int transparency,
    const std::uint8_t* transparency_table,
    std::size_t transparency_table_size,
    int transparency_r,
    int transparency_g,
    int transparency_b)
{
    return save_png_image_with_metadata_custom_chunks_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        langs,
        translated_keys,
        text_count,
        icc_profile,
        icc_profile_size,
        exif,
        exif_size,
        chunk_types,
        chunk_data,
        chunk_data_sizes,
        chunk_after_idat,
        chunk_count,
        flags,
        gama_raw,
        transparency,
        transparency_table,
        transparency_table_size,
        transparency_r,
        transparency_g,
        transparency_b);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_custom_chunks_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count)
{
    return save_png_image_with_custom_chunks_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        chunk_types,
        chunk_data,
        chunk_data_sizes,
        chunk_after_idat,
        chunk_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_custom_chunks_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    std::size_t text_count,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count)
{
    return save_png_image_with_text_entries_custom_chunks_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        text_count,
        chunk_types,
        chunk_data,
        chunk_data_sizes,
        chunk_after_idat,
        chunk_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_png_text_entries_custom_chunks_kind_options(
    const PillowCImage* image,
    const char* path,
    int compress_level,
    double dpi_x,
    double dpi_y,
    const char* const* keys,
    const char* const* values,
    const int* kinds,
    const int* compressed,
    const char* const* langs,
    const char* const* translated_keys,
    std::size_t text_count,
    const std::uint8_t* chunk_types,
    const std::uint8_t* const* chunk_data,
    const std::size_t* chunk_data_sizes,
    const int* chunk_after_idat,
    std::size_t chunk_count)
{
    return save_png_image_with_text_entries_custom_chunks_kind_options(
        image,
        path,
        compress_level,
        dpi_x,
        dpi_y,
        keys,
        values,
        kinds,
        compressed,
        langs,
        translated_keys,
        text_count,
        chunk_types,
        chunk_data,
        chunk_data_sizes,
        chunk_after_idat,
        chunk_count);
}
