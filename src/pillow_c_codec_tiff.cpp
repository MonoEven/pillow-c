#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pillow_c_internal.h"
#include "pillow_c_wic_internal.h"

namespace {

constexpr std::uint16_t TIFF_COMPRESSION_NONE = 1u;
constexpr std::uint16_t TIFF_COMPRESSION_LZW = 5u;
constexpr std::uint16_t TIFF_COMPRESSION_ADOBE_DEFLATE = 8u;
constexpr std::uint16_t TIFF_COMPRESSION_PACKBITS = 32773u;
std::uint16_t read_tiff16(const std::uint8_t* data, bool little_endian)
{
    return little_endian ? read_le16(data) : read_be16(data);
}

std::uint32_t read_tiff32(const std::uint8_t* data, bool little_endian)
{
    return little_endian ? read_le32(data) : read_be32(data);
}

std::uint64_t read_tiff64(const std::uint8_t* data, bool little_endian)
{
    std::uint64_t value = 0u;
    if (little_endian) {
        for (int index = 7; index >= 0; --index) {
            value = (value << 8u) | static_cast<std::uint64_t>(data[index]);
        }
    } else {
        for (int index = 0; index < 8; ++index) {
            value = (value << 8u) | static_cast<std::uint64_t>(data[index]);
        }
    }
    return value;
}

bool parse_tiff_bigtiff_header(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool* out_little_endian,
    std::uint64_t* out_first_ifd_offset)
{
    if (!tiff || !out_little_endian || !out_first_ifd_offset || tiff_size < 16u) {
        return false;
    }
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) ||
        read_tiff16(tiff + 2u, little_endian) != 43u ||
        read_tiff16(tiff + 4u, little_endian) != 8u ||
        read_tiff16(tiff + 6u, little_endian) != 0u) {
        return false;
    }
    *out_little_endian = little_endian;
    *out_first_ifd_offset = read_tiff64(tiff + 8u, little_endian);
    return true;
}

int parse_tiff_orientation(const std::uint8_t* tiff, std::size_t tiff_size)
{
    if (!tiff || tiff_size < 8u) {
        return 0;
    }
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || read_tiff16(tiff + 2u, little_endian) != 42u) {
        return 0;
    }

    const std::uint32_t ifd_offset = read_tiff32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return 0;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = read_tiff16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return 0;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (tag == 0x0112u && type == 3u && count == 1u) {
            const std::uint16_t value = read_tiff16(entry + 8u, little_endian);
            return value >= 1u && value <= 8u ? static_cast<int>(value) : 0;
        }
    }
    return 0;
}

bool locate_tiff_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* out_little_endian,
    std::uint16_t* out_entry_count,
    std::size_t* out_entries_offset);

struct TiffResolutionMetadata {
    bool has_dpi = false;
    double dpi_x = 0.0;
    double dpi_y = 0.0;
};

bool parse_tiff_rational(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    std::uint32_t offset,
    double* out_value)
{
    if (!tiff || !out_value || offset > tiff_size || tiff_size - offset < 8u) {
        return false;
    }
    const std::uint32_t numerator = read_tiff32(tiff + offset, little_endian);
    const std::uint32_t denominator = read_tiff32(tiff + offset + 4u, little_endian);
    if (denominator == 0u) {
        return false;
    }
    *out_value = static_cast<double>(numerator) / static_cast<double>(denominator);
    return true;
}

bool parse_tiff_resolution_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    TiffResolutionMetadata* out_metadata)
{
    if (!out_metadata) {
        return false;
    }
    *out_metadata = TiffResolutionMetadata{};
    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset)) {
        return false;
    }

    double x_resolution = 0.0;
    double y_resolution = 0.0;
    int resolution_unit = 0;
    bool has_x_resolution = false;
    bool has_y_resolution = false;
    bool has_resolution_unit = false;
    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if ((tag == 0x011au || tag == 0x011bu) && type == 5u && count == 1u) {
            double value = 0.0;
            if (parse_tiff_rational(tiff, tiff_size, little_endian, read_tiff32(entry + 8u, little_endian), &value)) {
                if (tag == 0x011au) {
                    x_resolution = value;
                    has_x_resolution = true;
                } else {
                    y_resolution = value;
                    has_y_resolution = true;
                }
            }
        } else if (tag == 0x0128u && type == 3u && count == 1u) {
            resolution_unit = static_cast<int>(read_tiff16(entry + 8u, little_endian));
            has_resolution_unit = true;
        }
    }

    if (!has_x_resolution || !has_y_resolution || !has_resolution_unit ||
        resolution_unit != 2 || x_resolution <= 0.0 || y_resolution <= 0.0) {
        return false;
    }
    out_metadata->has_dpi = true;
    out_metadata->dpi_x = x_resolution;
    out_metadata->dpi_y = y_resolution;
    return true;
}

bool parse_tiff_palette_rgb(const std::uint8_t* tiff, std::size_t tiff_size, std::vector<std::uint8_t>* out_palette)
{
    if (!out_palette) {
        return false;
    }
    out_palette->clear();
    if (!tiff || tiff_size < 8u) {
        return false;
    }
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || read_tiff16(tiff + 2u, little_endian) != 42u) {
        return false;
    }

    const std::uint32_t ifd_offset = read_tiff32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return false;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = read_tiff16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return false;
    }

    std::uint32_t color_map_offset = 0u;
    bool has_color_map = false;
    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (tag == 0x0140u && type == 3u && count == 768u) {
            color_map_offset = read_tiff32(entry + 8u, little_endian);
            has_color_map = true;
            break;
        }
    }
    if (!has_color_map) {
        return false;
    }
    if (color_map_offset > tiff_size || tiff_size - color_map_offset < 256u * 3u * 2u) {
        return false;
    }

    out_palette->assign(256u * 3u, std::uint8_t{0});
    for (std::size_t index = 0; index < 256u; ++index) {
        for (std::size_t channel = 0; channel < 3u; ++channel) {
            const std::size_t plane_index = channel * 256u + index;
            const std::uint16_t value = read_tiff16(tiff + color_map_offset + plane_index * 2u, little_endian);
            (*out_palette)[index * 3u + channel] = static_cast<std::uint8_t>(value >> 8);
        }
    }
    return true;
}

bool parse_tiff_is_la_mode(const std::uint8_t* tiff, std::size_t tiff_size)
{
    if (!tiff || tiff_size < 8u) {
        return false;
    }
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || read_tiff16(tiff + 2u, little_endian) != 42u) {
        return false;
    }

    const std::uint32_t ifd_offset = read_tiff32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return false;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = read_tiff16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return false;
    }

    bool has_bits = false;
    bool has_photometric = false;
    bool has_samples = false;
    bool has_extra_samples = false;
    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (type != 3u) {
            continue;
        }
        if (tag == 258u && count == 2u) {
            const std::uint16_t first = read_tiff16(entry + 8u, little_endian);
            const std::uint16_t second = read_tiff16(entry + 10u, little_endian);
            has_bits = first == 8u && second == 8u;
        } else if (tag == 262u && count == 1u) {
            has_photometric = read_tiff16(entry + 8u, little_endian) == 1u;
        } else if (tag == 277u && count == 1u) {
            has_samples = read_tiff16(entry + 8u, little_endian) == 2u;
        } else if (tag == 338u && count == 1u) {
            has_extra_samples = read_tiff16(entry + 8u, little_endian) == 2u;
        }
    }
    return has_bits && has_photometric && has_samples && has_extra_samples;
}

bool tiff_packbits_decode_strip(
    const std::uint8_t* data,
    std::size_t size,
    std::size_t expected_size,
    std::vector<std::uint8_t>* out)
{
    if (!data || !out || expected_size == 0u) {
        return false;
    }
    out->clear();
    out->reserve(expected_size);
    std::size_t pos = 0;
    while (pos < size && out->size() < expected_size) {
        const std::int8_t control = static_cast<std::int8_t>(data[pos++]);
        if (control >= 0) {
            const std::size_t count = static_cast<std::size_t>(control) + 1u;
            if (count > size - pos || count > expected_size - out->size()) {
                return false;
            }
            out->insert(out->end(), data + pos, data + pos + count);
            pos += count;
        } else if (control >= -127) {
            const std::size_t count = static_cast<std::size_t>(1 - static_cast<int>(control));
            if (pos >= size || count > expected_size - out->size()) {
                return false;
            }
            out->insert(out->end(), count, data[pos++]);
        }
    }
    return out->size() == expected_size;
}

struct TiffMsbBitReader {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t bit_pos = 0;

    bool read(int bit_count, int* out_code)
    {
        if (!data || !out_code || bit_count <= 0 || bit_count > 12) {
            return false;
        }
        if (static_cast<std::size_t>(bit_count) > size * 8u - bit_pos) {
            return false;
        }
        int code = 0;
        for (int bit = 0; bit < bit_count; ++bit) {
            const std::uint8_t value = data[bit_pos / 8u];
            code = (code << 1) | ((value >> (7u - (bit_pos % 8u))) & 1u);
            ++bit_pos;
        }
        *out_code = code;
        return true;
    }
};

bool tiff_lzw_decode_strip(
    const std::uint8_t* data,
    std::size_t size,
    std::size_t expected_size,
    std::vector<std::uint8_t>* out)
{
    if (!data || !out || size == 0u || expected_size == 0u) {
        return false;
    }
    out->clear();
    out->reserve(expected_size);

    constexpr int clear_code = 256;
    constexpr int end_code = 257;
    int next_code = 258;
    int code_size = 9;
    std::vector<std::vector<std::uint8_t>> dictionary(4096);

    auto reset_dictionary = [&]() {
        for (auto& entry : dictionary) {
            entry.clear();
        }
        for (int code = 0; code < 256; ++code) {
            dictionary[static_cast<std::size_t>(code)] = {static_cast<std::uint8_t>(code)};
        }
        next_code = 258;
        code_size = 9;
    };

    reset_dictionary();
    TiffMsbBitReader reader{data, size, 0};
    std::vector<std::uint8_t> previous;
    bool have_previous = false;

    while (out->size() < expected_size) {
        int code = 0;
        if (!reader.read(code_size, &code)) {
            return false;
        }
        if (code == clear_code) {
            reset_dictionary();
            previous.clear();
            have_previous = false;
            continue;
        }
        if (code == end_code) {
            break;
        }

        std::vector<std::uint8_t> entry;
        if (code >= 0 && code < next_code && !dictionary[static_cast<std::size_t>(code)].empty()) {
            entry = dictionary[static_cast<std::size_t>(code)];
        } else if (code == next_code && have_previous && !previous.empty()) {
            entry = previous;
            entry.push_back(previous.front());
        } else {
            return false;
        }
        if (entry.size() > expected_size - out->size()) {
            return false;
        }
        out->insert(out->end(), entry.begin(), entry.end());

        if (have_previous && next_code < 4096 && !previous.empty() && !entry.empty()) {
            std::vector<std::uint8_t> next_entry = previous;
            next_entry.push_back(entry.front());
            dictionary[static_cast<std::size_t>(next_code++)] = std::move(next_entry);
            if (next_code == ((1 << code_size) - 1) && code_size < 12) {
                ++code_size;
            }
        }
        previous = std::move(entry);
        have_previous = true;
    }
    return out->size() == expected_size;
}

bool tiff_decode_tiled_payload(
    const std::uint8_t* source,
    std::size_t source_size,
    std::uint16_t compression,
    std::size_t expected_size,
    std::vector<std::uint8_t>* decoded)
{
    if (!source || !decoded || expected_size == 0u) {
        return false;
    }
    if (compression == TIFF_COMPRESSION_NONE) {
        return source_size >= expected_size;
    }
    if (compression == TIFF_COMPRESSION_PACKBITS) {
        return tiff_packbits_decode_strip(source, source_size, expected_size, decoded);
    }
    if (compression == TIFF_COMPRESSION_LZW) {
        return tiff_lzw_decode_strip(source, source_size, expected_size, decoded);
    }
    if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
        return pillow_c_inflate_zlib_deflate(source, source_size, decoded, expected_size) &&
            decoded->size() == expected_size;
    }
    return false;
}

bool locate_tiff_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* out_little_endian,
    std::uint16_t* out_entry_count,
    std::size_t* out_entries_offset);

int parse_tiff_chunky_image_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* recognized,
    bool* tiled_storage,
    PillowCImage** out_image)
{
    if (!recognized || !tiled_storage || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *recognized = false;
    *tiled_storage = false;
    *out_image = nullptr;

    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(tiff, tiff_size, ifd_index, &little_endian, &entry_count, &entries_offset)) {
        return PILLOW_C_OK;
    }

    auto read_scalar = [little_endian](const std::uint8_t* entry, std::uint32_t* out_value) -> bool {
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (!out_value || count != 1u) {
            return false;
        }
        if (type == 3u) {
            *out_value = read_tiff16(entry + 8u, little_endian);
            return true;
        }
        if (type == 4u) {
            *out_value = read_tiff32(entry + 8u, little_endian);
            return true;
        }
        return false;
    };
    auto read_long_array = [tiff, tiff_size, little_endian](
        const std::uint8_t* entry,
        std::vector<std::uint32_t>* out_values) -> bool {
        if (!entry || !out_values) {
            return false;
        }
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (type != 4u || count == 0u) {
            return false;
        }
        out_values->clear();
        try {
            out_values->resize(count);
        } catch (const std::bad_alloc&) {
            return false;
        }
        if (count == 1u) {
            (*out_values)[0] = read_tiff32(entry + 8u, little_endian);
            return true;
        }
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        const std::size_t value_size = static_cast<std::size_t>(count) * 4u;
        if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
            out_values->clear();
            return false;
        }
        for (std::uint32_t index = 0u; index < count; ++index) {
            (*out_values)[index] = read_tiff32(
                tiff + static_cast<std::size_t>(value_offset) + static_cast<std::size_t>(index) * 4u,
                little_endian);
        }
        return true;
    };
    auto read_short_array = [tiff, tiff_size, little_endian](
        const std::uint8_t* entry,
        std::vector<std::uint32_t>* out_values) -> bool {
        if (!entry || !out_values) {
            return false;
        }
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (type != 3u || count == 0u ||
            static_cast<std::size_t>(count) > std::numeric_limits<std::size_t>::max() / 2u) {
            return false;
        }
        out_values->clear();
        try {
            out_values->resize(count);
        } catch (const std::bad_alloc&) {
            return false;
        }
        const std::size_t value_size = static_cast<std::size_t>(count) * 2u;
        if (value_size <= 4u) {
            for (std::uint32_t index = 0u; index < count; ++index) {
                (*out_values)[index] = read_tiff16(
                    entry + 8u + static_cast<std::size_t>(index) * 2u,
                    little_endian);
            }
            return true;
        }
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
            out_values->clear();
            return false;
        }
        for (std::uint32_t index = 0u; index < count; ++index) {
            (*out_values)[index] = read_tiff16(
                tiff + static_cast<std::size_t>(value_offset) + static_cast<std::size_t>(index) * 2u,
                little_endian);
        }
        return true;
    };

    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::vector<std::uint32_t> bits_per_sample;
    std::uint32_t compression = 0u;
    std::uint32_t photometric = 0u;
    std::uint32_t strip_offset = 0u;
    std::uint32_t rows_per_strip = 0u;
    std::uint32_t strip_byte_count = 0u;
    std::uint32_t planar_config = 1u;
    std::uint32_t samples_per_pixel = 1u;
    std::uint32_t extra_samples = 0u;
    std::uint32_t tile_width = 0u;
    std::uint32_t tile_length = 0u;
    std::vector<std::uint32_t> tile_offsets;
    std::vector<std::uint32_t> tile_byte_counts;
    bool has_width = false;
    bool has_height = false;
    bool has_bits = false;
    bool has_compression = false;
    bool has_photometric = false;
    bool has_strip_offset = false;
    bool has_rows_per_strip = false;
    bool has_strip_byte_count = false;
    bool has_tile_width = false;
    bool has_tile_length = false;
    bool has_samples_per_pixel = false;
    bool has_extra_samples = false;
    bool has_tile_offsets = false;
    bool has_tile_byte_counts = false;

    for (std::uint16_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        switch (tag) {
        case 256u:
            has_width = read_scalar(entry, &width);
            break;
        case 257u:
            has_height = read_scalar(entry, &height);
            break;
        case 258u:
            has_bits = read_short_array(entry, &bits_per_sample);
            break;
        case 259u:
            has_compression = read_scalar(entry, &compression);
            break;
        case 262u:
            has_photometric = read_scalar(entry, &photometric);
            break;
        case 273u:
            has_strip_offset = read_scalar(entry, &strip_offset);
            break;
        case 277u:
            has_samples_per_pixel = read_scalar(entry, &samples_per_pixel);
            break;
        case 278u:
            has_rows_per_strip = read_scalar(entry, &rows_per_strip);
            break;
        case 279u:
            has_strip_byte_count = read_scalar(entry, &strip_byte_count);
            break;
        case 284u:
            read_scalar(entry, &planar_config);
            break;
        case 322u:
            has_tile_width = read_scalar(entry, &tile_width);
            break;
        case 323u:
            has_tile_length = read_scalar(entry, &tile_length);
            break;
        case 324u:
            has_tile_offsets = read_long_array(entry, &tile_offsets);
            break;
        case 325u:
            has_tile_byte_counts = read_long_array(entry, &tile_byte_counts);
            break;
        case 338u:
            has_extra_samples = read_scalar(entry, &extra_samples);
            break;
        default:
            break;
        }
    }

    const bool has_valid_tile_shape =
        has_tile_width && has_tile_length && tile_width > 0u && tile_length > 0u;
    const bool has_valid_tile_byte_range_arrays =
        has_tile_offsets && has_tile_byte_counts &&
        tile_offsets.size() == tile_byte_counts.size() && !tile_offsets.empty();
    const std::uint64_t tile_columns = has_valid_tile_shape
        ? (static_cast<std::uint64_t>(width) + tile_width - 1u) / tile_width
        : 0u;
    const std::uint64_t tile_rows = has_valid_tile_shape
        ? (static_cast<std::uint64_t>(height) + tile_length - 1u) / tile_length
        : 0u;
    const std::uint64_t tile_planes = planar_config == 2u
        ? static_cast<std::uint64_t>(samples_per_pixel)
        : 1u;
    const bool has_expected_tile_count =
        has_valid_tile_shape && tile_columns > 0u && tile_rows > 0u &&
        tile_planes > 0u &&
        tile_columns <= std::numeric_limits<std::uint32_t>::max() / tile_rows &&
        tile_columns * tile_rows <= std::numeric_limits<std::uint32_t>::max() / tile_planes &&
        tile_offsets.size() == static_cast<std::size_t>(tile_columns * tile_rows * tile_planes) &&
        tile_byte_counts.size() == tile_offsets.size();
    const bool has_valid_tiled_storage =
        has_expected_tile_count && has_valid_tile_byte_range_arrays;
    const bool has_valid_strip_storage =
        has_strip_offset && has_rows_per_strip && has_strip_byte_count &&
        rows_per_strip == height;
    const bool has_supported_tiled_compression =
        compression == TIFF_COMPRESSION_NONE ||
        compression == TIFF_COMPRESSION_PACKBITS ||
        compression == TIFF_COMPRESSION_LZW ||
        compression == TIFF_COMPRESSION_ADOBE_DEFLATE;
    const bool has_supported_l_storage_compression =
        (has_valid_tiled_storage && has_supported_tiled_compression) ||
        (has_valid_strip_storage && compression == TIFF_COMPRESSION_NONE);
    const bool has_supported_tiled_planar_config =
        planar_config == 1u || planar_config == 2u;
    const bool matches_l_shape =
        has_width && has_height && has_bits && has_compression && has_photometric &&
        (has_valid_strip_storage || has_valid_tiled_storage) &&
        width > 0u && height > 0u &&
        bits_per_sample.size() == 1u && bits_per_sample[0] == 8u &&
        has_supported_l_storage_compression &&
        photometric == 1u &&
        (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        ((has_valid_tiled_storage && has_supported_tiled_planar_config) ||
            (has_valid_strip_storage && planar_config == 1u));
    const bool matches_rgb_tiled_shape =
        has_width && has_height && has_bits && has_compression && has_photometric &&
        has_valid_tiled_storage && has_samples_per_pixel &&
        width > 0u && height > 0u &&
        bits_per_sample.size() == 3u &&
        bits_per_sample[0] == 8u && bits_per_sample[1] == 8u && bits_per_sample[2] == 8u &&
        has_supported_tiled_compression &&
        photometric == 2u &&
        samples_per_pixel == 3u &&
        has_supported_tiled_planar_config;
    const bool matches_rgbx_tiled_shape =
        has_width && has_height && has_bits && has_compression && has_photometric &&
        has_valid_tiled_storage && has_samples_per_pixel && has_extra_samples &&
        width > 0u && height > 0u &&
        bits_per_sample.size() == 4u &&
        bits_per_sample[0] == 8u && bits_per_sample[1] == 8u &&
        bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        has_supported_tiled_compression &&
        photometric == 2u &&
        samples_per_pixel == 4u &&
        extra_samples == 0u &&
        planar_config == 1u;
    const bool matches_rgba_tiled_shape =
        has_width && has_height && has_bits && has_compression && has_photometric &&
        has_valid_tiled_storage && has_samples_per_pixel && has_extra_samples &&
        width > 0u && height > 0u &&
        bits_per_sample.size() == 4u &&
        bits_per_sample[0] == 8u && bits_per_sample[1] == 8u &&
        bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        has_supported_tiled_compression &&
        photometric == 2u &&
        samples_per_pixel == 4u &&
        extra_samples == 2u &&
        has_supported_tiled_planar_config;
    const bool matches_la_tiled_shape =
        has_width && has_height && has_bits && has_compression && has_photometric &&
        has_valid_tiled_storage && has_samples_per_pixel && has_extra_samples &&
        width > 0u && height > 0u &&
        bits_per_sample.size() == 2u &&
        bits_per_sample[0] == 8u && bits_per_sample[1] == 8u &&
        has_supported_tiled_compression &&
        photometric == 1u &&
        samples_per_pixel == 2u &&
        extra_samples == 2u &&
        has_supported_tiled_planar_config;
    if (!matches_l_shape && !matches_rgb_tiled_shape &&
        !matches_rgbx_tiled_shape && !matches_rgba_tiled_shape &&
        !matches_la_tiled_shape) {
        return PILLOW_C_OK;
    }
    *recognized = true;
    *tiled_storage = has_valid_tiled_storage;
    const int mode = matches_rgba_tiled_shape
        ? PILLOW_C_MODE_RGBA
        : (matches_la_tiled_shape
            ? PILLOW_C_MODE_LA
            : (matches_rgb_tiled_shape || matches_rgbx_tiled_shape
                ? PILLOW_C_MODE_RGB
                : PILLOW_C_MODE_L));
    const int channels = matches_rgba_tiled_shape
        ? 4
        : (matches_la_tiled_shape || matches_rgb_tiled_shape || matches_rgbx_tiled_shape
            ? (matches_la_tiled_shape ? 2 : 3)
            : 1);
    const int tile_channels = matches_rgba_tiled_shape || matches_rgbx_tiled_shape
        ? (planar_config == 2u ? 1 : 4)
        : (planar_config == 2u ? 1 : channels);

    if (width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0u;
    std::size_t size = 0u;
    if (!checked_image_size(static_cast<int>(width), static_cast<int>(height), channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_valid_strip_bytes =
        has_valid_strip_storage &&
        strip_offset <= tiff_size &&
        static_cast<std::size_t>(strip_byte_count) <= tiff_size - static_cast<std::size_t>(strip_offset) &&
        static_cast<std::size_t>(strip_byte_count) == size;
    if (!has_valid_tiled_storage && !has_valid_strip_bytes) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            static_cast<int>(width),
            static_cast<int>(height),
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        if (has_valid_tiled_storage) {
            const std::size_t tile_width_size = static_cast<std::size_t>(tile_width);
            const std::size_t tile_length_size = static_cast<std::size_t>(tile_length);
            if (tile_width_size > std::numeric_limits<std::size_t>::max() /
                    static_cast<std::size_t>(tile_channels)) {
                delete image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t tile_row_stride = tile_width_size * static_cast<std::size_t>(tile_channels);
            if (tile_length_size > std::numeric_limits<std::size_t>::max() / tile_row_stride) {
                delete image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t tile_size = tile_row_stride * tile_length_size;
            std::vector<std::uint8_t> decoded_tile;
            const std::size_t plane_tile_count = static_cast<std::size_t>(tile_columns * tile_rows);
            for (std::size_t tile_index = 0u; tile_index < tile_offsets.size(); ++tile_index) {
                const std::uint32_t tile_offset = tile_offsets[tile_index];
                const std::uint32_t tile_byte_count = tile_byte_counts[tile_index];
                if (tile_offset > tiff_size ||
                    static_cast<std::size_t>(tile_byte_count) > tiff_size - static_cast<std::size_t>(tile_offset)) {
                    delete image;
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::uint8_t* tile_source = tiff + static_cast<std::size_t>(tile_offset);
                if (!tiff_decode_tiled_payload(
                        tile_source,
                        static_cast<std::size_t>(tile_byte_count),
                        static_cast<std::uint16_t>(compression),
                        tile_size,
                        &decoded_tile)) {
                    delete image;
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                if (compression != TIFF_COMPRESSION_NONE) {
                    tile_source = decoded_tile.data();
                }
                const bool planar_separate = planar_config == 2u;
                const std::size_t plane_index = planar_separate
                    ? tile_index / plane_tile_count
                    : 0u;
                const std::size_t plane_tile_index = planar_separate
                    ? tile_index % plane_tile_count
                    : tile_index;
                const std::size_t tile_column =
                    plane_tile_index % static_cast<std::size_t>(tile_columns);
                const std::size_t tile_row =
                    plane_tile_index / static_cast<std::size_t>(tile_columns);
                const std::size_t origin_x = tile_column * tile_width_size;
                const std::size_t origin_y = tile_row * tile_length_size;
                const std::size_t copy_width = std::min(tile_width_size, static_cast<std::size_t>(width) - origin_x);
                const std::size_t copy_height = std::min(tile_length_size, static_cast<std::size_t>(height) - origin_y);
                const std::size_t copy_row_bytes = copy_width * static_cast<std::size_t>(channels);
                for (std::size_t y = 0u; y < copy_height; ++y) {
                    const std::uint8_t* tile_source_row =
                        tile_source + y * tile_row_stride;
                    std::uint8_t* image_row =
                        image->pixels.data() + (origin_y + y) * stride +
                        origin_x * static_cast<std::size_t>(channels);
                    if (planar_separate) {
                        for (std::size_t x = 0u; x < copy_width; ++x) {
                            image_row[x * static_cast<std::size_t>(channels) + plane_index] =
                                tile_source_row[x];
                        }
                    } else if (matches_rgbx_tiled_shape) {
                        for (std::size_t x = 0u; x < copy_width; ++x) {
                            std::memcpy(
                                image_row + x * 3u,
                                tile_source_row + x * 4u,
                                3u);
                        }
                    } else {
                        std::memcpy(image_row, tile_source_row, copy_row_bytes);
                    }
                }
            }
        } else if (size > 0u) {
            std::memcpy(image->pixels.data(), tiff + strip_offset, size);
        } else {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int parse_tiff_i16_image_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* recognized,
    PillowCImage** out_image)
{
    if (!recognized || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *recognized = false;
    *out_image = nullptr;
    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(tiff, tiff_size, ifd_index, &little_endian, &entry_count, &entries_offset)) {
        return PILLOW_C_OK;
    }

    auto read_scalar_tag = [little_endian](const std::uint8_t* entry, std::uint32_t* out_value) -> bool {
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (!out_value || count != 1u) {
            return false;
        }
        if (type == 3u) {
            *out_value = read_tiff16(entry + 8u, little_endian);
            return true;
        }
        if (type == 4u) {
            *out_value = read_tiff32(entry + 8u, little_endian);
            return true;
        }
        return false;
    };

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bits_per_sample = 0;
    std::uint32_t compression = TIFF_COMPRESSION_NONE;
    std::uint32_t photometric = 0;
    std::uint32_t strip_offset = 0;
    std::uint32_t rows_per_strip = 0;
    std::uint32_t strip_byte_count = 0;
    std::uint32_t planar_config = 1;
    std::uint32_t samples_per_pixel = 1;
    bool has_width = false;
    bool has_height = false;
    bool has_bits = false;
    bool has_compression = false;
    bool has_photometric = false;
    bool has_strip_offset = false;
    bool has_rows_per_strip = false;
    bool has_strip_byte_count = false;
    bool has_samples_per_pixel = false;
    bool has_sample_format = false;

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        std::uint32_t value = 0;
        switch (tag) {
        case 256u:
            has_width = read_scalar_tag(entry, &width);
            break;
        case 257u:
            has_height = read_scalar_tag(entry, &height);
            break;
        case 258u:
            has_bits = read_scalar_tag(entry, &bits_per_sample);
            break;
        case 259u:
            has_compression = read_scalar_tag(entry, &compression);
            break;
        case 262u:
            has_photometric = read_scalar_tag(entry, &photometric);
            break;
        case 273u:
            has_strip_offset = read_scalar_tag(entry, &strip_offset);
            break;
        case 277u:
            has_samples_per_pixel = read_scalar_tag(entry, &samples_per_pixel);
            break;
        case 278u:
            has_rows_per_strip = read_scalar_tag(entry, &rows_per_strip);
            break;
        case 279u:
            has_strip_byte_count = read_scalar_tag(entry, &strip_byte_count);
            break;
        case 284u:
            if (read_scalar_tag(entry, &value)) {
                planar_config = value;
            }
            break;
        case 339u:
            has_sample_format = true;
            break;
        default:
            break;
        }
    }

    const bool matches_i16 =
        has_width && has_height && has_bits && has_photometric && has_strip_offset && has_strip_byte_count &&
        width > 0u && height > 0u &&
        bits_per_sample == 16u &&
        (!has_compression ||
         compression == TIFF_COMPRESSION_NONE ||
         compression == TIFF_COMPRESSION_PACKBITS ||
         compression == TIFF_COMPRESSION_LZW ||
         compression == TIFF_COMPRESSION_ADOBE_DEFLATE) &&
        photometric == 1u &&
        (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        (compression == TIFF_COMPRESSION_NONE || (has_rows_per_strip && rows_per_strip == height)) &&
        planar_config == 1u &&
        !has_sample_format;
    if (!matches_i16) {
        return PILLOW_C_OK;
    }
    *recognized = true;
    if (width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(static_cast<int>(width), static_cast<int>(height), 2, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (static_cast<std::size_t>(strip_offset) > tiff_size ||
        static_cast<std::size_t>(strip_byte_count) > tiff_size - static_cast<std::size_t>(strip_offset)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint8_t* strip = tiff + strip_offset;
    const std::size_t strip_size = static_cast<std::size_t>(strip_byte_count);
    const std::uint8_t* pixels = strip;
    std::vector<std::uint8_t> decoded;
    if (compression == TIFF_COMPRESSION_NONE) {
        if (strip_size != size) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    } else if (compression == TIFF_COMPRESSION_PACKBITS) {
        if (!tiff_packbits_decode_strip(strip, strip_size, size, &decoded)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        pixels = decoded.data();
    } else if (compression == TIFF_COMPRESSION_LZW) {
        if (!tiff_lzw_decode_strip(strip, strip_size, size, &decoded)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        pixels = decoded.data();
    } else if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
        if (!pillow_c_inflate_zlib_deflate(strip, strip_size, &decoded, size) || decoded.size() != size) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        pixels = decoded.data();
    }
    try {
        auto* image = new PillowCImage{
            static_cast<int>(width),
            static_cast<int>(height),
            little_endian ? PILLOW_C_MODE_I16 : PILLOW_C_MODE_I16B,
            2,
            stride,
            std::vector<std::uint8_t>(size)};
        if (size > 0u) {
            std::memcpy(image->pixels.data(), pixels, size);
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int parse_tiff_i16_image(const std::uint8_t* tiff, std::size_t tiff_size, bool* recognized, PillowCImage** out_image)
{
    return parse_tiff_i16_image_for_ifd(tiff, tiff_size, 0, recognized, out_image);
}

int parse_tiff_numeric_image_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* recognized,
    PillowCImage** out_image)
{
    if (!recognized || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *recognized = false;
    *out_image = nullptr;

    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(tiff, tiff_size, ifd_index, &little_endian, &entry_count, &entries_offset)) {
        return PILLOW_C_OK;
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bits_per_sample = 0;
    std::uint32_t compression = 0;
    std::uint32_t photometric = 0;
    std::uint32_t strip_offset = 0;
    std::uint32_t samples_per_pixel = 0;
    std::uint32_t rows_per_strip = 0;
    std::uint32_t strip_byte_count = 0;
    std::uint32_t planar_config = 0;
    std::uint32_t sample_format = 0;
    bool has_width = false;
    bool has_height = false;
    bool has_bits = false;
    bool has_compression = false;
    bool has_photometric = false;
    bool has_strip_offset = false;
    bool has_samples = false;
    bool has_rows_per_strip = false;
    bool has_strip_byte_count = false;
    bool has_planar_config = false;
    bool has_sample_format = false;
    bool invalid_numeric_entry = false;

    auto read_scalar = [&](const std::uint8_t* entry, std::uint32_t* out_value) -> bool {
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
        if (!out_value || count != 1u) {
            return false;
        }
        if (type == 3u) {
            *out_value = read_tiff16(entry + 8u, little_endian);
            return true;
        }
        if (type == 4u) {
            *out_value = read_tiff32(entry + 8u, little_endian);
            return true;
        }
        return false;
    };

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        switch (tag) {
        case 256u:
            has_width = read_scalar(entry, &width);
            break;
        case 257u:
            has_height = read_scalar(entry, &height);
            break;
        case 258u:
            has_bits = read_scalar(entry, &bits_per_sample);
            break;
        case 259u:
            has_compression = read_scalar(entry, &compression);
            break;
        case 262u:
            has_photometric = read_scalar(entry, &photometric);
            break;
        case 273u:
            has_strip_offset = read_scalar(entry, &strip_offset);
            break;
        case 277u:
            has_samples = read_scalar(entry, &samples_per_pixel);
            break;
        case 278u:
            has_rows_per_strip = read_scalar(entry, &rows_per_strip);
            break;
        case 279u:
            has_strip_byte_count = read_scalar(entry, &strip_byte_count);
            break;
        case 284u:
            has_planar_config = read_scalar(entry, &planar_config);
            break;
        case 339u:
            has_sample_format = read_scalar(entry, &sample_format);
            invalid_numeric_entry = !has_sample_format;
            break;
        default:
            break;
        }
    }

    if (!has_sample_format || (sample_format != 2u && sample_format != 3u) ||
        !has_bits || bits_per_sample != 32u) {
        return PILLOW_C_OK;
    }
    *recognized = true;
    if (invalid_numeric_entry || !little_endian || !has_width || !has_height ||
        !has_compression ||
        (compression != TIFF_COMPRESSION_NONE &&
         compression != TIFF_COMPRESSION_PACKBITS &&
         compression != TIFF_COMPRESSION_LZW &&
         compression != TIFF_COMPRESSION_ADOBE_DEFLATE) ||
        !has_photometric || photometric != 1u ||
        !has_strip_offset || !has_rows_per_strip || !has_strip_byte_count ||
        !has_planar_config || planar_config != 1u ||
        (has_samples && samples_per_pixel != 1u) ||
        width == 0u || height == 0u ||
        width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        rows_per_strip != height) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(static_cast<int>(width), static_cast<int>(height), 4, &stride, &size) ||
        strip_offset > tiff_size ||
        strip_byte_count > tiff_size - static_cast<std::size_t>(strip_offset)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint8_t* strip = tiff + strip_offset;
    const std::size_t strip_size = static_cast<std::size_t>(strip_byte_count);
    const std::uint8_t* pixels = strip;
    std::vector<std::uint8_t> decoded;
    if (compression == TIFF_COMPRESSION_NONE) {
        if (strip_size != size) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    } else if (compression == TIFF_COMPRESSION_PACKBITS) {
        if (!tiff_packbits_decode_strip(strip, strip_size, size, &decoded)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        pixels = decoded.data();
    } else if (compression == TIFF_COMPRESSION_LZW) {
        if (!tiff_lzw_decode_strip(strip, strip_size, size, &decoded)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        pixels = decoded.data();
    } else if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
        if (!pillow_c_inflate_zlib_deflate(strip, strip_size, &decoded, size) || decoded.size() != size) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        pixels = decoded.data();
    }

    auto* image = new PillowCImage{
        static_cast<int>(width),
        static_cast<int>(height),
        sample_format == 2u ? PILLOW_C_MODE_I : PILLOW_C_MODE_F,
        4,
        stride,
        std::vector<std::uint8_t>(size)};
    std::memcpy(image->pixels.data(), pixels, size);
    *out_image = image;
    return PILLOW_C_OK;
}

int parse_tiff_numeric_image(const std::uint8_t* tiff, std::size_t tiff_size, bool* recognized, PillowCImage** out_image)
{
    return parse_tiff_numeric_image_for_ifd(tiff, tiff_size, 0, recognized, out_image);
}

void apply_tiff_orientation_three(PillowCImage* image)
{
    if (!image || image->channels <= 0) {
        return;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    const std::size_t pixel_count = image->pixels.size() / channels;
    const std::size_t half = pixel_count / 2u;
    for (std::size_t left_pixel = 0; left_pixel < half; ++left_pixel) {
        const std::size_t right_pixel = pixel_count - 1u - left_pixel;
        const std::size_t left = left_pixel * channels;
        const std::size_t right = right_pixel * channels;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            std::swap(image->pixels[left + channel], image->pixels[right + channel]);
        }
    }
}

int apply_tiff_orientation_six_or_eight(PillowCImage* image, int orientation)
{
    if (!image || image->channels <= 0 || image->width <= 0 || image->height <= 0 ||
        (orientation != 6 && orientation != 8)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int source_width = image->width;
    const int source_height = image->height;
    const int target_width = source_height;
    const int target_height = source_width;
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    std::size_t target_stride = 0;
    std::size_t target_size = 0;
    if (!checked_image_size(target_width, target_height, image->channels, &target_stride, &target_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> rotated(target_size);
    for (int y = 0; y < source_height; ++y) {
        for (int x = 0; x < source_width; ++x) {
            const int target_x = orientation == 6 ? (source_height - 1 - y) : y;
            const int target_y = orientation == 6 ? x : (source_width - 1 - x);
            const std::size_t source_offset =
                static_cast<std::size_t>(y) * image->stride + static_cast<std::size_t>(x) * channels;
            const std::size_t target_offset =
                static_cast<std::size_t>(target_y) * target_stride + static_cast<std::size_t>(target_x) * channels;
            for (std::size_t channel = 0; channel < channels; ++channel) {
                rotated[target_offset + channel] = image->pixels[source_offset + channel];
            }
        }
    }

    image->width = target_width;
    image->height = target_height;
    image->stride = target_stride;
    image->pixels = std::move(rotated);
    return PILLOW_C_OK;
}

int apply_tiff_orientation_mirror_or_transpose(PillowCImage* image, int orientation)
{
    if (!image || image->channels <= 0 || image->width <= 0 || image->height <= 0 ||
        (orientation != 2 && orientation != 4 && orientation != 5 && orientation != 7)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int source_width = image->width;
    const int source_height = image->height;
    const bool swaps_dimensions = orientation == 5 || orientation == 7;
    const int target_width = swaps_dimensions ? source_height : source_width;
    const int target_height = swaps_dimensions ? source_width : source_height;
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    std::size_t target_stride = 0;
    std::size_t target_size = 0;
    if (!checked_image_size(target_width, target_height, image->channels, &target_stride, &target_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> transformed(target_size);
    for (int y = 0; y < source_height; ++y) {
        for (int x = 0; x < source_width; ++x) {
            int target_x = x;
            int target_y = y;
            switch (orientation) {
            case 2:
                target_x = source_width - 1 - x;
                target_y = y;
                break;
            case 4:
                target_x = x;
                target_y = source_height - 1 - y;
                break;
            case 5:
                target_x = y;
                target_y = x;
                break;
            case 7:
                target_x = source_height - 1 - y;
                target_y = source_width - 1 - x;
                break;
            default:
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const std::size_t source_offset =
                static_cast<std::size_t>(y) * image->stride + static_cast<std::size_t>(x) * channels;
            const std::size_t target_offset =
                static_cast<std::size_t>(target_y) * target_stride + static_cast<std::size_t>(target_x) * channels;
            for (std::size_t channel = 0; channel < channels; ++channel) {
                transformed[target_offset + channel] = image->pixels[source_offset + channel];
            }
        }
    }

    image->width = target_width;
    image->height = target_height;
    image->stride = target_stride;
    image->pixels = std::move(transformed);
    return PILLOW_C_OK;
}

int apply_tiff_orientation_transform(PillowCImage* image, int orientation)
{
    if (orientation == 0 || orientation == 1) {
        return PILLOW_C_OK;
    }
    if (orientation == 3) {
        apply_tiff_orientation_three(image);
        return PILLOW_C_OK;
    }
    if (orientation == 2 || orientation == 4 || orientation == 5 || orientation == 7) {
        return apply_tiff_orientation_mirror_or_transpose(image, orientation);
    }
    if (orientation == 6 || orientation == 8) {
        return apply_tiff_orientation_six_or_eight(image, orientation);
    }
    return PILLOW_C_INVALID_ARGUMENT;
}


bool tiff_common_ascii_tag(int tag)
{
    return tag == 269 || tag == 270 || tag == 271 || tag == 272 || tag == 285 || tag == 305 || tag == 306 ||
        tag == 315 || tag == 316 || tag == 333 || tag == 337 || tag == 33432 || tag == 34737 || tag == 34852 || tag == 36867 || tag == 36868 ||
        tag == 36880 || tag == 36881 || tag == 36882 ||
        tag == 37394 || tag == 37395 || tag == 37520 || tag == 37521 || tag == 37522 ||
        tag == 40964 || tag == 42016 || tag == 42032 || tag == 42033 || tag == 42112 || tag == 42113 ||
        tag == 42035 || tag == 42036 || tag == 42037 || tag == 50708 || tag == 50735 || tag == 50827 ||
        tag == 50931 || tag == 50932 || tag == 50934 || tag == 50936 || tag == 50942 ||
        tag == 50966 || tag == 50967 || tag == 50968 || tag == 50971 || tag == 51081 || tag == 51092 || tag == 51182 ||
        tag == 52526 || tag == 52528;
}

bool tiff_common_uint_tag(int tag)
{
    return tag == 254 || tag == 255 || tag == 256 || tag == 257 || tag == 258 || tag == 259 || tag == 262 || tag == 263 ||
        tag == 264 || tag == 265 || tag == 266 || tag == 273 || tag == 277 || tag == 278 || tag == 279 ||
        tag == 280 || tag == 281 || tag == 284 || tag == 288 || tag == 289 || tag == 290 ||
        tag == 292 || tag == 293 || tag == 317 ||
        tag == 322 || tag == 323 || tag == 324 || tag == 325 ||
        tag == 326 || tag == 327 || tag == 328 ||
        tag == 332 || tag == 334 || tag == 34665 || tag == 34853 || tag == 34855 ||
        tag == 34864 || tag == 34865 || tag == 34866 || tag == 34867 || tag == 34868 || tag == 34869 ||
        tag == 338 || tag == 339 || tag == 340 || tag == 341 || tag == 531 ||
        tag == 40961 || tag == 40962 || tag == 40963 || tag == 41488 ||
        tag == 41495 || tag == 41985 || tag == 41986 || tag == 41987 ||
        tag == 41989 || tag == 41990 || tag == 41991 || tag == 41992 || tag == 41993 || tag == 41994 ||
        tag == 41996 || tag == 42080 || tag == 50711 || tag == 50717 || tag == 50741 || tag == 50778 || tag == 50779 || tag == 50879 ||
        tag == 50941 || tag == 50970 || tag == 50974 || tag == 50975 || tag == 51107 || tag == 51108 || tag == 51110 || tag == 51177 ||
        tag == 51180 || tag == 51181 || tag == 52529;
}

bool tiff_common_byte_array_tag(int tag)
{
    return tag == 34377 || tag == 40092 || tag == 40093 || tag == 40094 || tag == 40095 ||
        tag == 50706 || tag == 50707 || tag == 50709 || tag == 50710 || tag == 50781 || tag == 50831 || tag == 50833 ||
        tag == 50972 || tag == 50973 ||
        tag == 51043 || tag == 51111;
}

bool tiff_gps_ascii_tag(int tag)
{
    return tag == 1 || tag == 3 || tag == 9 || tag == 10 || tag == 12 || tag == 14 || tag == 16 || tag == 18 ||
        tag == 19 || tag == 23 || tag == 25 || tag == 27 || tag == 28;
}

bool tiff_gps_uint_tag(int tag)
{
    return tag == 5 || tag == 7 || tag == 11 || tag == 29 || tag == 30 || tag == 31;
}

bool tiff_gps_rational_tag(int tag)
{
    return tag == 6 || tag == 13 || tag == 15 || tag == 17 || tag == 21 || tag == 24 || tag == 26;
}

bool read_tiff_ascii_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::string* out_value)
{
    if (!tiff || !entry || !out_value) {
        return false;
    }
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 2u || count == 0u) {
        return false;
    }

    const std::uint8_t* value = nullptr;
    if (count <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size || static_cast<std::size_t>(count) > tiff_size - value_offset) {
            return false;
        }
        value = tiff + value_offset;
    }

    std::size_t value_size = 0u;
    while (value_size < static_cast<std::size_t>(count) && value[value_size] != 0u) {
        ++value_size;
    }
    out_value->assign(reinterpret_cast<const char*>(value), value_size);
    return true;
}

bool parse_tiff_resolution(const std::uint8_t* tiff, std::size_t tiff_size, TiffResolutionMetadata* out_metadata)
{
    return parse_tiff_resolution_for_ifd(tiff, tiff_size, 0, out_metadata);
}

bool read_tiff_rational_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::uint32_t* out_numerator,
    std::uint32_t* out_denominator)
{
    if (!tiff || !entry || !out_numerator || !out_denominator) {
        return false;
    }
    *out_numerator = 0u;
    *out_denominator = 0u;
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 5u || count != 1u) {
        return false;
    }

    const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
    if (value_offset > tiff_size || 8u > tiff_size - static_cast<std::size_t>(value_offset)) {
        return false;
    }
    const std::uint8_t* value = tiff + value_offset;
    const std::uint32_t denominator = read_tiff32(value + 4u, little_endian);
    if (denominator == 0u) {
        return false;
    }
    *out_numerator = read_tiff32(value, little_endian);
    *out_denominator = denominator;
    return true;
}

bool read_tiff_signed_rational_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::int32_t* out_numerator,
    std::int32_t* out_denominator)
{
    if (!tiff || !entry || !out_numerator || !out_denominator) {
        return false;
    }
    *out_numerator = 0;
    *out_denominator = 0;
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 10u || count != 1u) {
        return false;
    }

    const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
    if (value_offset > tiff_size || 8u > tiff_size - static_cast<std::size_t>(value_offset)) {
        return false;
    }
    const std::uint8_t* value = tiff + value_offset;
    const std::uint32_t raw_numerator = read_tiff32(value, little_endian);
    const std::uint32_t raw_denominator = read_tiff32(value + 4u, little_endian);
    const std::int32_t denominator = raw_denominator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        ? static_cast<std::int32_t>(raw_denominator)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_denominator) - 0x100000000ll);
    if (denominator == 0) {
        return false;
    }
    *out_numerator = raw_numerator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        ? static_cast<std::int32_t>(raw_numerator)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_numerator) - 0x100000000ll);
    *out_denominator = denominator;
    return true;
}

bool read_tiff_rational_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint32_t>* out_numerators,
    std::vector<std::uint32_t>* out_denominators)
{
    if (!tiff || !entry || !out_numerators || !out_denominators) {
        return false;
    }
    out_numerators->clear();
    out_denominators->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 5u || count == 0u ||
        count > std::numeric_limits<std::size_t>::max() / 8u) {
        return false;
    }

    const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
    const std::size_t value_size = static_cast<std::size_t>(count) * 8u;
    if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
        return false;
    }
    const std::uint8_t* value = tiff + value_offset;
    out_numerators->reserve(count);
    out_denominators->reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index) {
        const std::uint8_t* item = value + static_cast<std::size_t>(index) * 8u;
        const std::uint32_t denominator = read_tiff32(item + 4u, little_endian);
        if (denominator == 0u) {
            out_numerators->clear();
            out_denominators->clear();
            return false;
        }
        out_numerators->push_back(read_tiff32(item, little_endian));
        out_denominators->push_back(denominator);
    }
    return true;
}

bool read_tiff_signed_rational_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::int32_t>* out_numerators,
    std::vector<std::int32_t>* out_denominators)
{
    if (!tiff || !entry || !out_numerators || !out_denominators) {
        return false;
    }
    out_numerators->clear();
    out_denominators->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 10u || count == 0u ||
        count > std::numeric_limits<std::size_t>::max() / 8u) {
        return false;
    }

    const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
    const std::size_t value_size = static_cast<std::size_t>(count) * 8u;
    if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
        return false;
    }
    const std::uint8_t* value = tiff + value_offset;
    out_numerators->reserve(count);
    out_denominators->reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index) {
        const std::uint8_t* item = value + static_cast<std::size_t>(index) * 8u;
        const std::uint32_t raw_numerator = read_tiff32(item, little_endian);
        const std::uint32_t raw_denominator = read_tiff32(item + 4u, little_endian);
        const std::int32_t denominator = raw_denominator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(raw_denominator)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_denominator) - 0x100000000ll);
        if (denominator == 0) {
            out_numerators->clear();
            out_denominators->clear();
            return false;
        }
        const std::int32_t numerator = raw_numerator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(raw_numerator)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_numerator) - 0x100000000ll);
        out_numerators->push_back(numerator);
        out_denominators->push_back(denominator);
    }
    return true;
}

bool read_tiff_uint_entry_value(
    bool little_endian,
    const std::uint8_t* entry,
    std::uint32_t* out_value,
    int* out_type)
{
    if (!entry || !out_value || !out_type) {
        return false;
    }
    *out_value = 0u;
    *out_type = 0;
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (count != 1u) {
        return false;
    }
    if (type == 3u) {
        *out_value = read_tiff16(entry + 8u, little_endian);
    } else if (type == 4u) {
        *out_value = read_tiff32(entry + 8u, little_endian);
    } else {
        return false;
    }
    *out_type = static_cast<int>(type);
    return true;
}

bool read_tiff_ushort_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint32_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 3u || count == 0u) {
        return false;
    }

    const std::uint8_t* value = nullptr;
    if (count <= 2u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        const std::size_t value_size = static_cast<std::size_t>(count) * 2u;
        if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index) {
        out_values->push_back(read_tiff16(value + static_cast<std::size_t>(index) * 2u, little_endian));
    }
    return true;
}

bool read_tiff_uint_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint32_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 4u || count == 0u) {
        return false;
    }

    const std::size_t value_size = static_cast<std::size_t>(count) * 4u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index) {
        out_values->push_back(read_tiff32(value + static_cast<std::size_t>(index) * 4u, little_endian));
    }
    return true;
}

bool read_tiff_float_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<float>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 11u || count == 0u || count > std::numeric_limits<std::size_t>::max() / 4u) {
        return false;
    }

    const std::size_t value_size = static_cast<std::size_t>(count) * 4u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index) {
        const std::uint32_t bits = read_tiff32(value + static_cast<std::size_t>(index) * 4u, little_endian);
        float number = 0.0f;
        std::memcpy(&number, &bits, sizeof(number));
        out_values->push_back(number);
    }
    return true;
}

bool read_tiff_double_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<double>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 12u || count == 0u || count > std::numeric_limits<std::size_t>::max() / 8u) {
        return false;
    }

    const std::size_t value_size = static_cast<std::size_t>(count) * 8u;
    const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
    if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
        return false;
    }
    const std::uint8_t* value = tiff + value_offset;

    out_values->reserve(count);
    for (std::uint32_t index = 0u; index < count; ++index) {
        const std::uint64_t bits = read_tiff64(value + static_cast<std::size_t>(index) * 8u, little_endian);
        double number = 0.0;
        std::memcpy(&number, &bits, sizeof(number));
        out_values->push_back(number);
    }
    return true;
}

bool read_tiff_byte_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint8_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if (type != 1u || count == 0u) {
        return false;
    }

    const std::uint8_t* value = nullptr;
    if (count <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size || static_cast<std::size_t>(count) > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->assign(value, value + count);
    return true;
}

bool read_tiff_undefined_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint8_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if ((type != 1u && type != 7u) || count == 0u) {
        return false;
    }

    const std::uint8_t* value = nullptr;
    if (count <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size || static_cast<std::size_t>(count) > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->assign(value, value + count);
    return true;
}

bool read_tiff_icc_profile_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint8_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if ((type != 1u && type != 7u) || count == 0u) {
        return false;
    }

    const std::uint8_t* value = nullptr;
    if (count <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size ||
            static_cast<std::size_t>(count) > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->assign(value, value + count);
    return true;
}

bool read_tiff_xmp_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint8_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint32_t count = read_tiff32(entry + 4u, little_endian);
    if ((type != 1u && type != 7u) || count == 0u) {
        return false;
    }

    const std::uint8_t* value = nullptr;
    if (count <= 4u) {
        value = entry + 8u;
    } else {
        const std::uint32_t value_offset = read_tiff32(entry + 8u, little_endian);
        if (value_offset > tiff_size ||
            static_cast<std::size_t>(count) > tiff_size - static_cast<std::size_t>(value_offset)) {
            return false;
        }
        value = tiff + value_offset;
    }

    out_values->assign(value, value + count);
    return true;
}

bool read_tiff_bigtiff_blob_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint8_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if ((type != 1u && type != 7u) || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::uint8_t* value = nullptr;
    if (count <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            count > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }
    out_values->assign(value, value + count_size);
    return true;
}

bool read_tiff_bigtiff_uint_scalar_entry_value(
    bool little_endian,
    const std::uint8_t* entry,
    std::uint32_t* out_value,
    int* out_type)
{
    if (!entry || !out_value || !out_type) {
        return false;
    }
    *out_value = 0u;
    *out_type = 0;
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (count != 1u) {
        return false;
    }
    if (type == 3u) {
        *out_value = read_tiff16(entry + 12u, little_endian);
    } else if (type == 4u) {
        *out_value = read_tiff32(entry + 12u, little_endian);
    } else if (type == 16u) {
        const std::uint64_t value64 = read_tiff64(entry + 12u, little_endian);
        if (value64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }
        *out_value = static_cast<std::uint32_t>(value64);
        *out_type = 4;  // normalize LONG8 scalars into the serializer's LONG shape
        return true;
    } else {
        return false;
    }
    *out_type = static_cast<int>(type);
    return true;
}

bool read_tiff_bigtiff_uint_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint32_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    const std::size_t element_size = type == 4u ? 4u : (type == 16u ? 8u : 0u);
    if (element_size == 0u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / element_size)) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::size_t value_size = count_size * element_size;
    const std::uint8_t* value = nullptr;
    if (value_size <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    out_values->resize(count_size);
    for (std::size_t index = 0u; index < count_size; ++index) {
        if (type == 4u) {
            (*out_values)[index] = read_tiff32(value + index * 4u, little_endian);
        } else {
            const std::uint64_t value64 = read_tiff64(value + index * 8u, little_endian);
            if (value64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                out_values->clear();
                return false;
            }
            (*out_values)[index] = static_cast<std::uint32_t>(value64);
        }
    }
    return true;
}

bool read_tiff_bigtiff_ushort_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint32_t>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 3u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 2u)) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::size_t value_size = count_size * 2u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    out_values->resize(count_size);
    for (std::size_t index = 0u; index < count_size; ++index) {
        (*out_values)[index] = read_tiff16(value + index * 2u, little_endian);
    }
    return true;
}

bool read_tiff_bigtiff_rational_entry_value(
    bool little_endian,
    const std::uint8_t* entry,
    std::uint32_t* out_numerator,
    std::uint32_t* out_denominator)
{
    if (!entry || !out_numerator || !out_denominator) {
        return false;
    }
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 5u || count != 1u) {
        return false;
    }
    const std::uint32_t numerator = read_tiff32(entry + 12u, little_endian);
    const std::uint32_t denominator = read_tiff32(entry + 16u, little_endian);
    if (denominator == 0u) {
        return false;
    }
    *out_numerator = numerator;
    *out_denominator = denominator;
    return true;
}

bool read_tiff_bigtiff_rational_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::uint32_t>* out_numerators,
    std::vector<std::uint32_t>* out_denominators)
{
    if (!tiff || !entry || !out_numerators || !out_denominators) {
        return false;
    }
    out_numerators->clear();
    out_denominators->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 5u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 8u)) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::size_t value_size = count_size * 8u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    out_numerators->reserve(count_size);
    out_denominators->reserve(count_size);
    for (std::size_t index = 0u; index < count_size; ++index) {
        const std::uint8_t* item = value + index * 8u;
        const std::uint32_t denominator = read_tiff32(item + 4u, little_endian);
        if (denominator == 0u) {
            out_numerators->clear();
            out_denominators->clear();
            return false;
        }
        out_numerators->push_back(read_tiff32(item, little_endian));
        out_denominators->push_back(denominator);
    }
    return true;
}

bool read_tiff_bigtiff_signed_rational_entry_value(
    bool little_endian,
    const std::uint8_t* entry,
    std::int32_t* out_numerator,
    std::int32_t* out_denominator)
{
    if (!entry || !out_numerator || !out_denominator) {
        return false;
    }
    *out_numerator = 0;
    *out_denominator = 0;
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 10u || count != 1u) {
        return false;
    }
    const std::uint32_t raw_numerator = read_tiff32(entry + 12u, little_endian);
    const std::uint32_t raw_denominator = read_tiff32(entry + 16u, little_endian);
    const std::int32_t denominator = raw_denominator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        ? static_cast<std::int32_t>(raw_denominator)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_denominator) - 0x100000000ll);
    if (denominator == 0) {
        return false;
    }
    *out_numerator = raw_numerator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        ? static_cast<std::int32_t>(raw_numerator)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_numerator) - 0x100000000ll);
    *out_denominator = denominator;
    return true;
}

bool read_tiff_bigtiff_signed_rational_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::int32_t>* out_numerators,
    std::vector<std::int32_t>* out_denominators)
{
    if (!tiff || !entry || !out_numerators || !out_denominators) {
        return false;
    }
    out_numerators->clear();
    out_denominators->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 10u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 8u)) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::size_t value_size = count_size * 8u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    out_numerators->reserve(count_size);
    out_denominators->reserve(count_size);
    for (std::size_t index = 0u; index < count_size; ++index) {
        const std::uint8_t* item = value + index * 8u;
        const std::uint32_t raw_numerator = read_tiff32(item, little_endian);
        const std::uint32_t raw_denominator = read_tiff32(item + 4u, little_endian);
        const std::int32_t denominator =
            raw_denominator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(raw_denominator)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_denominator) - 0x100000000ll);
        if (denominator == 0) {
            out_numerators->clear();
            out_denominators->clear();
            return false;
        }
        out_numerators->push_back(
            raw_numerator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(raw_numerator)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_numerator) - 0x100000000ll));
        out_denominators->push_back(denominator);
    }
    return true;
}

bool read_tiff_bigtiff_double_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<double>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 12u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 8u)) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::size_t value_size = count_size * 8u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    out_values->reserve(count_size);
    for (std::size_t index = 0u; index < count_size; ++index) {
        const std::uint64_t bits = read_tiff64(value + index * 8u, little_endian);
        double number = 0.0;
        std::memcpy(&number, &bits, sizeof(number));
        out_values->push_back(number);
    }
    return true;
}

bool read_tiff_bigtiff_float_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<float>* out_values)
{
    if (!tiff || !entry || !out_values) {
        return false;
    }
    out_values->clear();
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 11u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 4u)) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::size_t value_size = count_size * 4u;
    const std::uint8_t* value = nullptr;
    if (value_size <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    out_values->reserve(count_size);
    for (std::size_t index = 0u; index < count_size; ++index) {
        const std::uint32_t bits = read_tiff32(value + index * 4u, little_endian);
        float number = 0.0f;
        std::memcpy(&number, &bits, sizeof(number));
        out_values->push_back(number);
    }
    return true;
}

bool read_tiff_bigtiff_ascii_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::string* out_value)
{
    if (!tiff || !entry || !out_value) {
        return false;
    }
    const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
    const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
    if (type != 2u || count == 0u ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    const std::size_t count_size = static_cast<std::size_t>(count);
    const std::uint8_t* value = nullptr;
    if (count <= 8u) {
        value = entry + 12u;
    } else {
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            count > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        value = tiff + static_cast<std::size_t>(value_offset);
    }

    std::size_t value_size = 0u;
    while (value_size < count_size && value[value_size] != 0u) {
        ++value_size;
    }
    out_value->assign(reinterpret_cast<const char*>(value), value_size);
    return true;
}

bool locate_tiff_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* out_little_endian,
    std::uint16_t* out_entry_count,
    std::size_t* out_entries_offset)
{
    if (!out_little_endian || !out_entry_count || !out_entries_offset) {
        return false;
    }
    *out_little_endian = false;
    *out_entry_count = 0u;
    *out_entries_offset = 0u;
    if (ifd_index < 0) {
        return false;
    }
    if (!tiff || tiff_size < 8u) {
        return false;
    }

    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || read_tiff16(tiff + 2u, little_endian) != 42u) {
        return false;
    }

    std::uint32_t ifd_offset = read_tiff32(tiff + 4u, little_endian);
    for (int index = 0; index <= ifd_index; ++index) {
        if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
            return false;
        }
        const std::uint8_t* ifd = tiff + ifd_offset;
        const std::uint16_t entry_count = read_tiff16(ifd, little_endian);
        const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
        if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
            return false;
        }
        if (index == ifd_index) {
            *out_little_endian = little_endian;
            *out_entry_count = entry_count;
            *out_entries_offset = entries_offset;
            return true;
        }
        const std::size_t next_offset_location = entries_offset + static_cast<std::size_t>(entry_count) * 12u;
        if (next_offset_location > tiff_size || tiff_size - next_offset_location < 4u) {
            return false;
        }
        ifd_offset = read_tiff32(tiff + next_offset_location, little_endian);
        if (ifd_offset == 0u) {
            return false;
        }
    }
    return false;
}

bool locate_tiff_bigtiff_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* out_little_endian,
    std::uint64_t* out_entry_count,
    std::size_t* out_entries_offset)
{
    if (!out_little_endian || !out_entry_count || !out_entries_offset) {
        return false;
    }
    *out_little_endian = false;
    *out_entry_count = 0u;
    *out_entries_offset = 0u;
    if (!tiff || tiff_size < 16u || ifd_index < 0) {
        return false;
    }

    std::uint64_t first_ifd_offset = 0u;
    bool little_endian = false;
    if (!parse_tiff_bigtiff_header(
            tiff,
            tiff_size,
            &little_endian,
            &first_ifd_offset)) {
        return false;
    }
    const std::uint64_t tiff_size_u64 = static_cast<std::uint64_t>(tiff_size);
    std::uint64_t ifd_offset = first_ifd_offset;
    for (int index = 0; index <= ifd_index; ++index) {
        if (ifd_offset > tiff_size_u64 || tiff_size_u64 - ifd_offset < 8u) {
            return false;
        }
        const std::uint64_t entry_count = read_tiff64(
            tiff + static_cast<std::size_t>(ifd_offset),
            little_endian);
        const std::uint64_t entries_offset_u64 = ifd_offset + 8u;
        if (entries_offset_u64 > tiff_size_u64 ||
            entry_count > (tiff_size_u64 - entries_offset_u64) / 20u) {
            return false;
        }
        if (index == ifd_index) {
            *out_little_endian = little_endian;
            *out_entry_count = entry_count;
            *out_entries_offset = static_cast<std::size_t>(entries_offset_u64);
            return true;
        }
        const std::uint64_t next_offset_location = entries_offset_u64 + entry_count * 20u;
        if (next_offset_location > tiff_size_u64 || tiff_size_u64 - next_offset_location < 8u) {
            return false;
        }
        ifd_offset = read_tiff64(
            tiff + static_cast<std::size_t>(next_offset_location),
            little_endian);
        if (ifd_offset == 0u) {
            return false;
        }
    }
    return false;
}

bool parse_tiff_bigtiff_icc_profile_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    std::vector<std::uint8_t>* out_profile)
{
    if (!out_profile) {
        return false;
    }
    out_profile->clear();

    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset) ||
        entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return false;
    }

    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 20u;
        const int tag = static_cast<int>(read_tiff16(entry, little_endian));
        if (tag == 34675) {
            return read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, out_profile);
        }
    }
    return false;
}

bool parse_tiff_bigtiff_xmp_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    std::vector<std::uint8_t>* out_xmp)
{
    if (!out_xmp) {
        return false;
    }
    out_xmp->clear();

    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset) ||
        entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return false;
    }

    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 20u;
        const int tag = static_cast<int>(read_tiff16(entry, little_endian));
        if (tag == 700) {
            return read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, out_xmp);
        }
    }
    return false;
}

bool parse_tiff_bigtiff_resolution_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    TiffResolutionMetadata* out_metadata)
{
    if (!out_metadata) {
        return false;
    }
    *out_metadata = TiffResolutionMetadata{};

    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset) ||
        entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return false;
    }

    double x_resolution = 0.0;
    double y_resolution = 0.0;
    int resolution_unit = 0;
    bool has_x_resolution = false;
    bool has_y_resolution = false;
    bool has_resolution_unit = false;
    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 20u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
        if ((tag == 0x011au || tag == 0x011bu) && type == 5u && count == 1u) {
            const std::uint32_t numerator = read_tiff32(entry + 12u, little_endian);
            const std::uint32_t denominator = read_tiff32(entry + 16u, little_endian);
            if (denominator == 0u) {
                continue;
            }
            const double value = static_cast<double>(numerator) / static_cast<double>(denominator);
            if (tag == 0x011au) {
                x_resolution = value;
                has_x_resolution = true;
            } else {
                y_resolution = value;
                has_y_resolution = true;
            }
        } else if (tag == 0x0128u && type == 3u && count == 1u) {
            resolution_unit = static_cast<int>(read_tiff16(entry + 12u, little_endian));
            has_resolution_unit = true;
        }
    }

    if (!has_x_resolution || !has_y_resolution || !has_resolution_unit ||
        resolution_unit != 2 || x_resolution <= 0.0 || y_resolution <= 0.0) {
        return false;
    }
    out_metadata->has_dpi = true;
    out_metadata->dpi_x = x_resolution;
    out_metadata->dpi_y = y_resolution;
    return true;
}

int read_tiff_bigtiff_orientation_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index)
{
    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset) ||
        entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return 0;
    }

    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 20u;
        if (read_tiff16(entry, little_endian) == 274u &&
            read_tiff16(entry + 2u, little_endian) == 3u &&
            read_tiff64(entry + 4u, little_endian) == 1u) {
            const std::uint16_t value = read_tiff16(entry + 12u, little_endian);
            return value >= 1u && value <= 8u ? static_cast<int>(value) : 0;
        }
    }
    return 0;
}

int parse_tiff_bigtiff_tiled_image_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* is_bigtiff,
    bool* recognized,
    PillowCImage** out_image)
{
    if (!is_bigtiff || !recognized || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *is_bigtiff = false;
    *recognized = false;
    *out_image = nullptr;
    if (!tiff || tiff_size < 16u) {
        return PILLOW_C_OK;
    }
    const bool has_bigtiff_magic =
        (tiff[0] == 'I' && tiff[1] == 'I' && read_tiff16(tiff + 2u, true) == 43u) ||
        (tiff[0] == 'M' && tiff[1] == 'M' && read_tiff16(tiff + 2u, false) == 43u);
    if (!has_bigtiff_magic) {
        return PILLOW_C_OK;
    }
    *is_bigtiff = true;

    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    auto read_scalar = [little_endian](const std::uint8_t* entry, std::uint64_t* out_value) -> bool {
        if (!entry || !out_value || read_tiff64(entry + 4u, little_endian) != 1u) {
            return false;
        }
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        if (type == 3u) {
            *out_value = read_tiff16(entry + 12u, little_endian);
            return true;
        }
        if (type == 4u) {
            *out_value = read_tiff32(entry + 12u, little_endian);
            return true;
        }
        if (type == 16u) {
            *out_value = read_tiff64(entry + 12u, little_endian);
            return true;
        }
        return false;
    };
    auto read_short_array = [tiff, tiff_size, little_endian](
        const std::uint8_t* entry,
        std::vector<std::uint32_t>* out_values) -> bool {
        if (!entry || !out_values || read_tiff16(entry + 2u, little_endian) != 3u) {
            return false;
        }
        const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
        if (count == 0u || count > std::numeric_limits<std::size_t>::max() / 2u) {
            return false;
        }
        const std::size_t value_size = static_cast<std::size_t>(count) * 2u;
        out_values->clear();
        out_values->resize(static_cast<std::size_t>(count));
        if (value_size <= 8u) {
            for (std::size_t index = 0u; index < out_values->size(); ++index) {
                (*out_values)[index] = read_tiff16(entry + 12u + index * 2u, little_endian);
            }
            return true;
        }
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            out_values->clear();
            return false;
        }
        for (std::size_t index = 0u; index < out_values->size(); ++index) {
            (*out_values)[index] = read_tiff16(
                tiff + static_cast<std::size_t>(value_offset) + index * 2u,
                little_endian);
        }
        return true;
    };
    auto read_long8_array = [tiff, tiff_size, little_endian](
        const std::uint8_t* entry,
        std::vector<std::uint64_t>* out_values) -> bool {
        if (!entry || !out_values || read_tiff16(entry + 2u, little_endian) != 16u) {
            return false;
        }
        const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
        if (count == 0u || count > std::numeric_limits<std::size_t>::max() / 8u) {
            return false;
        }
        const std::size_t value_size = static_cast<std::size_t>(count) * 8u;
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            return false;
        }
        out_values->resize(static_cast<std::size_t>(count));
        for (std::size_t index = 0u; index < out_values->size(); ++index) {
            (*out_values)[index] = read_tiff64(
                tiff + static_cast<std::size_t>(value_offset) + index * 8u,
                little_endian);
        }
        return true;
    };

    std::uint64_t width = 0u;
    std::uint64_t height = 0u;
    std::vector<std::uint32_t> bits_per_sample;
    std::uint64_t compression = 0u;
    std::uint64_t photometric = 0u;
    std::uint64_t orientation = 0u;
    std::uint64_t planar_config = 1u;
    std::uint64_t samples_per_pixel = 1u;
    std::uint64_t extra_samples = 0u;
    std::uint64_t sample_format = 1u;
    std::uint64_t tile_width = 0u;
    std::uint64_t tile_length = 0u;
    std::vector<std::uint64_t> tile_offsets;
    std::vector<std::uint64_t> tile_byte_counts;
    bool has_width = false;
    bool has_height = false;
    bool has_bits = false;
    bool has_compression = false;
    bool has_photometric = false;
    bool has_samples_per_pixel = false;
    bool has_extra_samples = false;
    bool has_sample_format = false;
    bool has_tile_width = false;
    bool has_tile_length = false;
    bool has_tile_offsets = false;
    bool has_tile_byte_counts = false;

    if (entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 20u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        switch (tag) {
        case 256u:
            has_width = read_scalar(entry, &width);
            break;
        case 257u:
            has_height = read_scalar(entry, &height);
            break;
        case 258u:
            has_bits = read_short_array(entry, &bits_per_sample);
            break;
        case 259u:
            has_compression = read_scalar(entry, &compression);
            break;
        case 262u:
            has_photometric = read_scalar(entry, &photometric);
            break;
        case 274u:
            if (!read_scalar(entry, &orientation)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            break;
        case 277u:
            has_samples_per_pixel = read_scalar(entry, &samples_per_pixel);
            break;
        case 284u:
            read_scalar(entry, &planar_config);
            break;
        case 322u:
            has_tile_width = read_scalar(entry, &tile_width);
            break;
        case 323u:
            has_tile_length = read_scalar(entry, &tile_length);
            break;
        case 324u:
            has_tile_offsets = read_long8_array(entry, &tile_offsets);
            break;
        case 325u:
            has_tile_byte_counts = read_long8_array(entry, &tile_byte_counts);
            break;
        case 338u:
            has_extra_samples = read_scalar(entry, &extra_samples);
            break;
        case 339u:
            has_sample_format = read_scalar(entry, &sample_format);
            break;
        default:
            break;
        }
    }

    const bool has_supported_compression =
        compression == TIFF_COMPRESSION_NONE ||
        compression == TIFF_COMPRESSION_PACKBITS ||
        compression == TIFF_COMPRESSION_LZW ||
        compression == TIFF_COMPRESSION_ADOBE_DEFLATE;
    const bool has_valid_shape =
        has_width && has_height && has_bits && has_compression && has_photometric &&
        has_tile_width && has_tile_length && has_tile_offsets && has_tile_byte_counts &&
        width > 0u && height > 0u && tile_width > 0u && tile_length > 0u &&
        has_supported_compression &&
        (photometric == 1u || photometric == 2u || photometric == 5u) &&
        (planar_config == 1u || planar_config == 2u);
    if (!has_valid_shape || tile_offsets.size() != tile_byte_counts.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint64_t tile_columns = (width + tile_width - 1u) / tile_width;
    const std::uint64_t tile_rows = (height + tile_length - 1u) / tile_length;
    const std::uint64_t tile_planes = planar_config == 2u ? samples_per_pixel : 1u;
    if (tile_columns == 0u || tile_rows == 0u ||
        tile_columns > std::numeric_limits<std::uint64_t>::max() / tile_rows ||
        tile_planes == 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint64_t plane_tile_count_u64 = tile_columns * tile_rows;
    if (plane_tile_count_u64 > std::numeric_limits<std::uint64_t>::max() / tile_planes) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint64_t expected_tile_count_u64 = plane_tile_count_u64 * tile_planes;
    if (expected_tile_count_u64 > std::numeric_limits<std::size_t>::max() ||
        tile_offsets.size() != static_cast<std::size_t>(expected_tile_count_u64)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool matches_l =
        bits_per_sample.size() == 1u && bits_per_sample[0] == 8u && photometric == 1u &&
        (!has_samples_per_pixel || samples_per_pixel == 1u);
    const bool matches_rgb =
        bits_per_sample.size() == 3u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && photometric == 2u &&
        has_samples_per_pixel && samples_per_pixel == 3u;
    const bool matches_rgba =
        bits_per_sample.size() == 4u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        photometric == 2u && has_samples_per_pixel && samples_per_pixel == 4u &&
        has_extra_samples && extra_samples == 2u;
    const bool matches_compressed_planar_storage =
        planar_config == 2u && compression != TIFF_COMPRESSION_NONE;
    const bool matches_i16 =
        bits_per_sample.size() == 1u && bits_per_sample[0] == 16u &&
        photometric == 1u && (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        (!has_sample_format || sample_format == 1u) && planar_config == 1u;
    const bool matches_i =
        bits_per_sample.size() == 1u && bits_per_sample[0] == 32u &&
        photometric == 1u && (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        has_sample_format && sample_format == 2u && planar_config == 1u;
    const bool matches_f =
        bits_per_sample.size() == 1u && bits_per_sample[0] == 32u &&
        photometric == 1u && (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        has_sample_format && sample_format == 3u && planar_config == 1u;
    const bool matches_cmyk =
        bits_per_sample.size() == 4u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        photometric == 5u && has_samples_per_pixel && samples_per_pixel == 4u &&
        !has_extra_samples && planar_config == 1u;
    const bool matches_rgbx =
        bits_per_sample.size() == 4u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        photometric == 2u && has_samples_per_pixel && samples_per_pixel == 4u &&
        has_extra_samples && extra_samples == 0u &&
        (planar_config == 1u || matches_compressed_planar_storage);
    const bool matches_la =
        bits_per_sample.size() == 2u && bits_per_sample[0] == 8u && bits_per_sample[1] == 8u &&
        photometric == 1u && has_samples_per_pixel && samples_per_pixel == 2u &&
        has_extra_samples && extra_samples == 2u &&
        (planar_config == 1u || matches_compressed_planar_storage);
    if (!matches_l && !matches_rgb && !matches_rgba && !matches_rgbx && !matches_la &&
        !matches_i16 && !matches_i && !matches_f && !matches_cmyk) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (width > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        tile_width > std::numeric_limits<std::size_t>::max() ||
        tile_length > std::numeric_limits<std::size_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int mode = matches_i16
        ? (little_endian ? PILLOW_C_MODE_I16 : PILLOW_C_MODE_I16B)
        : (matches_i
            ? PILLOW_C_MODE_I
            : (matches_f
                ? PILLOW_C_MODE_F
                : (matches_cmyk
                    ? PILLOW_C_MODE_CMYK
                    : (matches_rgba
                        ? PILLOW_C_MODE_RGBA
                        : (matches_la
                            ? PILLOW_C_MODE_LA
                            : (matches_rgb || matches_rgbx ? PILLOW_C_MODE_RGB : PILLOW_C_MODE_L))))));
    const int channels = matches_i16
        ? 2
        : ((matches_i || matches_f || matches_cmyk || matches_rgba)
            ? 4
            : (matches_la ? 2 : (matches_rgbx || matches_rgb ? 3 : 1)));
    const int tile_channels = planar_config == 2u ? 1 : (matches_rgbx ? 4 : channels);
    std::size_t stride = 0u;
    std::size_t image_size = 0u;
    if (!checked_image_size(static_cast<int>(width), static_cast<int>(height), channels, &stride, &image_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t tile_width_size = static_cast<std::size_t>(tile_width);
    const std::size_t tile_length_size = static_cast<std::size_t>(tile_length);
    if (tile_width_size > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(tile_channels)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t tile_row_stride = tile_width_size * static_cast<std::size_t>(tile_channels);
    if (tile_length_size > std::numeric_limits<std::size_t>::max() / tile_row_stride) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t tile_size = tile_row_stride * tile_length_size;
    const bool normalize_big_endian_numeric_samples =
        !little_endian && (matches_i || matches_f);

    try {
        auto* image = new PillowCImage{
            static_cast<int>(width),
            static_cast<int>(height),
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(image_size)};
        std::vector<std::uint8_t> decoded_tile;
        const bool planar_separate = planar_config == 2u;
        const std::size_t plane_tile_count = static_cast<std::size_t>(plane_tile_count_u64);
        for (std::size_t tile_index = 0u; tile_index < tile_offsets.size(); ++tile_index) {
            const std::uint64_t tile_offset = tile_offsets[tile_index];
            const std::uint64_t tile_byte_count = tile_byte_counts[tile_index];
            if (tile_offset > static_cast<std::uint64_t>(tiff_size) ||
                tile_byte_count > static_cast<std::uint64_t>(tiff_size) - tile_offset) {
                delete image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint8_t* tile_source = tiff + static_cast<std::size_t>(tile_offset);
            if (!tiff_decode_tiled_payload(
                    tile_source,
                    static_cast<std::size_t>(tile_byte_count),
                    static_cast<std::uint16_t>(compression),
                    tile_size,
                    &decoded_tile)) {
                delete image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (compression != TIFF_COMPRESSION_NONE) {
                tile_source = decoded_tile.data();
            }
            const std::size_t plane_index = planar_separate
                ? tile_index / plane_tile_count
                : 0u;
            const std::size_t plane_tile_index = planar_separate
                ? tile_index % plane_tile_count
                : tile_index;
            const std::size_t tile_column =
                plane_tile_index % static_cast<std::size_t>(tile_columns);
            const std::size_t tile_row =
                plane_tile_index / static_cast<std::size_t>(tile_columns);
            const std::size_t origin_x = tile_column * tile_width_size;
            const std::size_t origin_y = tile_row * tile_length_size;
            const std::size_t copy_width = std::min(tile_width_size, static_cast<std::size_t>(width) - origin_x);
            const std::size_t copy_height = std::min(tile_length_size, static_cast<std::size_t>(height) - origin_y);
            const bool skip_planar_output_plane =
                (matches_rgbx && plane_index == 3u) ||
                (matches_la && plane_index == 1u);
            for (std::size_t y = 0u; y < copy_height; ++y) {
                const std::uint8_t* tile_row_source = tile_source + y * tile_row_stride;
                std::uint8_t* image_row = image->pixels.data() + (origin_y + y) * stride + origin_x * static_cast<std::size_t>(channels);
                if (planar_separate) {
                    if (!skip_planar_output_plane) {
                        for (std::size_t x = 0u; x < copy_width; ++x) {
                            image_row[x * static_cast<std::size_t>(channels) + plane_index] =
                                tile_row_source[x];
                        }
                    }
                } else if (matches_rgbx) {
                    for (std::size_t x = 0u; x < copy_width; ++x) {
                        std::memcpy(
                            image_row + x * 3u,
                            tile_row_source + x * 4u,
                            3u);
                    }
                } else if (normalize_big_endian_numeric_samples) {
                    for (std::size_t x = 0u; x < copy_width; ++x) {
                        const std::uint8_t* sample = tile_row_source + x * 4u;
                        std::uint8_t* destination = image_row + x * 4u;
                        destination[0] = sample[3];
                        destination[1] = sample[2];
                        destination[2] = sample[1];
                        destination[3] = sample[0];
                    }
                } else {
                    std::memcpy(
                        image_row,
                        tile_row_source,
                        copy_width * static_cast<std::size_t>(channels));
                }
            }
        }
        if (orientation > 8u) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (orientation != 0u) {
            const int orientation_status = apply_tiff_orientation_transform(
                image,
                static_cast<int>(orientation));
            if (orientation_status != PILLOW_C_OK) {
                delete image;
                return orientation_status;
            }
        }
        *recognized = true;
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int parse_tiff_bigtiff_strip_image_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    bool* is_bigtiff,
    bool* recognized,
    PillowCImage** out_image)
{
    if (!is_bigtiff || !recognized || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *is_bigtiff = false;
    *recognized = false;
    *out_image = nullptr;
    if (!tiff || tiff_size < 16u) {
        return PILLOW_C_OK;
    }
    const bool has_bigtiff_magic =
        (tiff[0] == 'I' && tiff[1] == 'I' && read_tiff16(tiff + 2u, true) == 43u) ||
        (tiff[0] == 'M' && tiff[1] == 'M' && read_tiff16(tiff + 2u, false) == 43u);
    if (!has_bigtiff_magic) {
        return PILLOW_C_OK;
    }
    *is_bigtiff = true;

    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    auto read_scalar = [little_endian](const std::uint8_t* entry, std::uint64_t* out_value) -> bool {
        if (!entry || !out_value || read_tiff64(entry + 4u, little_endian) != 1u) {
            return false;
        }
        const std::uint16_t type = read_tiff16(entry + 2u, little_endian);
        if (type == 3u) {
            *out_value = read_tiff16(entry + 12u, little_endian);
            return true;
        }
        if (type == 4u) {
            *out_value = read_tiff32(entry + 12u, little_endian);
            return true;
        }
        if (type == 16u) {
            *out_value = read_tiff64(entry + 12u, little_endian);
            return true;
        }
        return false;
    };
    auto read_short_array = [tiff, tiff_size, little_endian](
        const std::uint8_t* entry,
        std::vector<std::uint32_t>* out_values) -> bool {
        if (!entry || !out_values || read_tiff16(entry + 2u, little_endian) != 3u) {
            return false;
        }
        const std::uint64_t count = read_tiff64(entry + 4u, little_endian);
        if (count == 0u || count > std::numeric_limits<std::size_t>::max() / 2u) {
            return false;
        }
        const std::size_t value_size = static_cast<std::size_t>(count) * 2u;
        out_values->clear();
        out_values->resize(static_cast<std::size_t>(count));
        if (value_size <= 8u) {
            for (std::size_t index = 0u; index < out_values->size(); ++index) {
                (*out_values)[index] = read_tiff16(entry + 12u + index * 2u, little_endian);
            }
            return true;
        }
        const std::uint64_t value_offset = read_tiff64(entry + 12u, little_endian);
        if (value_offset > static_cast<std::uint64_t>(tiff_size) ||
            static_cast<std::uint64_t>(value_size) > static_cast<std::uint64_t>(tiff_size) - value_offset) {
            out_values->clear();
            return false;
        }
        for (std::size_t index = 0u; index < out_values->size(); ++index) {
            (*out_values)[index] = read_tiff16(
                tiff + static_cast<std::size_t>(value_offset) + index * 2u,
                little_endian);
        }
        return true;
    };

    std::uint64_t width = 0u;
    std::uint64_t height = 0u;
    std::vector<std::uint32_t> bits_per_sample;
    std::uint64_t compression = 1u;
    std::uint64_t photometric = 0u;
    std::uint64_t samples_per_pixel = 1u;
    std::uint64_t extra_samples = 0u;
    std::uint64_t sample_format = 1u;
    std::uint64_t planar_config = 1u;
    std::uint64_t strip_offset = 0u;
    std::uint64_t strip_byte_count = 0u;
    bool has_width = false;
    bool has_height = false;
    bool has_bits = false;
    bool has_photometric = false;
    bool has_samples_per_pixel = false;
    bool has_extra_samples = false;
    bool has_sample_format = false;
    bool has_planar_config = false;
    bool has_strip_offset = false;
    bool has_strip_byte_count = false;

    if (entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 20u;
        const std::uint16_t tag = read_tiff16(entry, little_endian);
        switch (tag) {
        case 256u:
            has_width = read_scalar(entry, &width);
            break;
        case 257u:
            has_height = read_scalar(entry, &height);
            break;
        case 258u:
            has_bits = read_short_array(entry, &bits_per_sample);
            break;
        case 259u:
            read_scalar(entry, &compression);
            break;
        case 262u:
            has_photometric = read_scalar(entry, &photometric);
            break;
        case 273u:
            has_strip_offset = read_scalar(entry, &strip_offset);
            break;
        case 277u:
            has_samples_per_pixel = read_scalar(entry, &samples_per_pixel);
            break;
        case 279u:
            has_strip_byte_count = read_scalar(entry, &strip_byte_count);
            break;
        case 284u:
            has_planar_config = read_scalar(entry, &planar_config);
            break;
        case 338u:
            has_extra_samples = read_scalar(entry, &extra_samples);
            break;
        case 339u:
            has_sample_format = read_scalar(entry, &sample_format);
            break;
        default:
            break;
        }
    }

    const bool supported_compression =
        compression == TIFF_COMPRESSION_NONE ||
        compression == TIFF_COMPRESSION_PACKBITS ||
        compression == TIFF_COMPRESSION_LZW ||
        compression == TIFF_COMPRESSION_ADOBE_DEFLATE;
    const bool matches_l =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 1u && bits_per_sample[0] == 8u && photometric == 1u &&
        supported_compression &&
        (!has_samples_per_pixel || samples_per_pixel == 1u);
    const bool matches_rgb =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 3u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && photometric == 2u &&
        supported_compression &&
        has_samples_per_pixel && samples_per_pixel == 3u;
    const bool matches_rgba =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 4u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        photometric == 2u && supported_compression &&
        has_samples_per_pixel && samples_per_pixel == 4u &&
        has_extra_samples && extra_samples == 2u;
    const bool matches_la =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 2u && bits_per_sample[0] == 8u && bits_per_sample[1] == 8u &&
        photometric == 1u && supported_compression &&
        has_samples_per_pixel && samples_per_pixel == 2u &&
        has_extra_samples && extra_samples == 2u;
    const bool matches_i16 =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 1u && bits_per_sample[0] == 16u && photometric == 1u &&
        supported_compression &&
        (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        (!has_sample_format || sample_format == 1u) &&
        (!has_planar_config || planar_config == 1u);
    const bool matches_i =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 1u && bits_per_sample[0] == 32u && photometric == 1u &&
        supported_compression &&
        (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        has_sample_format && sample_format == 2u &&
        (!has_planar_config || planar_config == 1u);
    const bool matches_f =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 1u && bits_per_sample[0] == 32u && photometric == 1u &&
        supported_compression &&
        (!has_samples_per_pixel || samples_per_pixel == 1u) &&
        has_sample_format && sample_format == 3u &&
        (!has_planar_config || planar_config == 1u);
    const bool matches_cmyk =
        has_width && has_height && has_bits && has_photometric &&
        has_strip_offset && has_strip_byte_count &&
        bits_per_sample.size() == 4u && bits_per_sample[0] == 8u &&
        bits_per_sample[1] == 8u && bits_per_sample[2] == 8u && bits_per_sample[3] == 8u &&
        photometric == 5u && supported_compression &&
        has_samples_per_pixel && samples_per_pixel == 4u &&
        !has_extra_samples &&
        (!has_planar_config || planar_config == 1u);
    if (!matches_l && !matches_rgb && !matches_rgba && !matches_la &&
        !matches_i16 && !matches_i && !matches_f && !matches_cmyk) {
        return PILLOW_C_OK;
    }
    if (width == 0u || height == 0u ||
        width > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int mode = matches_i16
        ? (little_endian ? PILLOW_C_MODE_I16 : PILLOW_C_MODE_I16B)
        : (matches_i
            ? PILLOW_C_MODE_I
            : (matches_f
                ? PILLOW_C_MODE_F
                : (matches_cmyk
                    ? PILLOW_C_MODE_CMYK
                    : (matches_rgba
                        ? PILLOW_C_MODE_RGBA
                        : (matches_la ? PILLOW_C_MODE_LA : (matches_rgb ? PILLOW_C_MODE_RGB : PILLOW_C_MODE_L))))));
    const int channels = matches_i16
        ? 2
        : ((matches_i || matches_f || matches_cmyk || matches_rgba)
            ? 4
            : (matches_la ? 2 : (matches_rgb ? 3 : 1)));
    std::size_t stride = 0u;
    std::size_t image_size = 0u;
    if (!checked_image_size(static_cast<int>(width), static_cast<int>(height), channels, &stride, &image_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (strip_offset > static_cast<std::uint64_t>(tiff_size) ||
        strip_byte_count > static_cast<std::uint64_t>(tiff_size) - strip_offset) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        const std::uint8_t* strip_source = tiff + static_cast<std::size_t>(strip_offset);
        std::vector<std::uint8_t> strip_storage;
        if (compression != TIFF_COMPRESSION_NONE) {
            if (!tiff_decode_tiled_payload(
                    strip_source,
                    static_cast<std::size_t>(strip_byte_count),
                    static_cast<std::uint16_t>(compression),
                    image_size,
                    &strip_storage) ||
                strip_storage.size() != image_size) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            strip_source = strip_storage.data();
        } else if (strip_byte_count != static_cast<std::uint64_t>(image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // Big-endian I/F strip samples are byte-swapped into little-endian
        // storage (Pillow normalizes I/F to native byte order); I16 keeps its
        // raw big-endian bytes under PILLOW_C_MODE_I16B.
        std::vector<std::uint8_t> numeric_storage;
        if (!little_endian && (matches_i || matches_f)) {
            numeric_storage.assign(strip_source, strip_source + image_size);
            for (std::size_t index = 0u; index + 4u <= image_size; index += 4u) {
                std::swap(numeric_storage[index], numeric_storage[index + 3u]);
                std::swap(numeric_storage[index + 1u], numeric_storage[index + 2u]);
            }
            strip_source = numeric_storage.data();
        }
        auto* image = new PillowCImage{
            static_cast<int>(width),
            static_cast<int>(height),
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(strip_source, strip_source + image_size)};
        *recognized = true;
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

struct TiffExifCollector {
    std::vector<int> ascii_tags;
    std::vector<std::string> ascii_values;
    std::vector<int> uint_tags;
    std::vector<std::uint32_t> uint_values;
    std::vector<int> uint_types;
    std::vector<int> rational_tags;
    std::vector<std::uint32_t> rational_numerators;
    std::vector<std::uint32_t> rational_denominators;
    std::vector<int> rational_array_tags;
    std::vector<std::uint32_t> rational_array_numerators;
    std::vector<std::uint32_t> rational_array_denominators;
    std::vector<std::size_t> rational_array_offsets;
    std::vector<std::size_t> rational_array_counts;
    std::vector<int> short_array_tags;
    std::vector<std::uint32_t> short_array_values;
    std::vector<std::size_t> short_array_offsets;
    std::vector<std::size_t> short_array_counts;
    std::vector<int> uint_array_tags;
    std::vector<std::uint32_t> uint_array_values;
    std::vector<std::size_t> uint_array_offsets;
    std::vector<std::size_t> uint_array_counts;
    std::vector<int> double_array_tags;
    std::vector<double> double_array_values;
    std::vector<std::size_t> double_array_offsets;
    std::vector<std::size_t> double_array_counts;
    std::vector<int> float_array_tags;
    std::vector<float> float_array_values;
    std::vector<std::size_t> float_array_offsets;
    std::vector<std::size_t> float_array_counts;
    std::vector<int> byte_array_tags;
    std::vector<std::uint8_t> byte_array_values;
    std::vector<std::size_t> byte_array_offsets;
    std::vector<std::size_t> byte_array_counts;
    std::vector<int> signed_rational_tags;
    std::vector<std::int32_t> signed_rational_numerators;
    std::vector<std::int32_t> signed_rational_denominators;
    std::vector<int> signed_rational_array_tags;
    std::vector<std::int32_t> signed_rational_array_numerators;
    std::vector<std::int32_t> signed_rational_array_denominators;
    std::vector<std::size_t> signed_rational_array_offsets;
    std::vector<std::size_t> signed_rational_array_counts;
    std::vector<int> undefined_tags;
    std::vector<std::uint8_t> undefined_values;
    std::vector<std::size_t> undefined_offsets;
    std::vector<std::size_t> undefined_counts;
    std::vector<std::uint32_t> sub_ifd_offsets;
    bool has_x_resolution = false;
    bool has_y_resolution = false;
    bool has_resolution_unit = false;
    std::uint32_t x_resolution_numerator = 0u;
    std::uint32_t x_resolution_denominator = 0u;
    std::uint32_t y_resolution_numerator = 0u;
    std::uint32_t y_resolution_denominator = 0u;
    std::uint32_t resolution_unit = 0u;
    int resolution_unit_type = 0;
};


void collect_tiff_bigtiff_exif_entries(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* first_entry,
    std::uint64_t entry_count,
    TiffExifCollector* collector);


int build_tiff_bigtiff_common_ascii_exif_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    int orientation,
    std::vector<std::uint8_t>* out_exif)
{
    if (!out_exif) {
        return PILLOW_C_NULL_POINTER;
    }
    out_exif->clear();
    if (!tiff || tiff_size < 16u) {
        return PILLOW_C_OK;
    }

    bool little_endian = false;
    std::uint64_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_bigtiff_ifd(
            tiff,
            tiff_size,
            ifd_index,
            &little_endian,
            &entry_count,
            &entries_offset)) {
        return PILLOW_C_OK;
    }
    if (entry_count > std::numeric_limits<std::size_t>::max() / 20u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    // Bounded BigTIFF per-IFD surface: ICCProfile (34675) and XMP (700) blobs,
    // the scalar orientation tag, and the full classic common-EXIF family matrix
    // (ASCII, scalar uint, uint arrays, SHORT arrays, rationals, rational arrays,
    // signed rationals, signed rational arrays, double arrays, float arrays,
    // byte arrays, and undefined blobs), with BigTIFF inline/LONG8-offset layout.
    TiffExifCollector collector;
    collect_tiff_bigtiff_exif_entries(tiff, tiff_size, little_endian, tiff + entries_offset, entry_count, &collector);
    for (std::size_t sub_index = 0u; sub_index < collector.sub_ifd_offsets.size(); ++sub_index) {
        const std::uint32_t sub_offset = collector.sub_ifd_offsets[sub_index];
        if (static_cast<std::size_t>(sub_offset) + 8u > tiff_size) {
            continue;
        }
        const std::uint64_t sub_count = read_tiff64(tiff + sub_offset, little_endian);
        if (sub_count == 0u || sub_count > 4096u ||
            sub_count > (static_cast<std::uint64_t>(tiff_size) - static_cast<std::uint64_t>(sub_offset) - 8u) / 20u) {
            continue;
        }
        collect_tiff_bigtiff_exif_entries(
            tiff,
            tiff_size,
            little_endian,
            tiff + static_cast<std::size_t>(sub_offset) + 8u,
            sub_count,
            &collector);
    }
    const int serialized_orientation = orientation == 1 ? 1 : 0;
    const bool has_resolution_exif =
        collector.has_x_resolution && collector.has_y_resolution && collector.has_resolution_unit && collector.resolution_unit == 2u;
    if (collector.ascii_tags.empty() && collector.uint_tags.empty() && collector.rational_tags.empty() && collector.rational_array_tags.empty() &&
        collector.short_array_tags.empty() && collector.uint_array_tags.empty() && collector.double_array_tags.empty() && collector.float_array_tags.empty() &&
        collector.byte_array_tags.empty() && collector.signed_rational_tags.empty() && collector.signed_rational_array_tags.empty() &&
        collector.undefined_tags.empty() && serialized_orientation == 0 && !has_resolution_exif) {
        return PILLOW_C_OK;
    }

    std::vector<const char*> ascii_value_ptrs;
    ascii_value_ptrs.reserve(collector.ascii_values.size());
    for (const std::string& value : collector.ascii_values) {
        ascii_value_ptrs.push_back(value.c_str());
    }
    if (has_resolution_exif) {
        collector.uint_tags.push_back(296);
        collector.uint_values.push_back(collector.resolution_unit);
        collector.uint_types.push_back(collector.resolution_unit_type);
        collector.rational_tags.push_back(282);
        collector.rational_numerators.push_back(collector.x_resolution_numerator);
        collector.rational_denominators.push_back(collector.x_resolution_denominator);
        collector.rational_tags.push_back(283);
        collector.rational_numerators.push_back(collector.y_resolution_numerator);
        collector.rational_denominators.push_back(collector.y_resolution_denominator);
    }
    const std::size_t ascii_count = collector.ascii_tags.size();
    const std::size_t uint_count = collector.uint_tags.size();
    const std::size_t rational_count = collector.rational_tags.size();
    const std::size_t rational_array_count = collector.rational_array_tags.size();
    const std::size_t short_array_count = collector.short_array_tags.size();
    const std::size_t uint_array_count = collector.uint_array_tags.size();
    const std::size_t double_array_count = collector.double_array_tags.size();
    const std::size_t float_array_count = collector.float_array_tags.size();
    const std::size_t byte_array_count = collector.byte_array_tags.size();
    const std::size_t signed_rational_count = collector.signed_rational_tags.size();
    const std::size_t signed_rational_array_count = collector.signed_rational_array_tags.size();
    const std::size_t undefined_count = collector.undefined_tags.size();
    std::size_t required = 0u;
    int status = copy_exif_entries_internal_uint_array_bytes(
        serialized_orientation,
        ascii_count == 0u ? nullptr : collector.ascii_tags.data(),
        ascii_count == 0u ? nullptr : ascii_value_ptrs.data(),
        ascii_count,
        uint_count == 0u ? nullptr : collector.uint_tags.data(),
        uint_count == 0u ? nullptr : collector.uint_values.data(),
        uint_count == 0u ? nullptr : collector.uint_types.data(),
        uint_count,
        rational_count == 0u ? nullptr : collector.rational_tags.data(),
        rational_count == 0u ? nullptr : collector.rational_numerators.data(),
        rational_count == 0u ? nullptr : collector.rational_denominators.data(),
        rational_count,
        rational_array_count == 0u ? nullptr : collector.rational_array_tags.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_numerators.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_denominators.data(),
        collector.rational_array_numerators.size(),
        rational_array_count == 0u ? nullptr : collector.rational_array_offsets.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_counts.data(),
        rational_array_count,
        short_array_count == 0u ? nullptr : collector.short_array_tags.data(),
        short_array_count == 0u ? nullptr : collector.short_array_values.data(),
        collector.short_array_values.size(),
        short_array_count == 0u ? nullptr : collector.short_array_offsets.data(),
        short_array_count == 0u ? nullptr : collector.short_array_counts.data(),
        short_array_count,
        uint_array_count == 0u ? nullptr : collector.uint_array_tags.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_values.data(),
        collector.uint_array_values.size(),
        uint_array_count == 0u ? nullptr : collector.uint_array_offsets.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_counts.data(),
        uint_array_count,
        double_array_count == 0u ? nullptr : collector.double_array_tags.data(),
        double_array_count == 0u ? nullptr : collector.double_array_values.data(),
        collector.double_array_values.size(),
        double_array_count == 0u ? nullptr : collector.double_array_offsets.data(),
        double_array_count == 0u ? nullptr : collector.double_array_counts.data(),
        double_array_count,
        float_array_count == 0u ? nullptr : collector.float_array_tags.data(),
        float_array_count == 0u ? nullptr : collector.float_array_values.data(),
        collector.float_array_values.size(),
        float_array_count == 0u ? nullptr : collector.float_array_offsets.data(),
        float_array_count == 0u ? nullptr : collector.float_array_counts.data(),
        float_array_count,
        byte_array_count == 0u ? nullptr : collector.byte_array_tags.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_values.data(),
        collector.byte_array_values.size(),
        byte_array_count == 0u ? nullptr : collector.byte_array_offsets.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_counts.data(),
        byte_array_count,
        signed_rational_count == 0u ? nullptr : collector.signed_rational_tags.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_numerators.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_denominators.data(),
        signed_rational_count,
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_tags.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_numerators.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_denominators.data(),
        collector.signed_rational_array_numerators.size(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_offsets.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_counts.data(),
        signed_rational_array_count,
        undefined_count == 0u ? nullptr : collector.undefined_tags.data(),
        undefined_count == 0u ? nullptr : collector.undefined_values.data(),
        collector.undefined_values.size(),
        undefined_count == 0u ? nullptr : collector.undefined_offsets.data(),
        undefined_count == 0u ? nullptr : collector.undefined_counts.data(),
        undefined_count,
        nullptr,
        0u,
        &required);
    if (status != PILLOW_C_OK && status != PILLOW_C_NULL_POINTER) {
        return status;
    }
    if (required == 0u) {
        return PILLOW_C_OK;
    }

    out_exif->assign(required, std::uint8_t{0});
    status = copy_exif_entries_internal_uint_array_bytes(
        serialized_orientation,
        ascii_count == 0u ? nullptr : collector.ascii_tags.data(),
        ascii_count == 0u ? nullptr : ascii_value_ptrs.data(),
        ascii_count,
        uint_count == 0u ? nullptr : collector.uint_tags.data(),
        uint_count == 0u ? nullptr : collector.uint_values.data(),
        uint_count == 0u ? nullptr : collector.uint_types.data(),
        uint_count,
        rational_count == 0u ? nullptr : collector.rational_tags.data(),
        rational_count == 0u ? nullptr : collector.rational_numerators.data(),
        rational_count == 0u ? nullptr : collector.rational_denominators.data(),
        rational_count,
        rational_array_count == 0u ? nullptr : collector.rational_array_tags.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_numerators.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_denominators.data(),
        collector.rational_array_numerators.size(),
        rational_array_count == 0u ? nullptr : collector.rational_array_offsets.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_counts.data(),
        rational_array_count,
        short_array_count == 0u ? nullptr : collector.short_array_tags.data(),
        short_array_count == 0u ? nullptr : collector.short_array_values.data(),
        collector.short_array_values.size(),
        short_array_count == 0u ? nullptr : collector.short_array_offsets.data(),
        short_array_count == 0u ? nullptr : collector.short_array_counts.data(),
        short_array_count,
        uint_array_count == 0u ? nullptr : collector.uint_array_tags.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_values.data(),
        collector.uint_array_values.size(),
        uint_array_count == 0u ? nullptr : collector.uint_array_offsets.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_counts.data(),
        uint_array_count,
        double_array_count == 0u ? nullptr : collector.double_array_tags.data(),
        double_array_count == 0u ? nullptr : collector.double_array_values.data(),
        collector.double_array_values.size(),
        double_array_count == 0u ? nullptr : collector.double_array_offsets.data(),
        double_array_count == 0u ? nullptr : collector.double_array_counts.data(),
        double_array_count,
        float_array_count == 0u ? nullptr : collector.float_array_tags.data(),
        float_array_count == 0u ? nullptr : collector.float_array_values.data(),
        collector.float_array_values.size(),
        float_array_count == 0u ? nullptr : collector.float_array_offsets.data(),
        float_array_count == 0u ? nullptr : collector.float_array_counts.data(),
        float_array_count,
        byte_array_count == 0u ? nullptr : collector.byte_array_tags.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_values.data(),
        collector.byte_array_values.size(),
        byte_array_count == 0u ? nullptr : collector.byte_array_offsets.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_counts.data(),
        byte_array_count,
        signed_rational_count == 0u ? nullptr : collector.signed_rational_tags.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_numerators.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_denominators.data(),
        signed_rational_count,
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_tags.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_numerators.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_denominators.data(),
        collector.signed_rational_array_numerators.size(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_offsets.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_counts.data(),
        signed_rational_array_count,
        undefined_count == 0u ? nullptr : collector.undefined_tags.data(),
        undefined_count == 0u ? nullptr : collector.undefined_values.data(),
        collector.undefined_values.size(),
        undefined_count == 0u ? nullptr : collector.undefined_offsets.data(),
        undefined_count == 0u ? nullptr : collector.undefined_counts.data(),
        undefined_count,
        out_exif->data(),
        out_exif->size(),
        &required);
    if (status != PILLOW_C_OK) {
        out_exif->clear();
        return status;
    }
    return PILLOW_C_OK;
}

int attach_tiff_bigtiff_metadata_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    PillowCImage* image)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    std::vector<std::uint8_t> tiff_icc_profile;
    if (parse_tiff_bigtiff_icc_profile_for_ifd(tiff, tiff_size, ifd_index, &tiff_icc_profile)) {
        image->tiff_icc_profile = std::move(tiff_icc_profile);
    }
    std::vector<std::uint8_t> tiff_xmp;
    if (parse_tiff_bigtiff_xmp_for_ifd(tiff, tiff_size, ifd_index, &tiff_xmp)) {
        image->xmp = std::move(tiff_xmp);
    }
    const int orientation = read_tiff_bigtiff_orientation_for_ifd(tiff, tiff_size, ifd_index);
    const int status = build_tiff_bigtiff_common_ascii_exif_for_ifd(
        tiff,
        tiff_size,
        ifd_index,
        orientation,
        &image->tiff_exif);
    if (status != PILLOW_C_OK) {
        return status;
    }
    TiffResolutionMetadata resolution;
    if (parse_tiff_bigtiff_resolution_for_ifd(tiff, tiff_size, ifd_index, &resolution)) {
        image->has_dpi = true;
        image->dpi_x = resolution.dpi_x;
        image->dpi_y = resolution.dpi_y;
    }
    return PILLOW_C_OK;
}

bool parse_tiff_icc_profile_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    std::vector<std::uint8_t>* out_profile)
{
    if (!out_profile) {
        return false;
    }
    out_profile->clear();

    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(tiff, tiff_size, ifd_index, &little_endian, &entry_count, &entries_offset)) {
        return false;
    }

    for (std::uint16_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const int tag = static_cast<int>(read_tiff16(entry, little_endian));
        if (tag == 34675) {
            return read_tiff_icc_profile_entry_value(tiff, tiff_size, little_endian, entry, out_profile);
        }
    }
    return false;
}

bool parse_tiff_icc_profile(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    std::vector<std::uint8_t>* out_profile)
{
    return parse_tiff_icc_profile_for_ifd(tiff, tiff_size, 0, out_profile);
}

bool parse_tiff_xmp_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    std::vector<std::uint8_t>* out_xmp)
{
    if (!out_xmp) {
        return false;
    }
    out_xmp->clear();

    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(tiff, tiff_size, ifd_index, &little_endian, &entry_count, &entries_offset)) {
        return false;
    }

    for (std::uint16_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const int tag = static_cast<int>(read_tiff16(entry, little_endian));
        if (tag == 700) {
            return read_tiff_xmp_entry_value(tiff, tiff_size, little_endian, entry, out_xmp);
        }
    }
    return false;
}


void collect_tiff_exif_entries(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* first_entry,
    std::uint32_t entry_count,
    TiffExifCollector* collector)
{
    for (std::uint32_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = first_entry + static_cast<std::size_t>(index) * 12u;
        const int tag = static_cast<int>(read_tiff16(entry, little_endian));
        const std::uint16_t entry_value_type = read_tiff16(entry + 2u, little_endian);
        const std::uint32_t entry_value_count = read_tiff32(entry + 4u, little_endian);
        if (tiff_common_ascii_tag(tag)) {
            std::string value;
            if (!read_tiff_ascii_entry_value(tiff, tiff_size, little_endian, entry, &value)) {
                continue;
            }
            collector->ascii_tags.push_back(tag);
            collector->ascii_values.push_back(std::move(value));
        } else if (tag == 282 || tag == 283) {
            std::uint32_t numerator = 0u;
            std::uint32_t denominator = 0u;
            if (!read_tiff_rational_entry_value(tiff, tiff_size, little_endian, entry, &numerator, &denominator)) {
                continue;
            }
            if (tag == 282) {
                collector->has_x_resolution = true;
                collector->x_resolution_numerator = numerator;
                collector->x_resolution_denominator = denominator;
            } else {
                collector->has_y_resolution = true;
                collector->y_resolution_numerator = numerator;
                collector->y_resolution_denominator = denominator;
            }
        } else if (tag == 37377 || tag == 37379 || tag == 37380 || tag == 50716 || tag == 50730 || tag == 50739 ||
                   tag == 51044 || tag == 51109) {
            std::int32_t numerator = 0;
            std::int32_t denominator = 0;
            if (!read_tiff_signed_rational_entry_value(tiff, tiff_size, little_endian, entry, &numerator, &denominator)) {
                continue;
            }
            collector->signed_rational_tags.push_back(tag);
            collector->signed_rational_numerators.push_back(numerator);
            collector->signed_rational_denominators.push_back(denominator);
        } else if (tag == 50715 || (tag >= 50721 && tag <= 50726) || tag == 50832 || tag == 50834 || tag == 50964 ||
                   tag == 50965 || (tag >= 52530 && tag <= 52532)) {
            std::vector<std::int32_t> numerators;
            std::vector<std::int32_t> denominators;
            if (read_tiff_signed_rational_array_entry_value(
                    tiff,
                    tiff_size,
                    little_endian,
                    entry,
                    &numerators,
                    &denominators) &&
                ((tag == 50715 && numerators.size() == 2u) || (tag != 50715 && numerators.size() == 9u))) {
                collector->signed_rational_array_tags.push_back(tag);
                collector->signed_rational_array_offsets.push_back(collector->signed_rational_array_numerators.size());
                collector->signed_rational_array_counts.push_back(numerators.size());
                collector->signed_rational_array_numerators.insert(
                    collector->signed_rational_array_numerators.end(), numerators.begin(), numerators.end());
                collector->signed_rational_array_denominators.insert(
                    collector->signed_rational_array_denominators.end(), denominators.begin(), denominators.end());
            }
        } else if (tag == 286 || tag == 287 || tag == 33434 || tag == 33437 || tag == 37122 || tag == 37378 ||
            tag == 37381 || tag == 37382 || tag == 37386 || tag == 41483 || tag == 41486 || tag == 41487 ||
            tag == 41493 || tag == 41988 || tag == 42240 || tag == 50731 || tag == 50732 || tag == 50734 ||
            tag == 50737 || tag == 50738 || tag == 50780 || tag == 50935 || tag == 51058 || tag == 51112 || tag == 51178 || tag == 51179) {
            std::uint32_t numerator = 0u;
            std::uint32_t denominator = 0u;
            if (!read_tiff_rational_entry_value(tiff, tiff_size, little_endian, entry, &numerator, &denominator)) {
                continue;
            }
            collector->rational_tags.push_back(tag);
            collector->rational_numerators.push_back(numerator);
            collector->rational_denominators.push_back(denominator);
        } else if (tag == 318 || tag == 319 || tag == 529 || tag == 532 || tag == 42034 || tag == 42082 ||
            tag == 50714 || tag == 50718 || (tag == 50719 && entry_value_type == 5u) || (tag == 50720 && entry_value_type == 5u) || tag == 50727 || tag == 50728 || tag == 50729 || tag == 50736 ||
            (tag == 51091 && entry_value_type == 5u) || tag == 51125) {
            std::vector<std::uint32_t> numerators;
            std::vector<std::uint32_t> denominators;
            if (read_tiff_rational_array_entry_value(tiff, tiff_size, little_endian, entry, &numerators, &denominators) &&
                ((tag == 318 && numerators.size() == 2u) ||
                    (tag == 319 && numerators.size() == 6u) ||
                    (tag == 529 && numerators.size() == 3u) ||
                    (tag == 532 && numerators.size() == 6u) ||
                    ((tag == 42034 || tag == 50714) && numerators.size() == 4u) ||
                    ((tag == 42082 || tag == 50718 || tag == 50719 || tag == 50720) && numerators.size() == 2u) ||
                    ((tag == 50727 || tag == 50728) && numerators.size() == 3u) ||
                    (tag == 50729 && numerators.size() == 2u) ||
                    (tag == 51091 && numerators.size() == 2u) ||
                    ((tag == 50736 || tag == 51125) && numerators.size() == 4u))) {
                collector->rational_array_tags.push_back(tag);
                collector->rational_array_offsets.push_back(collector->rational_array_numerators.size());
                collector->rational_array_counts.push_back(numerators.size());
                collector->rational_array_numerators.insert(collector->rational_array_numerators.end(), numerators.begin(), numerators.end());
                collector->rational_array_denominators.insert(collector->rational_array_denominators.end(), denominators.begin(), denominators.end());
            }
        } else if (tag == 296) {
            std::uint32_t value = 0u;
            int value_type = 0;
            if (!read_tiff_uint_entry_value(little_endian, entry, &value, &value_type)) {
                continue;
            }
            collector->has_resolution_unit = true;
            collector->resolution_unit = value;
            collector->resolution_unit_type = value_type;
        } else if (tag == 291 || tag == 297 || tag == 301 || tag == 320 || tag == 321 || tag == 336 || tag == 342 || tag == 530 || tag == 34735 || tag == 37396 || tag == 41492 || tag == 42081 || tag == 50712 || tag == 50713 || ((tag == 50719 || tag == 50720) && entry_value_type == 3u) ||
            (tag == 50829 && entry_value_type == 3u)) {
            std::vector<std::uint32_t> values;
            if (read_tiff_ushort_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (((tag == 291 || tag == 301) && values.size() == 3u) ||
                    (tag == 342 && values.size() == 6u) ||
                    (tag == 34735 && values.size() == 8u) ||
                    (tag == 37396 && (values.size() == 2u || values.size() == 3u || values.size() == 4u)) ||
                    (tag == 50712 && values.size() == 4u) ||
                    (tag == 50829 && values.size() == 4u) ||
                    (tag == 320 && values.size() == 768u) ||
                    (tag != 291 && tag != 301 && tag != 320 && tag != 342 && tag != 34735 && tag != 37396 && tag != 50712 && values.size() == 2u))) {
                collector->short_array_tags.push_back(tag);
                collector->short_array_offsets.push_back(collector->short_array_values.size());
                collector->short_array_counts.push_back(values.size());
                collector->short_array_values.insert(collector->short_array_values.end(), values.begin(), values.end());
            }
        } else if (((tag == 50719 || tag == 50720) && entry_value_type == 4u) || ((tag == 50829 || tag == 50830) && entry_value_type == 4u) || tag == 50937 || tag == 50981 || tag == 51089 || tag == 51090 ||
            (tag == 51091 && entry_value_type == 4u) || tag == 52536 ||
            ((tag == 273 || tag == 279) && entry_value_count > 1u) ||
            ((tag == 324 || tag == 325) && entry_value_count > 1u)) {
            std::vector<std::uint32_t> values;
            const std::size_t expected_count =
                (tag == 50937 || tag == 50981) ? 3u :
                ((tag == 50719 || tag == 50720 || tag == 51089 || tag == 51090 || tag == 51091) ? 2u :
                    ((tag == 50829 || tag == 52536) ? 4u : static_cast<std::size_t>(entry_value_count)));
            if (read_tiff_uint_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                ((tag == 50830 && (values.size() == 4u || values.size() == 8u)) ||
                    (tag != 50830 && values.size() == expected_count))) {
                collector->uint_array_tags.push_back(tag);
                collector->uint_array_offsets.push_back(collector->uint_array_values.size());
                collector->uint_array_counts.push_back(values.size());
                collector->uint_array_values.insert(collector->uint_array_values.end(), values.begin(), values.end());
            }
        } else if (tag == 33550 || tag == 33922 || tag == 34264 || tag == 34736 || tag == 50844 || tag == 51041) {
            std::vector<double> values;
            const std::size_t expected_count =
                tag == 33550 ? 3u : (tag == 33922 ? 6u : (tag == 34264 ? 16u : (tag == 34736 ? 3u : (tag == 50844 ? 92u : 6u))));
            if (read_tiff_double_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                ((tag == 51041 && (values.size() == 2u || values.size() == 4u || values.size() == 6u || values.size() == 8u)) ||
                    (tag != 51041 && values.size() == expected_count))) {
                collector->double_array_tags.push_back(tag);
                collector->double_array_offsets.push_back(collector->double_array_values.size());
                collector->double_array_counts.push_back(values.size());
                collector->double_array_values.insert(collector->double_array_values.end(), values.begin(), values.end());
            }
        } else if (tag == 50938 || tag == 50939 || tag == 50940 || tag == 50982) {
            std::vector<float> values;
            if (read_tiff_float_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (values.size() == 6u ||
                    ((tag == 50940 || tag == 50982) && values.size() == 18u) ||
                    ((tag == 50938 || tag == 50939 || tag == 50982) && values.size() == 54u))) {
                collector->float_array_tags.push_back(tag);
                collector->float_array_offsets.push_back(collector->float_array_values.size());
                collector->float_array_counts.push_back(values.size());
                collector->float_array_values.insert(collector->float_array_values.end(), values.begin(), values.end());
            }
        } else if (tiff_common_byte_array_tag(tag)) {
            std::vector<std::uint8_t> values;
            if (read_tiff_byte_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (((tag == 50781 || tag == 50972 || tag == 50973) && values.size() == 16u) ||
                 (tag != 50781 && tag != 50972 && tag != 50973 &&
                  ((tag != 50831 && tag != 50833 && tag != 51043) || values.size() == 8u)))) {
                collector->byte_array_tags.push_back(tag);
                collector->byte_array_offsets.push_back(collector->byte_array_values.size());
                collector->byte_array_counts.push_back(values.size());
                collector->byte_array_values.insert(collector->byte_array_values.end(), values.begin(), values.end());
            }
        } else if (tag == 258) {
            std::vector<std::uint32_t> values;
            if (read_tiff_ushort_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (values.size() == 2u || values.size() == 3u || values.size() == 4u)) {
                collector->short_array_tags.push_back(tag);
                collector->short_array_offsets.push_back(collector->short_array_values.size());
                collector->short_array_counts.push_back(values.size());
                collector->short_array_values.insert(collector->short_array_values.end(), values.begin(), values.end());
                continue;
            }
            std::uint32_t value = 0u;
            int value_type = 0;
            if (!read_tiff_uint_entry_value(little_endian, entry, &value, &value_type)) {
                continue;
            }
            if (tag == 50879 && value_type != 3) {
                continue;
            }
            collector->uint_tags.push_back(tag);
            collector->uint_values.push_back(value);
            collector->uint_types.push_back(value_type);
        } else if (tag == 34675) {
            std::vector<std::uint8_t> values;
            if (read_tiff_icc_profile_entry_value(tiff, tiff_size, little_endian, entry, &values)) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        } else if (tag == 700) {
            std::vector<std::uint8_t> values;
            if (read_tiff_xmp_entry_value(tiff, tiff_size, little_endian, entry, &values)) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        } else if ((tag == 37510 || tag == 37724) && read_tiff16(entry + 2u, little_endian) == 7u) {
            std::vector<std::uint8_t> values;
            if (read_tiff_undefined_entry_value(tiff, tiff_size, little_endian, entry, &values)) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        } else if (tag == 347 || tag == 33723 || tag == 34856 || tag == 36864 || tag == 37121 || tag == 37500 ||
            tag == 40960 || tag == 41484 || tag == 41728 || tag == 41729 || tag == 41730 || tag == 41995 ||
            tag == 50828 || tag == 50969 || tag == 51008 || tag == 51009 || tag == 51022 || tag == 52525 ||
            (tag >= 52533 && tag <= 52535)) {
            std::vector<std::uint8_t> values;
            if (read_tiff_undefined_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                ((tag == 50969 && values.size() == 16u) ||
                 (tag == 52525 && (values.size() == 4u || values.size() == 8u)) ||
                 (tag != 50969 && tag != 52525 &&
                  ((tag != 50828 && tag != 51008 && tag != 51009 && tag != 51022) || values.size() == 8u)))) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        } else if (tiff_gps_ascii_tag(tag)) {
            std::string value;
            if (read_tiff_ascii_entry_value(tiff, tiff_size, little_endian, entry, &value)) {
                collector->ascii_tags.push_back(tag);
                collector->ascii_values.push_back(std::move(value));
            }
        } else if (tag == 2 || tag == 4 || tag == 20 || tag == 22) {
            std::vector<std::uint32_t> numerators;
            std::vector<std::uint32_t> denominators;
            if (read_tiff_rational_array_entry_value(tiff, tiff_size, little_endian, entry, &numerators, &denominators) &&
                numerators.size() == 3u) {
                collector->rational_array_tags.push_back(tag);
                collector->rational_array_offsets.push_back(collector->rational_array_numerators.size());
                collector->rational_array_counts.push_back(numerators.size());
                collector->rational_array_numerators.insert(
                    collector->rational_array_numerators.end(), numerators.begin(), numerators.end());
                collector->rational_array_denominators.insert(
                    collector->rational_array_denominators.end(), denominators.begin(), denominators.end());
            }
        } else if (tiff_gps_rational_tag(tag)) {
            std::uint32_t numerator = 0u;
            std::uint32_t denominator = 0u;
            if (read_tiff_rational_entry_value(tiff, tiff_size, little_endian, entry, &numerator, &denominator)) {
                collector->rational_tags.push_back(tag);
                collector->rational_numerators.push_back(numerator);
                collector->rational_denominators.push_back(denominator);
            }
        } else if (tiff_gps_uint_tag(tag)) {
            std::uint32_t value = 0u;
            int value_type = 0;
            if (read_tiff_uint_entry_value(little_endian, entry, &value, &value_type)) {
                collector->uint_tags.push_back(tag);
                collector->uint_values.push_back(value);
                collector->uint_types.push_back(value_type);
            }
        } else if (tag == 0) {
            std::vector<std::uint8_t> values;
            if (read_tiff_byte_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                values.size() == 4u) {
                collector->byte_array_tags.push_back(tag);
                collector->byte_array_offsets.push_back(collector->byte_array_values.size());
                collector->byte_array_counts.push_back(values.size());
                collector->byte_array_values.insert(collector->byte_array_values.end(), values.begin(), values.end());
            }
        } else if (tiff_common_uint_tag(tag)) {
            std::uint32_t value = 0u;
            int value_type = 0;
            if (!read_tiff_uint_entry_value(little_endian, entry, &value, &value_type)) {
                continue;
            }
            if ((tag == 50717 && value_type != 3) || ((tag == 50974 || tag == 50975) && value_type != 4)) {
                continue;
            }
            collector->uint_tags.push_back(tag);
            collector->uint_values.push_back(value);
            collector->uint_types.push_back(value_type);
            if ((tag == 34665 || tag == 34853) && value_type == 4u) {
                collector->sub_ifd_offsets.push_back(value);
            }
        }    }
}

void collect_tiff_bigtiff_exif_entries(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* first_entry,
    std::uint64_t entry_count,
    TiffExifCollector* collector)
{
    for (std::uint64_t index = 0u; index < entry_count; ++index) {
        const std::uint8_t* entry = first_entry + static_cast<std::size_t>(index) * 20u;
        const int tag = static_cast<int>(read_tiff16(entry, little_endian));
        const std::uint16_t entry_value_type = read_tiff16(entry + 2u, little_endian);
        const std::uint64_t entry_value_count = read_tiff64(entry + 4u, little_endian);
        if (tiff_common_ascii_tag(tag)) {
            std::string value;
            if (read_tiff_bigtiff_ascii_entry_value(tiff, tiff_size, little_endian, entry, &value)) {
                collector->ascii_tags.push_back(tag);
                collector->ascii_values.push_back(std::move(value));
            }
        } else if (tag == 282 || tag == 283) {
            std::uint32_t numerator = 0u;
            std::uint32_t denominator = 0u;
            if (!read_tiff_bigtiff_rational_entry_value(little_endian, entry, &numerator, &denominator)) {
                continue;
            }
            if (tag == 282) {
                collector->has_x_resolution = true;
                collector->x_resolution_numerator = numerator;
                collector->x_resolution_denominator = denominator;
            } else {
                collector->has_y_resolution = true;
                collector->y_resolution_numerator = numerator;
                collector->y_resolution_denominator = denominator;
            }
        } else if (tag == 37377 || tag == 37379 || tag == 37380 || tag == 50716 || tag == 50730 || tag == 50739 ||
                   tag == 51044 || tag == 51109) {
            std::int32_t numerator = 0;
            std::int32_t denominator = 0;
            if (!read_tiff_bigtiff_signed_rational_entry_value(little_endian, entry, &numerator, &denominator)) {
                continue;
            }
            collector->signed_rational_tags.push_back(tag);
            collector->signed_rational_numerators.push_back(numerator);
            collector->signed_rational_denominators.push_back(denominator);
        } else if (tag == 50715 || (tag >= 50721 && tag <= 50726) || tag == 50832 || tag == 50834 || tag == 50964 ||
                   tag == 50965 || (tag >= 52530 && tag <= 52532)) {
            std::vector<std::int32_t> numerators;
            std::vector<std::int32_t> denominators;
            if (read_tiff_bigtiff_signed_rational_array_entry_value(
                    tiff,
                    tiff_size,
                    little_endian,
                    entry,
                    &numerators,
                    &denominators) &&
                ((tag == 50715 && numerators.size() == 2u) || (tag != 50715 && numerators.size() == 9u))) {
                collector->signed_rational_array_tags.push_back(tag);
                collector->signed_rational_array_offsets.push_back(collector->signed_rational_array_numerators.size());
                collector->signed_rational_array_counts.push_back(numerators.size());
                collector->signed_rational_array_numerators.insert(
                    collector->signed_rational_array_numerators.end(), numerators.begin(), numerators.end());
                collector->signed_rational_array_denominators.insert(
                    collector->signed_rational_array_denominators.end(), denominators.begin(), denominators.end());
            }
        } else if (tag == 286 || tag == 287 || tag == 33434 || tag == 33437 || tag == 37122 || tag == 37378 ||
            tag == 37381 || tag == 37382 || tag == 37386 || tag == 41483 || tag == 41486 || tag == 41487 ||
            tag == 41493 || tag == 41988 || tag == 42240 || tag == 50731 || tag == 50732 || tag == 50734 ||
            tag == 50737 || tag == 50738 || tag == 50780 || tag == 50935 || tag == 51058 || tag == 51112 || tag == 51178 || tag == 51179) {
            std::uint32_t numerator = 0u;
            std::uint32_t denominator = 0u;
            if (!read_tiff_bigtiff_rational_entry_value(little_endian, entry, &numerator, &denominator)) {
                continue;
            }
            collector->rational_tags.push_back(tag);
            collector->rational_numerators.push_back(numerator);
            collector->rational_denominators.push_back(denominator);
        } else if (tag == 318 || tag == 319 || tag == 529 || tag == 532 || tag == 42034 || tag == 42082 ||
            tag == 50714 || tag == 50718 || (tag == 50719 && entry_value_type == 5u) || (tag == 50720 && entry_value_type == 5u) || tag == 50727 || tag == 50728 || tag == 50729 || tag == 50736 ||
            (tag == 51091 && entry_value_type == 5u) || tag == 51125) {
            std::vector<std::uint32_t> numerators;
            std::vector<std::uint32_t> denominators;
            if (read_tiff_bigtiff_rational_array_entry_value(tiff, tiff_size, little_endian, entry, &numerators, &denominators) &&
                ((tag == 318 && numerators.size() == 2u) ||
                    (tag == 319 && numerators.size() == 6u) ||
                    (tag == 529 && numerators.size() == 3u) ||
                    (tag == 532 && numerators.size() == 6u) ||
                    ((tag == 42034 || tag == 50714) && numerators.size() == 4u) ||
                    ((tag == 42082 || tag == 50718 || tag == 50719 || tag == 50720) && numerators.size() == 2u) ||
                    ((tag == 50727 || tag == 50728) && numerators.size() == 3u) ||
                    (tag == 50729 && numerators.size() == 2u) ||
                    (tag == 51091 && numerators.size() == 2u) ||
                    ((tag == 50736 || tag == 51125) && numerators.size() == 4u))) {
                collector->rational_array_tags.push_back(tag);
                collector->rational_array_offsets.push_back(collector->rational_array_numerators.size());
                collector->rational_array_counts.push_back(numerators.size());
                collector->rational_array_numerators.insert(collector->rational_array_numerators.end(), numerators.begin(), numerators.end());
                collector->rational_array_denominators.insert(collector->rational_array_denominators.end(), denominators.begin(), denominators.end());
            }
        } else if (tag == 296) {
            std::uint32_t value = 0u;
            int value_type = 0;
            if (!read_tiff_bigtiff_uint_scalar_entry_value(little_endian, entry, &value, &value_type)) {
                continue;
            }
            collector->has_resolution_unit = true;
            collector->resolution_unit = value;
            collector->resolution_unit_type = value_type;
        } else if (tag == 291 || tag == 297 || tag == 301 || tag == 320 || tag == 321 || tag == 336 ||
                   tag == 342 || tag == 530 || tag == 34735 || tag == 37396 || tag == 41492 || tag == 42081 ||
                   tag == 50712 || tag == 50713 ||
                   ((tag == 50719 || tag == 50720) && entry_value_type == 3u) ||
                   (tag == 50829 && entry_value_type == 3u)) {
            std::vector<std::uint32_t> values;
            if (read_tiff_bigtiff_ushort_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (((tag == 291 || tag == 301) && values.size() == 3u) ||
                    (tag == 342 && values.size() == 6u) ||
                    (tag == 34735 && values.size() == 8u) ||
                    (tag == 37396 && (values.size() == 2u || values.size() == 3u || values.size() == 4u)) ||
                    (tag == 50712 && values.size() == 4u) ||
                    (tag == 50829 && values.size() == 4u) ||
                    (tag == 320 && values.size() == 768u) ||
                    (tag != 291 && tag != 301 && tag != 320 && tag != 342 && tag != 34735 && tag != 37396 &&
                        tag != 50712 && values.size() == 2u))) {
                collector->short_array_tags.push_back(tag);
                collector->short_array_offsets.push_back(collector->short_array_values.size());
                collector->short_array_counts.push_back(values.size());
                collector->short_array_values.insert(collector->short_array_values.end(), values.begin(), values.end());
            }
        } else if (((tag == 50719 || tag == 50720) && entry_value_type == 4u) ||
                   ((tag == 50829 || tag == 50830) && entry_value_type == 4u) ||
                   tag == 50937 || tag == 50981 || tag == 51089 || tag == 51090 ||
                   (tag == 51091 && entry_value_type == 4u) || tag == 52536 ||
                   ((tag == 273 || tag == 279) && entry_value_count > 1u) ||
                   ((tag == 324 || tag == 325) && entry_value_count > 1u)) {
            std::vector<std::uint32_t> values;
            const std::size_t expected_count =
                (tag == 50937 || tag == 50981) ? 3u :
                ((tag == 50719 || tag == 50720 || tag == 51089 || tag == 51090 || tag == 51091) ? 2u :
                    ((tag == 50829 || tag == 52536) ? 4u : static_cast<std::size_t>(entry_value_count)));
            if (read_tiff_bigtiff_uint_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                ((tag == 50830 && (values.size() == 4u || values.size() == 8u)) ||
                    (tag != 50830 && values.size() == expected_count))) {
                collector->uint_array_tags.push_back(tag);
                collector->uint_array_offsets.push_back(collector->uint_array_values.size());
                collector->uint_array_counts.push_back(values.size());
                collector->uint_array_values.insert(collector->uint_array_values.end(), values.begin(), values.end());
            }
        } else if (tag == 33550 || tag == 33922 || tag == 34264 || tag == 34736 || tag == 50844 || tag == 51041) {
            std::vector<double> values;
            const std::size_t expected_count =
                tag == 33550 ? 3u : (tag == 33922 ? 6u : (tag == 34264 ? 16u : (tag == 34736 ? 3u : (tag == 50844 ? 92u : 6u))));
            if (read_tiff_bigtiff_double_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                ((tag == 51041 && (values.size() == 2u || values.size() == 4u || values.size() == 6u || values.size() == 8u)) ||
                    (tag != 51041 && values.size() == expected_count))) {
                collector->double_array_tags.push_back(tag);
                collector->double_array_offsets.push_back(collector->double_array_values.size());
                collector->double_array_counts.push_back(values.size());
                collector->double_array_values.insert(collector->double_array_values.end(), values.begin(), values.end());
            }
        } else if (tag == 50938 || tag == 50939 || tag == 50940 || tag == 50982) {
            std::vector<float> values;
            if (read_tiff_bigtiff_float_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (values.size() == 6u ||
                    ((tag == 50940 || tag == 50982) && values.size() == 18u) ||
                    ((tag == 50938 || tag == 50939 || tag == 50982) && values.size() == 54u))) {
                collector->float_array_tags.push_back(tag);
                collector->float_array_offsets.push_back(collector->float_array_values.size());
                collector->float_array_counts.push_back(values.size());
                collector->float_array_values.insert(collector->float_array_values.end(), values.begin(), values.end());
            }
        } else if (tiff_common_byte_array_tag(tag)) {
            std::vector<std::uint8_t> values;
            if (read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                (((tag == 50781 || tag == 50972 || tag == 50973) && values.size() == 16u) ||
                 (tag != 50781 && tag != 50972 && tag != 50973 &&
                  ((tag != 50831 && tag != 50833 && tag != 51043) || values.size() == 8u)))) {
                collector->byte_array_tags.push_back(tag);
                collector->byte_array_offsets.push_back(collector->byte_array_values.size());
                collector->byte_array_counts.push_back(values.size());
                collector->byte_array_values.insert(collector->byte_array_values.end(), values.begin(), values.end());
            }
        } else if (tiff_gps_ascii_tag(tag)) {
            std::string value;
            if (read_tiff_bigtiff_ascii_entry_value(tiff, tiff_size, little_endian, entry, &value)) {
                collector->ascii_tags.push_back(tag);
                collector->ascii_values.push_back(std::move(value));
            }
        } else if (tag == 2 || tag == 4 || tag == 20 || tag == 22) {
            std::vector<std::uint32_t> numerators;
            std::vector<std::uint32_t> denominators;
            if (read_tiff_bigtiff_rational_array_entry_value(tiff, tiff_size, little_endian, entry, &numerators, &denominators) &&
                numerators.size() == 3u) {
                collector->rational_array_tags.push_back(tag);
                collector->rational_array_offsets.push_back(collector->rational_array_numerators.size());
                collector->rational_array_counts.push_back(numerators.size());
                collector->rational_array_numerators.insert(
                    collector->rational_array_numerators.end(), numerators.begin(), numerators.end());
                collector->rational_array_denominators.insert(
                    collector->rational_array_denominators.end(), denominators.begin(), denominators.end());
            }
        } else if (tiff_gps_rational_tag(tag)) {
            std::uint32_t numerator = 0u;
            std::uint32_t denominator = 0u;
            if (read_tiff_bigtiff_rational_entry_value(little_endian, entry, &numerator, &denominator)) {
                collector->rational_tags.push_back(tag);
                collector->rational_numerators.push_back(numerator);
                collector->rational_denominators.push_back(denominator);
            }
        } else if (tiff_gps_uint_tag(tag)) {
            std::uint32_t value = 0u;
            int value_type = 0;
            if (read_tiff_bigtiff_uint_scalar_entry_value(little_endian, entry, &value, &value_type)) {
                collector->uint_tags.push_back(tag);
                collector->uint_values.push_back(value);
                collector->uint_types.push_back(value_type);
            }
        } else if (tag == 0) {
            std::vector<std::uint8_t> values;
            if (read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                values.size() == 4u) {
                collector->byte_array_tags.push_back(tag);
                collector->byte_array_offsets.push_back(collector->byte_array_values.size());
                collector->byte_array_counts.push_back(values.size());
                collector->byte_array_values.insert(collector->byte_array_values.end(), values.begin(), values.end());
            }
        } else if (tiff_common_uint_tag(tag)) {
            if (tag == 258 && entry_value_type == 3u && entry_value_count > 1u) {
                std::vector<std::uint32_t> values;
                if (read_tiff_bigtiff_ushort_array_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                    (values.size() == 2u || values.size() == 3u || values.size() == 4u)) {
                    collector->short_array_tags.push_back(tag);
                    collector->short_array_offsets.push_back(collector->short_array_values.size());
                    collector->short_array_counts.push_back(values.size());
                    collector->short_array_values.insert(collector->short_array_values.end(), values.begin(), values.end());
                    continue;
                }
            }
            std::uint32_t value = 0u;
            int value_type = 0;
            if (read_tiff_bigtiff_uint_scalar_entry_value(little_endian, entry, &value, &value_type)) {
                if ((tag == 50717 && value_type != 3) || ((tag == 50974 || tag == 50975) && value_type != 4)) {
                    continue;
                }
                collector->uint_tags.push_back(tag);
                collector->uint_values.push_back(value);
                collector->uint_types.push_back(value_type);
            if ((tag == 34665 || tag == 34853) && value_type == 4u) {
                collector->sub_ifd_offsets.push_back(value);
            }
            }
        } else if (tag == 34675 || tag == 700) {
            std::vector<std::uint8_t> values;
            if (read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, &values)) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        } else if ((tag == 37510 || tag == 37724) && entry_value_type == 7u) {
            std::vector<std::uint8_t> values;
            if (read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, &values)) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        } else if (tag == 347 || tag == 33723 || tag == 34856 || tag == 36864 || tag == 37121 || tag == 37500 ||
            tag == 40960 || tag == 41484 || tag == 41728 || tag == 41729 || tag == 41730 || tag == 41995 ||
            tag == 50828 || tag == 50969 || tag == 51008 || tag == 51009 || tag == 51022 || tag == 52525 ||
            (tag >= 52533 && tag <= 52535)) {
            std::vector<std::uint8_t> values;
            if (read_tiff_bigtiff_blob_entry_value(tiff, tiff_size, little_endian, entry, &values) &&
                ((tag == 50969 && values.size() == 16u) ||
                 (tag == 52525 && (values.size() == 4u || values.size() == 8u)) ||
                 (tag != 50969 && tag != 52525 &&
                  ((tag != 50828 && tag != 51008 && tag != 51009 && tag != 51022) || values.size() == 8u)))) {
                collector->undefined_tags.push_back(tag);
                collector->undefined_offsets.push_back(collector->undefined_values.size());
                collector->undefined_counts.push_back(values.size());
                collector->undefined_values.insert(collector->undefined_values.end(), values.begin(), values.end());
            }
        }    }
}

int build_tiff_common_ascii_exif_for_ifd(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int ifd_index,
    int orientation,
    std::vector<std::uint8_t>* out_exif)
{
    if (!out_exif) {
        return PILLOW_C_NULL_POINTER;
    }
    out_exif->clear();
    if (!tiff || tiff_size < 8u) {
        return PILLOW_C_OK;
    }

    bool little_endian = false;
    std::uint16_t entry_count = 0u;
    std::size_t entries_offset = 0u;
    if (!locate_tiff_ifd(tiff, tiff_size, ifd_index, &little_endian, &entry_count, &entries_offset)) {
        return PILLOW_C_OK;
    }

    TiffExifCollector collector;
    collect_tiff_exif_entries(tiff, tiff_size, little_endian, tiff + entries_offset, entry_count, &collector);
    for (std::size_t sub_index = 0u; sub_index < collector.sub_ifd_offsets.size(); ++sub_index) {
        const std::uint32_t sub_offset = collector.sub_ifd_offsets[sub_index];
        if (static_cast<std::size_t>(sub_offset) + 2u > tiff_size) {
            continue;
        }
        const std::uint16_t sub_count = read_tiff16(tiff + sub_offset, little_endian);
        const std::size_t sub_entries_size = static_cast<std::size_t>(sub_count) * 12u;
        if (sub_count == 0u || sub_entries_size > tiff_size - static_cast<std::size_t>(sub_offset) - 2u) {
            continue;
        }
        collect_tiff_exif_entries(
            tiff,
            tiff_size,
            little_endian,
            tiff + static_cast<std::size_t>(sub_offset) + 2u,
            sub_count,
            &collector);
    }
    const int serialized_orientation = orientation == 1 ? 1 : 0;
    const bool has_resolution_exif =
        collector.has_x_resolution && collector.has_y_resolution && collector.has_resolution_unit && collector.resolution_unit == 2u;
    if (collector.ascii_tags.empty() && collector.uint_tags.empty() && collector.rational_tags.empty() && collector.rational_array_tags.empty() && collector.short_array_tags.empty() && collector.uint_array_tags.empty() && collector.double_array_tags.empty() && collector.float_array_tags.empty() &&
        collector.byte_array_tags.empty() && collector.signed_rational_tags.empty() && collector.signed_rational_array_tags.empty() && collector.undefined_tags.empty() && serialized_orientation == 0 &&
        !has_resolution_exif) {
        return PILLOW_C_OK;
    }

    std::vector<const char*> ascii_value_ptrs;
    ascii_value_ptrs.reserve(collector.ascii_values.size());
    for (const std::string& value : collector.ascii_values) {
        ascii_value_ptrs.push_back(value.c_str());
    }

    if (has_resolution_exif) {
        collector.uint_tags.push_back(296);
        collector.uint_values.push_back(collector.resolution_unit);
        collector.uint_types.push_back(collector.resolution_unit_type);
        collector.rational_tags.push_back(282);
        collector.rational_numerators.push_back(collector.x_resolution_numerator);
        collector.rational_denominators.push_back(collector.x_resolution_denominator);
        collector.rational_tags.push_back(283);
        collector.rational_numerators.push_back(collector.y_resolution_numerator);
        collector.rational_denominators.push_back(collector.y_resolution_denominator);
    }
    const std::size_t uint_count = collector.uint_tags.size();
    const std::size_t rational_count = collector.rational_tags.size();
    const std::size_t rational_array_count = collector.rational_array_tags.size();
    const std::size_t short_array_count = collector.short_array_tags.size();
    const std::size_t uint_array_count = collector.uint_array_tags.size();
    const std::size_t double_array_count = collector.double_array_tags.size();
    const std::size_t float_array_count = collector.float_array_tags.size();
    const std::size_t byte_array_count = collector.byte_array_tags.size();
    const std::size_t signed_rational_count = collector.signed_rational_tags.size();
    const std::size_t signed_rational_array_count = collector.signed_rational_array_tags.size();
    const std::size_t undefined_count = collector.undefined_tags.size();

    std::size_t required = 0u;
    int status = copy_exif_entries_internal_uint_array_bytes(
        serialized_orientation,
        collector.ascii_tags.empty() ? nullptr : collector.ascii_tags.data(),
        ascii_value_ptrs.empty() ? nullptr : ascii_value_ptrs.data(),
        collector.ascii_tags.size(),
        uint_count == 0u ? nullptr : collector.uint_tags.data(),
        uint_count == 0u ? nullptr : collector.uint_values.data(),
        uint_count == 0u ? nullptr : collector.uint_types.data(),
        uint_count,
        rational_count == 0u ? nullptr : collector.rational_tags.data(),
        rational_count == 0u ? nullptr : collector.rational_numerators.data(),
        rational_count == 0u ? nullptr : collector.rational_denominators.data(),
        rational_count,
        rational_array_count == 0u ? nullptr : collector.rational_array_tags.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_numerators.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_denominators.data(),
        collector.rational_array_numerators.size(),
        rational_array_count == 0u ? nullptr : collector.rational_array_offsets.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_counts.data(),
        rational_array_count,
        short_array_count == 0u ? nullptr : collector.short_array_tags.data(),
        short_array_count == 0u ? nullptr : collector.short_array_values.data(),
        collector.short_array_values.size(),
        short_array_count == 0u ? nullptr : collector.short_array_offsets.data(),
        short_array_count == 0u ? nullptr : collector.short_array_counts.data(),
        short_array_count,
        uint_array_count == 0u ? nullptr : collector.uint_array_tags.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_values.data(),
        collector.uint_array_values.size(),
        uint_array_count == 0u ? nullptr : collector.uint_array_offsets.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_counts.data(),
        uint_array_count,
        double_array_count == 0u ? nullptr : collector.double_array_tags.data(),
        double_array_count == 0u ? nullptr : collector.double_array_values.data(),
        collector.double_array_values.size(),
        double_array_count == 0u ? nullptr : collector.double_array_offsets.data(),
        double_array_count == 0u ? nullptr : collector.double_array_counts.data(),
        double_array_count,
        float_array_count == 0u ? nullptr : collector.float_array_tags.data(),
        float_array_count == 0u ? nullptr : collector.float_array_values.data(),
        collector.float_array_values.size(),
        float_array_count == 0u ? nullptr : collector.float_array_offsets.data(),
        float_array_count == 0u ? nullptr : collector.float_array_counts.data(),
        float_array_count,
        byte_array_count == 0u ? nullptr : collector.byte_array_tags.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_values.data(),
        collector.byte_array_values.size(),
        byte_array_count == 0u ? nullptr : collector.byte_array_offsets.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_counts.data(),
        byte_array_count,
        signed_rational_count == 0u ? nullptr : collector.signed_rational_tags.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_numerators.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_denominators.data(),
        signed_rational_count,
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_tags.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_numerators.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_denominators.data(),
        collector.signed_rational_array_numerators.size(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_offsets.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_counts.data(),
        signed_rational_array_count,
        undefined_count == 0u ? nullptr : collector.undefined_tags.data(),
        undefined_count == 0u ? nullptr : collector.undefined_values.data(),
        collector.undefined_values.size(),
        undefined_count == 0u ? nullptr : collector.undefined_offsets.data(),
        undefined_count == 0u ? nullptr : collector.undefined_counts.data(),
        undefined_count,
        nullptr,
        0u,
        &required);
    if (status != PILLOW_C_OK && status != PILLOW_C_NULL_POINTER) {
        return status;
    }
    if (required == 0u) {
        return PILLOW_C_OK;
    }

    out_exif->assign(required, std::uint8_t{0});
    status = copy_exif_entries_internal_uint_array_bytes(
        serialized_orientation,
        collector.ascii_tags.empty() ? nullptr : collector.ascii_tags.data(),
        ascii_value_ptrs.empty() ? nullptr : ascii_value_ptrs.data(),
        collector.ascii_tags.size(),
        uint_count == 0u ? nullptr : collector.uint_tags.data(),
        uint_count == 0u ? nullptr : collector.uint_values.data(),
        uint_count == 0u ? nullptr : collector.uint_types.data(),
        uint_count,
        rational_count == 0u ? nullptr : collector.rational_tags.data(),
        rational_count == 0u ? nullptr : collector.rational_numerators.data(),
        rational_count == 0u ? nullptr : collector.rational_denominators.data(),
        rational_count,
        rational_array_count == 0u ? nullptr : collector.rational_array_tags.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_numerators.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_denominators.data(),
        collector.rational_array_numerators.size(),
        rational_array_count == 0u ? nullptr : collector.rational_array_offsets.data(),
        rational_array_count == 0u ? nullptr : collector.rational_array_counts.data(),
        rational_array_count,
        short_array_count == 0u ? nullptr : collector.short_array_tags.data(),
        short_array_count == 0u ? nullptr : collector.short_array_values.data(),
        collector.short_array_values.size(),
        short_array_count == 0u ? nullptr : collector.short_array_offsets.data(),
        short_array_count == 0u ? nullptr : collector.short_array_counts.data(),
        short_array_count,
        uint_array_count == 0u ? nullptr : collector.uint_array_tags.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_values.data(),
        collector.uint_array_values.size(),
        uint_array_count == 0u ? nullptr : collector.uint_array_offsets.data(),
        uint_array_count == 0u ? nullptr : collector.uint_array_counts.data(),
        uint_array_count,
        double_array_count == 0u ? nullptr : collector.double_array_tags.data(),
        double_array_count == 0u ? nullptr : collector.double_array_values.data(),
        collector.double_array_values.size(),
        double_array_count == 0u ? nullptr : collector.double_array_offsets.data(),
        double_array_count == 0u ? nullptr : collector.double_array_counts.data(),
        double_array_count,
        float_array_count == 0u ? nullptr : collector.float_array_tags.data(),
        float_array_count == 0u ? nullptr : collector.float_array_values.data(),
        collector.float_array_values.size(),
        float_array_count == 0u ? nullptr : collector.float_array_offsets.data(),
        float_array_count == 0u ? nullptr : collector.float_array_counts.data(),
        float_array_count,
        byte_array_count == 0u ? nullptr : collector.byte_array_tags.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_values.data(),
        collector.byte_array_values.size(),
        byte_array_count == 0u ? nullptr : collector.byte_array_offsets.data(),
        byte_array_count == 0u ? nullptr : collector.byte_array_counts.data(),
        byte_array_count,
        signed_rational_count == 0u ? nullptr : collector.signed_rational_tags.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_numerators.data(),
        signed_rational_count == 0u ? nullptr : collector.signed_rational_denominators.data(),
        signed_rational_count,
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_tags.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_numerators.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_denominators.data(),
        collector.signed_rational_array_numerators.size(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_offsets.data(),
        signed_rational_array_count == 0u ? nullptr : collector.signed_rational_array_counts.data(),
        signed_rational_array_count,
        undefined_count == 0u ? nullptr : collector.undefined_tags.data(),
        undefined_count == 0u ? nullptr : collector.undefined_values.data(),
        collector.undefined_values.size(),
        undefined_count == 0u ? nullptr : collector.undefined_offsets.data(),
        undefined_count == 0u ? nullptr : collector.undefined_counts.data(),
        undefined_count,
        out_exif->data(),
        out_exif->size(),
        &required);
    if (status != PILLOW_C_OK) {
        out_exif->clear();
        return status;
    }
    return PILLOW_C_OK;
}

int build_tiff_common_ascii_exif(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int orientation,
    std::vector<std::uint8_t>* out_exif)
{
    return build_tiff_common_ascii_exif_for_ifd(tiff, tiff_size, 0, orientation, out_exif);
}

int try_open_tiff_chunky_frame(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    int frame_index,
    bool* recognized,
    bool* tiled_storage,
    PillowCImage** out_image)
{
    if (!recognized || !tiled_storage || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *recognized = false;
    *tiled_storage = false;
    *out_image = nullptr;

    bool is_tiff_chunky_shape = false;
    bool is_tiff_tiled_storage = false;
    PillowCImage* image = nullptr;
    const int parse_status = parse_tiff_chunky_image_for_ifd(
        tiff,
        tiff_size,
        frame_index,
        &is_tiff_chunky_shape,
        &is_tiff_tiled_storage,
        &image);
    if (parse_status != PILLOW_C_OK) {
        return parse_status;
    }
    if (!is_tiff_chunky_shape) {
        return PILLOW_C_OK;
    }

    const int orientation = frame_index == 0 ? parse_tiff_orientation(tiff, tiff_size) : 0;
    if (!is_tiff_tiled_storage && orientation != 0 && orientation != 1) {
        delete image;
        return PILLOW_C_OK;
    }

    int status = build_tiff_common_ascii_exif_for_ifd(
        tiff,
        tiff_size,
        frame_index,
        orientation,
        &image->tiff_exif);
    if (status != PILLOW_C_OK) {
        delete image;
        return status;
    }
    std::vector<std::uint8_t> tiff_icc_profile;
    if (parse_tiff_icc_profile_for_ifd(tiff, tiff_size, frame_index, &tiff_icc_profile)) {
        image->tiff_icc_profile = std::move(tiff_icc_profile);
    }
    std::vector<std::uint8_t> tiff_xmp;
    if (parse_tiff_xmp_for_ifd(tiff, tiff_size, frame_index, &tiff_xmp)) {
        image->xmp = std::move(tiff_xmp);
    }
    if (orientation == 1) {
        image->exif_orientation = 1;
    } else if (is_tiff_tiled_storage && orientation != 0) {
        status = apply_tiff_orientation_transform(image, orientation);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
    }
    TiffResolutionMetadata resolution;
    if (parse_tiff_resolution_for_ifd(tiff, tiff_size, frame_index, &resolution)) {
        image->has_dpi = true;
        image->dpi_x = resolution.dpi_x;
        image->dpi_y = resolution.dpi_y;
    }

    *recognized = true;
    *tiled_storage = is_tiff_tiled_storage;
    *out_image = image;
    return PILLOW_C_OK;
}


int open_tiff_frame_image(const char* path, int frame_index, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<wchar_t> wide_path;
        if (!utf8_path_to_wide(path, &wide_path)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<std::uint8_t> tiff_bytes;
        if (!read_binary_file(path, &tiff_bytes)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const bool has_tiff_bytes = true;
        int status = PILLOW_C_OK;
        bool is_tiff_bigtiff = false;
        bool is_tiff_bigtiff_shape = false;
        PillowCImage* bigtiff_image = nullptr;
        status = parse_tiff_bigtiff_tiled_image_for_ifd(
            tiff_bytes.data(),
            tiff_bytes.size(),
            frame_index,
            &is_tiff_bigtiff,
            &is_tiff_bigtiff_shape,
            &bigtiff_image);
        if (status != PILLOW_C_OK && !is_tiff_bigtiff) {
            return status;
        }
        if (is_tiff_bigtiff) {
            if (status != PILLOW_C_OK || !is_tiff_bigtiff_shape) {
                // The tiled parser rejected the shape (e.g. Pillow 11.3.0
                // big_tiff strip saves); fall back to the bounded strip
                // layout. The tiled parser already freed its image on
                // failure, so drop the stale pointer.
                bigtiff_image = nullptr;
                bool strip_is_bigtiff = false;
                bool strip_recognized = false;
                PillowCImage* strip_image = nullptr;
                status = parse_tiff_bigtiff_strip_image_for_ifd(
                    tiff_bytes.data(),
                    tiff_bytes.size(),
                    frame_index,
                    &strip_is_bigtiff,
                    &strip_recognized,
                    &strip_image);
                if (status != PILLOW_C_OK) {
                    delete strip_image;
                    return status;
                }
                if (!strip_is_bigtiff || !strip_recognized) {
                    delete strip_image;
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                bigtiff_image = strip_image;
            }
            const int metadata_status = attach_tiff_bigtiff_metadata_for_ifd(
                tiff_bytes.data(),
                tiff_bytes.size(),
                frame_index,
                bigtiff_image);
            if (metadata_status != PILLOW_C_OK) {
                delete bigtiff_image;
                return metadata_status;
            }
            *out_image = bigtiff_image;
            return PILLOW_C_OK;
        }
        bool is_tiff_chunky_shape = false;
        bool is_tiff_tiled_storage = false;
        PillowCImage* tile_shape_image = nullptr;
        status = try_open_tiff_chunky_frame(
            tiff_bytes.data(),
            tiff_bytes.size(),
            frame_index,
            &is_tiff_chunky_shape,
            &is_tiff_tiled_storage,
            &tile_shape_image);
        if (status != PILLOW_C_OK) {
            return status;
        }
        if (is_tiff_chunky_shape) {
            *out_image = tile_shape_image;
            return PILLOW_C_OK;
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

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            wide_path.data(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatTiff)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || static_cast<UINT>(frame_index) >= frame_count) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(static_cast<UINT>(frame_index), frame.put());
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
        bool is_tiff_la = false;
        if (frame_index == 0) {
            bool is_tiff_numeric = false;
            PillowCImage* numeric_image = nullptr;
            status = parse_tiff_numeric_image(tiff_bytes.data(), tiff_bytes.size(), &is_tiff_numeric, &numeric_image);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (is_tiff_numeric) {
                const int orientation = parse_tiff_orientation(tiff_bytes.data(), tiff_bytes.size());
                status = build_tiff_common_ascii_exif(tiff_bytes.data(), tiff_bytes.size(), orientation, &numeric_image->tiff_exif);
                if (status != PILLOW_C_OK) {
                    delete numeric_image;
                    return status;
                }
                std::vector<std::uint8_t> tiff_icc_profile;
                if (parse_tiff_icc_profile(tiff_bytes.data(), tiff_bytes.size(), &tiff_icc_profile)) {
                    numeric_image->tiff_icc_profile = std::move(tiff_icc_profile);
                }
                std::vector<std::uint8_t> tiff_xmp;
                if (parse_tiff_xmp_for_ifd(tiff_bytes.data(), tiff_bytes.size(), 0, &tiff_xmp)) {
                    numeric_image->xmp = std::move(tiff_xmp);
                }
                if (orientation == 1) {
                    numeric_image->exif_orientation = 1;
                }
                TiffResolutionMetadata resolution;
                if (parse_tiff_resolution(tiff_bytes.data(), tiff_bytes.size(), &resolution)) {
                    numeric_image->has_dpi = true;
                    numeric_image->dpi_x = resolution.dpi_x;
                    numeric_image->dpi_y = resolution.dpi_y;
                }
                *out_image = numeric_image;
                return PILLOW_C_OK;
            }
            bool is_tiff_i16 = false;
            PillowCImage* i16_image = nullptr;
            status = parse_tiff_i16_image(tiff_bytes.data(), tiff_bytes.size(), &is_tiff_i16, &i16_image);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (is_tiff_i16) {
                const int orientation = parse_tiff_orientation(tiff_bytes.data(), tiff_bytes.size());
                status = build_tiff_common_ascii_exif(tiff_bytes.data(), tiff_bytes.size(), orientation, &i16_image->tiff_exif);
                if (status != PILLOW_C_OK) {
                    delete i16_image;
                    return status;
                }
                std::vector<std::uint8_t> tiff_icc_profile;
                if (parse_tiff_icc_profile(tiff_bytes.data(), tiff_bytes.size(), &tiff_icc_profile)) {
                    i16_image->tiff_icc_profile = std::move(tiff_icc_profile);
                }
                std::vector<std::uint8_t> tiff_xmp;
                if (parse_tiff_xmp_for_ifd(tiff_bytes.data(), tiff_bytes.size(), 0, &tiff_xmp)) {
                    i16_image->xmp = std::move(tiff_xmp);
                }
                if (orientation == 1) {
                    i16_image->exif_orientation = 1;
                }
                TiffResolutionMetadata resolution;
                if (parse_tiff_resolution(tiff_bytes.data(), tiff_bytes.size(), &resolution)) {
                    i16_image->has_dpi = true;
                    i16_image->dpi_x = resolution.dpi_x;
                    i16_image->dpi_y = resolution.dpi_y;
                }
                *out_image = i16_image;
                return PILLOW_C_OK;
            }
            is_tiff_la = parse_tiff_is_la_mode(tiff_bytes.data(), tiff_bytes.size());
        }
        if (frame_index != 0) {
            bool is_tiff_numeric_frame = false;
            PillowCImage* numeric_frame_image = nullptr;
            status = parse_tiff_numeric_image_for_ifd(
                tiff_bytes.data(),
                tiff_bytes.size(),
                frame_index,
                &is_tiff_numeric_frame,
                &numeric_frame_image);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (is_tiff_numeric_frame) {
                status = build_tiff_common_ascii_exif_for_ifd(
                    tiff_bytes.data(),
                    tiff_bytes.size(),
                    frame_index,
                    0,
                    &numeric_frame_image->tiff_exif);
                if (status != PILLOW_C_OK) {
                    delete numeric_frame_image;
                    return status;
                }
                std::vector<std::uint8_t> tiff_icc_profile;
                if (parse_tiff_icc_profile_for_ifd(
                        tiff_bytes.data(),
                        tiff_bytes.size(),
                        frame_index,
                        &tiff_icc_profile)) {
                    numeric_frame_image->tiff_icc_profile = std::move(tiff_icc_profile);
                }
                std::vector<std::uint8_t> tiff_xmp;
                if (parse_tiff_xmp_for_ifd(tiff_bytes.data(), tiff_bytes.size(), frame_index, &tiff_xmp)) {
                    numeric_frame_image->xmp = std::move(tiff_xmp);
                }
                TiffResolutionMetadata resolution;
                if (parse_tiff_resolution_for_ifd(
                        tiff_bytes.data(),
                        tiff_bytes.size(),
                        frame_index,
                        &resolution)) {
                    numeric_frame_image->has_dpi = true;
                    numeric_frame_image->dpi_x = resolution.dpi_x;
                    numeric_frame_image->dpi_y = resolution.dpi_y;
                }
                *out_image = numeric_frame_image;
                return PILLOW_C_OK;
            }
            bool is_tiff_i16_frame = false;
            PillowCImage* i16_frame_image = nullptr;
            status = parse_tiff_i16_image_for_ifd(
                tiff_bytes.data(),
                tiff_bytes.size(),
                frame_index,
                &is_tiff_i16_frame,
                &i16_frame_image);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (is_tiff_i16_frame) {
                status = build_tiff_common_ascii_exif_for_ifd(
                    tiff_bytes.data(),
                    tiff_bytes.size(),
                    frame_index,
                    0,
                    &i16_frame_image->tiff_exif);
                if (status != PILLOW_C_OK) {
                    delete i16_frame_image;
                    return status;
                }
                std::vector<std::uint8_t> tiff_icc_profile;
                if (parse_tiff_icc_profile_for_ifd(
                        tiff_bytes.data(),
                        tiff_bytes.size(),
                        frame_index,
                        &tiff_icc_profile)) {
                    i16_frame_image->tiff_icc_profile = std::move(tiff_icc_profile);
                }
                std::vector<std::uint8_t> tiff_xmp;
                if (parse_tiff_xmp_for_ifd(tiff_bytes.data(), tiff_bytes.size(), frame_index, &tiff_xmp)) {
                    i16_frame_image->xmp = std::move(tiff_xmp);
                }
                TiffResolutionMetadata resolution;
                if (parse_tiff_resolution_for_ifd(
                        tiff_bytes.data(),
                        tiff_bytes.size(),
                        frame_index,
                        &resolution)) {
                    i16_frame_image->has_dpi = true;
                    i16_frame_image->dpi_x = resolution.dpi_x;
                    i16_frame_image->dpi_y = resolution.dpi_y;
                }
                *out_image = i16_frame_image;
                return PILLOW_C_OK;
            }
        }
        int mode = 0;
        int channels = 0;
        int decoded_channels = 0;
        WICPixelFormatGUID target_format = {};
        std::vector<std::uint8_t> palette_rgb;
        if (IsEqualGUID(source_format, GUID_WICPixelFormat8bppIndexed)) {
            mode = PILLOW_C_MODE_P;
            channels = 1;
            decoded_channels = 1;
            target_format = GUID_WICPixelFormat8bppIndexed;
            status = copy_wic_palette_rgb(frame.get(), factory.get(), &palette_rgb);
            if (status != PILLOW_C_OK) {
                return status;
            }
        } else if (is_tiff_la) {
            mode = PILLOW_C_MODE_LA;
            channels = 2;
            decoded_channels = 4;
            target_format = GUID_WICPixelFormat32bppRGBA;
        } else {
            status = wic_format_to_mode(source_format, &mode, &channels, &target_format);
            if (status != PILLOW_C_OK) {
                return status;
            }
            decoded_channels = channels;
        }
        if (!((mode == PILLOW_C_MODE_1 && channels == 1) ||
              (mode == PILLOW_C_MODE_L && channels == 1) ||
              (mode == PILLOW_C_MODE_LA && channels == 2) ||
              (mode == PILLOW_C_MODE_P && channels == 1) ||
              (mode == PILLOW_C_MODE_CMYK && channels == 4) ||
              (mode == PILLOW_C_MODE_RGB && channels == 3) ||
              (mode == PILLOW_C_MODE_RGBA && channels == 4))) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size) ||
            stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t decoded_stride = 0;
        std::size_t decoded_size = 0;
        if (!checked_image_size(width, height, decoded_channels, &decoded_stride, &decoded_size) ||
            decoded_stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            decoded_size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
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
                nullptr,
                0.0,
                WICBitmapPaletteTypeMedianCut);
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
        }
        if (mode == PILLOW_C_MODE_P) {
            image->palette_rgb = std::move(palette_rgb);
        }

        if (has_tiff_bytes) {
            if (frame_index == 0 && mode == PILLOW_C_MODE_P) {
                std::vector<std::uint8_t> parsed_palette;
                if (!parse_tiff_palette_rgb(tiff_bytes.data(), tiff_bytes.size(), &parsed_palette)) {
                    delete image;
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                image->palette_rgb = std::move(parsed_palette);
            }
            const int orientation = frame_index == 0 ? parse_tiff_orientation(tiff_bytes.data(), tiff_bytes.size()) : 0;
            status = build_tiff_common_ascii_exif_for_ifd(
                tiff_bytes.data(),
                tiff_bytes.size(),
                frame_index,
                orientation,
                &image->tiff_exif);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
            std::vector<std::uint8_t> tiff_icc_profile;
            if (parse_tiff_icc_profile_for_ifd(tiff_bytes.data(), tiff_bytes.size(), frame_index, &tiff_icc_profile)) {
                image->tiff_icc_profile = std::move(tiff_icc_profile);
            }
            std::vector<std::uint8_t> tiff_xmp;
            if (parse_tiff_xmp_for_ifd(tiff_bytes.data(), tiff_bytes.size(), frame_index, &tiff_xmp)) {
                image->xmp = std::move(tiff_xmp);
            }
            if (frame_index == 0) {
                if (orientation == 1) {
                    image->exif_orientation = 1;
                } else {
                    status = apply_tiff_orientation_transform(image, orientation);
                    if (status != PILLOW_C_OK) {
                        delete image;
                        return status;
                    }
                }
            }
            TiffResolutionMetadata resolution;
            if (parse_tiff_resolution_for_ifd(tiff_bytes.data(), tiff_bytes.size(), frame_index, &resolution)) {
                image->has_dpi = true;
                image->dpi_x = resolution.dpi_x;
                image->dpi_y = resolution.dpi_y;
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_tiff_image(const char* path, PillowCImage** out_image)
{
    return open_tiff_frame_image(path, 0, out_image);
}

int validate_tiff_save_image(const PillowCImage* image)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool is_palette = image->mode == PILLOW_C_MODE_P && image->channels == 1;
    const bool is_numeric = (image->mode == PILLOW_C_MODE_I || image->mode == PILLOW_C_MODE_F) &&
        image->channels == 4;
    if (!((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_1 && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_I16 && image->channels == 2) ||
          (image->mode == PILLOW_C_MODE_I16B && image->channels == 2) ||
          (image->mode == PILLOW_C_MODE_LA && image->channels == 2) ||
          is_palette ||
          (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) ||
          is_numeric ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
          (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (is_palette &&
        (image->palette_rgb.empty() || image->palette_rgb.size() % 3u != 0u ||
         image->palette_rgb.size() > 256u * 3u)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

WICPixelFormatGUID tiff_save_pixel_format(const PillowCImage* image)
{
    if (image->mode == PILLOW_C_MODE_RGB) {
        return GUID_WICPixelFormat24bppBGR;
    }
    if (image->mode == PILLOW_C_MODE_RGBA) {
        return GUID_WICPixelFormat32bppBGRA;
    }
    return GUID_WICPixelFormat8bppGray;
}

int write_tiff_frame(IWICBitmapEncoder* encoder, const PillowCImage* image)
{
    if (!encoder) {
        return PILLOW_C_NULL_POINTER;
    }
    const int status = validate_tiff_save_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    HRESULT hr = encoder->CreateNewFrame(frame.put(), nullptr);
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

    const WICPixelFormatGUID format = tiff_save_pixel_format(image);
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
    return PILLOW_C_OK;
}

void append_tiff_entry(
    std::vector<std::uint8_t>& out,
    std::uint16_t tag,
    std::uint16_t type,
    std::uint32_t count,
    std::uint32_t value)
{
    append_le16(out, tag);
    append_le16(out, type);
    append_le32(out, count);
    append_le32(out, value);
}

void append_tiff_entry_be(
    std::vector<std::uint8_t>& out,
    std::uint16_t tag,
    std::uint16_t type,
    std::uint32_t count,
    std::uint32_t value)
{
    append_be16(out, tag);
    append_be16(out, type);
    append_be32(out, count);
    if (type == 3u && count == 1u) {
        append_be16(out, static_cast<std::uint16_t>(value));
        append_be16(out, 0u);
        return;
    }
    append_be32(out, value);
}

std::size_t align_tiff_offset(std::size_t value)
{
    return (value + 1u) & ~std::size_t{1u};
}

bool tiff_u32_offset(std::size_t value, std::uint32_t* out_value)
{
    if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    *out_value = static_cast<std::uint32_t>(value);
    return true;
}

int tiff_dpi_to_rational(bool has_dpi, double dpi_x, double dpi_y, std::uint32_t* out_x, std::uint32_t* out_y)
{
    if (!out_x || !out_y) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_x = 0u;
    *out_y = 0u;
    if (!has_dpi) {
        return PILLOW_C_OK;
    }

    std::int64_t rounded_x = 0;
    std::int64_t rounded_y = 0;
    if (!pillow_c_round_to_i64(dpi_x, &rounded_x) || !pillow_c_round_to_i64(dpi_y, &rounded_y) ||
        rounded_x <= 0 || rounded_y <= 0 ||
        rounded_x > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()) ||
        rounded_y > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_x = static_cast<std::uint32_t>(rounded_x);
    *out_y = static_cast<std::uint32_t>(rounded_y);
    return PILLOW_C_OK;
}

struct TiffFrameLayout {
    std::uint32_t ifd_offset = 0;
    std::uint32_t bits_offset = 0;
    std::uint32_t x_resolution_offset = 0;
    std::uint32_t y_resolution_offset = 0;
    std::uint32_t xmp_offset = 0;
    std::uint32_t icc_profile_offset = 0;
    std::vector<std::uint32_t> ascii_offsets;
    std::uint32_t color_map_offset = 0;
    std::uint32_t pixel_offset = 0;
    std::uint32_t next_ifd_offset = 0;
    std::uint32_t pixel_byte_count = 0;
    std::uint16_t entry_count = 0;
};

int normalize_tiff_save_compression(int compression, std::uint16_t* out_compression)
{
    if (!out_compression) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compression == 0 || compression == TIFF_COMPRESSION_NONE) {
        *out_compression = TIFF_COMPRESSION_NONE;
        return PILLOW_C_OK;
    }
    if (compression == TIFF_COMPRESSION_PACKBITS) {
        *out_compression = TIFF_COMPRESSION_PACKBITS;
        return PILLOW_C_OK;
    }
    if (compression == TIFF_COMPRESSION_LZW) {
        *out_compression = TIFF_COMPRESSION_LZW;
        return PILLOW_C_OK;
    }
    if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
        *out_compression = TIFF_COMPRESSION_ADOBE_DEFLATE;
        return PILLOW_C_OK;
    }
    return PILLOW_C_INVALID_ARGUMENT;
}

void append_packbits_encoded_row(const std::uint8_t* row, std::size_t length, std::vector<std::uint8_t>& out)
{
    enum class PackBitsState {
        Base,
        Literal,
        Run,
        LiteralRun,
    };

    std::size_t i = 0;
    std::size_t last_literal = 0;
    PackBitsState state = PackBitsState::Base;
    while (i < length) {
        std::size_t run = 1;
        while (i + run < length && row[i + run] == row[i]) {
            ++run;
        }
        const std::uint8_t value = row[i];
        i += run;

        for (;;) {
            switch (state) {
            case PackBitsState::Base:
                if (run > 1u) {
                    state = PackBitsState::Run;
                    const std::size_t encoded_run = std::min<std::size_t>(run, 128u);
                    out.push_back(static_cast<std::uint8_t>(257u - encoded_run));
                    out.push_back(value);
                    run -= encoded_run;
                } else {
                    last_literal = out.size();
                    out.push_back(0u);
                    out.push_back(value);
                    state = PackBitsState::Literal;
                    run = 0u;
                }
                break;

            case PackBitsState::Literal:
                if (run > 1u) {
                    state = PackBitsState::LiteralRun;
                    const std::size_t encoded_run = std::min<std::size_t>(run, 128u);
                    out.push_back(static_cast<std::uint8_t>(257u - encoded_run));
                    out.push_back(value);
                    run -= encoded_run;
                } else {
                    ++out[last_literal];
                    out.push_back(value);
                    state = out[last_literal] == 127u
                        ? PackBitsState::Base
                        : PackBitsState::Literal;
                    run = 0u;
                }
                break;

            case PackBitsState::Run:
                if (run > 1u) {
                    const std::size_t encoded_run = std::min<std::size_t>(run, 128u);
                    out.push_back(static_cast<std::uint8_t>(257u - encoded_run));
                    out.push_back(value);
                    run -= encoded_run;
                } else {
                    last_literal = out.size();
                    out.push_back(0u);
                    out.push_back(value);
                    state = PackBitsState::Literal;
                    run = 0u;
                }
                break;

            case PackBitsState::LiteralRun:
                if (run == 1u && out[out.size() - 2u] == 255u && out[last_literal] < 126u) {
                    out[last_literal] = static_cast<std::uint8_t>(out[last_literal] + 2u);
                    out[out.size() - 2u] = out.back();
                    state = out[last_literal] == 127u
                        ? PackBitsState::Base
                        : PackBitsState::Literal;
                } else {
                    state = PackBitsState::Run;
                }
                break;
            }
            if (run == 0u) {
                break;
            }
        }
    }
}

std::size_t tiff_uncompressed_row_stride(const PillowCImage* image)
{
    if (image->mode == PILLOW_C_MODE_1) {
        return (static_cast<std::size_t>(image->width) + 7u) / 8u;
    }
    return static_cast<std::size_t>(image->stride);
}

std::vector<std::uint8_t> tiff_pack_mode_one_pixels(const PillowCImage* image)
{
    const std::size_t row_stride = tiff_uncompressed_row_stride(image);
    std::vector<std::uint8_t> packed(row_stride * static_cast<std::size_t>(image->height), 0);
    for (int y = 0; y < image->height; ++y) {
        const std::size_t src_row = static_cast<std::size_t>(y) * static_cast<std::size_t>(image->stride);
        const std::size_t dst_row = static_cast<std::size_t>(y) * row_stride;
        for (int x = 0; x < image->width; ++x) {
            if (image->pixels[src_row + static_cast<std::size_t>(x)] != 0u) {
                packed[dst_row + static_cast<std::size_t>(x / 8)] |=
                    static_cast<std::uint8_t>(0x80u >> (x % 8));
            }
        }
    }
    return packed;
}

std::vector<std::uint8_t> tiff_i16b_to_i16_pixels(const PillowCImage* image)
{
    std::vector<std::uint8_t> swapped(image->pixels.size(), 0u);
    for (std::size_t index = 0u; index + 1u < image->pixels.size(); index += 2u) {
        swapped[index] = image->pixels[index + 1u];
        swapped[index + 1u] = image->pixels[index];
    }
    return swapped;
}

std::vector<std::uint8_t> tiff_packbits_encode_pixels(
    const std::uint8_t* pixels,
    std::size_t row_stride,
    int height)
{
    std::vector<std::uint8_t> encoded;
    encoded.reserve(row_stride * static_cast<std::size_t>(height) + static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        const std::size_t row_offset = static_cast<std::size_t>(y) * row_stride;
        append_packbits_encoded_row(
            pixels + row_offset,
            row_stride,
            encoded);
    }
    return encoded;
}

struct TiffMsbBitWriter {
    std::vector<std::uint8_t> bytes;
    std::uint32_t bits = 0;
    int bit_count = 0;

    void write(int code, int size)
    {
        bits = (bits << size) | static_cast<std::uint32_t>(code);
        bit_count += size;
        while (bit_count >= 8) {
            const int shift = bit_count - 8;
            bytes.push_back(static_cast<std::uint8_t>((bits >> shift) & 0xffu));
            bit_count -= 8;
            bits = bit_count == 0 ? 0u : (bits & ((std::uint32_t{1} << bit_count) - 1u));
        }
    }

    void flush()
    {
        if (bit_count > 0) {
            bytes.push_back(static_cast<std::uint8_t>((bits << (8 - bit_count)) & 0xffu));
            bits = 0;
            bit_count = 0;
        }
    }
};

bool tiff_lzw_encode_pixels(
    const std::uint8_t* pixels,
    std::size_t row_stride,
    int height,
    std::vector<std::uint8_t>* out)
{
    if (!pixels || !out || row_stride == 0u || height <= 0 ||
        row_stride > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(height)) {
        return false;
    }
    const std::size_t pixel_byte_count = row_stride * static_cast<std::size_t>(height);
    if (pixel_byte_count == 0u) {
        return false;
    }

    constexpr int clear_code = 256;
    constexpr int end_code = 257;
    int next_code = 258;
    int code_size = 9;

    TiffMsbBitWriter writer;
    writer.bytes.reserve(pixel_byte_count + pixel_byte_count / 8u + 8u);
    std::unordered_map<std::uint32_t, int> dictionary;
    dictionary.reserve(4096);

    writer.write(clear_code, code_size);
    int prefix = pixels[0];
    for (std::size_t index = 1u; index < pixel_byte_count; ++index) {
        const int value = pixels[index];
        const std::uint32_t key = (static_cast<std::uint32_t>(prefix) << 8) |
                                  static_cast<std::uint32_t>(value);
        const auto found = dictionary.find(key);
        if (found != dictionary.end()) {
            prefix = found->second;
            continue;
        }

        writer.write(prefix, code_size);
        if (next_code <= 4095) {
            dictionary.emplace(key, next_code++);
            if (next_code == 4094) {
                writer.write(clear_code, code_size);
                dictionary.clear();
                next_code = 258;
                code_size = 9;
            } else if (next_code == (1 << code_size) && code_size < 12) {
                ++code_size;
            }
        } else {
            writer.write(clear_code, code_size);
            dictionary.clear();
            next_code = 258;
            code_size = 9;
        }
        prefix = value;
    }
    writer.write(prefix, code_size);
    writer.write(end_code, code_size);
    writer.flush();
    *out = std::move(writer.bytes);
    return true;
}

bool tiff_deflate_encode_pixels(
    const std::uint8_t* pixels,
    std::size_t row_stride,
    int height,
    std::vector<std::uint8_t>* out)
{
    if (!pixels || !out || row_stride == 0u || height <= 0 ||
        row_stride > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(height)) {
        return false;
    }
    const std::size_t pixel_byte_count = row_stride * static_cast<std::size_t>(height);
    if (pixel_byte_count == 0u) {
        return false;
    }

    std::vector<std::uint8_t> raw(pixels, pixels + pixel_byte_count);
    out->clear();
    out->reserve(pixel_byte_count + pixel_byte_count / 65535u * 5u + 11u);
    return pillow_c_append_zlib_stored(*out, raw, 0x9Cu) == PILLOW_C_OK;
}

void append_tiff_palette_color_map(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& palette_rgb)
{
    for (std::size_t channel = 0; channel < 3u; ++channel) {
        for (std::size_t index = 0; index < 256u; ++index) {
            const std::size_t offset = index * 3u + channel;
            const std::uint16_t value = offset < palette_rgb.size()
                ? static_cast<std::uint16_t>(palette_rgb[offset]) << 8
                : 0u;
            append_le16(out, value);
        }
    }
}

int save_tiff_i16b_frames_image(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count < 2u || image_count > 3u || !has_dpi || !icc_profile ||
        icc_profile_size == 0u || !xmp || xmp_size == 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (icc_profile_size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        xmp_size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::uint32_t x_resolution_numerator = 0u;
    std::uint32_t y_resolution_numerator = 0u;
    const int dpi_status = tiff_dpi_to_rational(
        true,
        dpi_x,
        dpi_y,
        &x_resolution_numerator,
        &y_resolution_numerator);
    if (dpi_status != PILLOW_C_OK) {
        return dpi_status;
    }

    try {
        std::vector<TiffFrameLayout> layouts(image_count);
        std::size_t cursor = 8u;
        constexpr std::uint16_t entry_count = 14u;
        constexpr std::size_t ifd_bytes = 2u + static_cast<std::size_t>(entry_count) * 12u + 4u;
        for (std::size_t index = 0u; index < image_count; ++index) {
            const PillowCImage* image = images[index];
            if (!image) {
                return PILLOW_C_NULL_POINTER;
            }
            const int image_status = validate_tiff_save_image(image);
            if (image_status != PILLOW_C_OK) {
                return image_status;
            }
            if (image->mode != PILLOW_C_MODE_I16B || image->channels != 2) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (index > 0u) {
                cursor = (cursor + 15u) & ~std::size_t{15u};
                if (cursor > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 8u) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                cursor += 8u;
            }

            TiffFrameLayout& layout = layouts[index];
            if (!tiff_u32_offset(cursor, &layout.ifd_offset)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            layout.entry_count = entry_count;
            const std::size_t data_cursor = cursor + ifd_bytes;
            if (!tiff_u32_offset(data_cursor, &layout.x_resolution_offset) ||
                !tiff_u32_offset(data_cursor + 8u, &layout.y_resolution_offset) ||
                !tiff_u32_offset(data_cursor + 16u, &layout.xmp_offset) ||
                !tiff_u32_offset(data_cursor + 16u + xmp_size, &layout.icc_profile_offset) ||
                !tiff_u32_offset(
                    align_tiff_offset(data_cursor + 16u + xmp_size + icc_profile_size),
                    &layout.pixel_offset) ||
                image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) -
                    static_cast<std::size_t>(layout.pixel_offset)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            layout.pixel_byte_count = static_cast<std::uint32_t>(image->pixels.size());
            cursor = (align_tiff_offset(
                static_cast<std::size_t>(layout.pixel_offset) + image->pixels.size()) + 15u) &
                ~std::size_t{15u};
            if (cursor > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        for (std::size_t index = 0u; index + 1u < image_count; ++index) {
            layouts[index].next_ifd_offset = layouts[index + 1u].ifd_offset;
        }

        std::vector<std::uint8_t> out;
        out.reserve(cursor);
        out.push_back('M');
        out.push_back('M');
        append_be16(out, 42u);
        append_be32(out, 8u);

        for (std::size_t index = 0u; index < image_count; ++index) {
            const PillowCImage* image = images[index];
            const TiffFrameLayout& layout = layouts[index];
            if (index > 0u) {
                const std::size_t header_offset = static_cast<std::size_t>(layout.ifd_offset) - 8u;
                out.resize(header_offset, 0u);
                out.push_back('M');
                out.push_back('M');
                append_be16(out, 42u);
                append_be32(out, 8u);
            }
            out.resize(layout.ifd_offset, 0u);
            append_be16(out, layout.entry_count);
            append_tiff_entry_be(out, 256u, 4u, 1u, static_cast<std::uint32_t>(image->width));
            append_tiff_entry_be(out, 257u, 4u, 1u, static_cast<std::uint32_t>(image->height));
            append_tiff_entry_be(out, 258u, 3u, 1u, 16u);
            append_tiff_entry_be(out, 259u, 3u, 1u, TIFF_COMPRESSION_NONE);
            append_tiff_entry_be(out, 262u, 3u, 1u, 1u);
            append_tiff_entry_be(out, 273u, 4u, 1u, layout.pixel_offset);
            append_tiff_entry_be(out, 278u, 4u, 1u, static_cast<std::uint32_t>(image->height));
            append_tiff_entry_be(out, 279u, 4u, 1u, layout.pixel_byte_count);
            append_tiff_entry_be(out, 282u, 5u, 1u, layout.x_resolution_offset);
            append_tiff_entry_be(out, 283u, 5u, 1u, layout.y_resolution_offset);
            append_tiff_entry_be(out, 284u, 3u, 1u, 1u);
            append_tiff_entry_be(out, 296u, 3u, 1u, 2u);
            append_tiff_entry_be(out, 700u, 1u, static_cast<std::uint32_t>(xmp_size), layout.xmp_offset);
            append_tiff_entry_be(
                out,
                34675u,
                7u,
                static_cast<std::uint32_t>(icc_profile_size),
                layout.icc_profile_offset);
            append_be32(out, layout.next_ifd_offset);

            out.resize(layout.x_resolution_offset, 0u);
            append_be32(out, x_resolution_numerator);
            append_be32(out, 1u);
            append_be32(out, y_resolution_numerator);
            append_be32(out, 1u);
            out.resize(layout.xmp_offset, 0u);
            out.insert(out.end(), xmp, xmp + xmp_size);
            out.resize(layout.icc_profile_offset, 0u);
            out.insert(out.end(), icc_profile, icc_profile + icc_profile_size);
            out.resize(layout.pixel_offset, 0u);
            out.insert(out.end(), image->pixels.begin(), image->pixels.end());
        }
        out.resize(cursor, 0u);
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_i16b_image(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* ascii_tags,
    const std::uint8_t* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count)
{
    const int status = validate_tiff_save_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (image->mode != PILLOW_C_MODE_I16B || image->channels != 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::uint32_t x_resolution_numerator = 0u;
    std::uint32_t y_resolution_numerator = 0u;
    const int dpi_status = tiff_dpi_to_rational(
        has_dpi, dpi_x, dpi_y, &x_resolution_numerator, &y_resolution_numerator);
    if (dpi_status != PILLOW_C_OK) {
        return dpi_status;
    }

    const std::uint16_t entry_count = static_cast<std::uint16_t>(
        9u + (has_dpi ? 3u : 0u) + ascii_count +
        (xmp_size > 0u ? 1u : 0u) + (icc_profile_size > 0u ? 1u : 0u));
    constexpr std::uint32_t ifd_offset = 8u;
    if (image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::uint32_t ascii_offsets[2] = {0u, 0u};
    std::size_t data_cursor =
        static_cast<std::size_t>(ifd_offset) + 2u + static_cast<std::size_t>(entry_count) * 12u + 4u;
    for (std::size_t index = 0u; index < ascii_count; ++index) {
        if (ascii_tags[index] != 270 || ascii_sizes[index] <= 4u) {
            continue;
        }
        if (!tiff_u32_offset(data_cursor, &ascii_offsets[index])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        data_cursor = align_tiff_offset(data_cursor + ascii_sizes[index]);
    }
    std::uint32_t x_resolution_offset = 0u;
    std::uint32_t y_resolution_offset = 0u;
    if (has_dpi) {
        if (!tiff_u32_offset(data_cursor, &x_resolution_offset) ||
            !tiff_u32_offset(data_cursor + 8u, &y_resolution_offset)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        data_cursor += 16u;
    }
    for (std::size_t index = 0u; index < ascii_count; ++index) {
        if (ascii_tags[index] != 315 || ascii_sizes[index] <= 4u) {
            continue;
        }
        if (!tiff_u32_offset(data_cursor, &ascii_offsets[index])) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        data_cursor = align_tiff_offset(data_cursor + ascii_sizes[index]);
    }
    std::uint32_t xmp_offset = 0u;
    if (xmp_size > 0u) {
        if (!tiff_u32_offset(data_cursor, &xmp_offset)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        data_cursor = align_tiff_offset(data_cursor + xmp_size);
    }
    std::uint32_t icc_profile_offset = 0u;
    if (icc_profile_size > 0u) {
        if (!tiff_u32_offset(data_cursor, &icc_profile_offset)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        data_cursor = align_tiff_offset(data_cursor + icc_profile_size);
    }
    std::uint32_t strip_offset = 0u;
    if (!tiff_u32_offset(data_cursor, &strip_offset) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - strip_offset) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t output_size = data_cursor + image->pixels.size();

    try {
        std::vector<std::uint8_t> out;
        out.reserve(output_size);
        out.push_back('M');
        out.push_back('M');
        append_be16(out, 42u);
        append_be32(out, ifd_offset);
        append_be16(out, entry_count);
        append_tiff_entry_be(out, 256u, 4u, 1u, static_cast<std::uint32_t>(image->width));
        append_tiff_entry_be(out, 257u, 4u, 1u, static_cast<std::uint32_t>(image->height));
        append_tiff_entry_be(out, 258u, 3u, 1u, 16u);
        append_tiff_entry_be(out, 259u, 3u, 1u, TIFF_COMPRESSION_NONE);
        append_tiff_entry_be(out, 262u, 3u, 1u, 1u);
        const auto append_ascii_entry = [&](int wanted_tag) {
            for (std::size_t index = 0u; index < ascii_count; ++index) {
                if (ascii_tags[index] != wanted_tag) {
                    continue;
                }
                std::uint32_t value = ascii_offsets[index];
                if (ascii_sizes[index] <= 4u) {
                    value = 0u;
                    for (std::size_t byte_index = 0u; byte_index < ascii_sizes[index]; ++byte_index) {
                        value |= static_cast<std::uint32_t>(ascii_values[index][byte_index]) <<
                            static_cast<unsigned int>(24u - byte_index * 8u);
                    }
                }
                append_tiff_entry_be(
                    out,
                    static_cast<std::uint16_t>(wanted_tag),
                    2u,
                    static_cast<std::uint32_t>(ascii_sizes[index]),
                    value);
            }
        };
        append_ascii_entry(270);
        append_tiff_entry_be(out, 273u, 4u, 1u, strip_offset);
        append_tiff_entry_be(out, 278u, 4u, 1u, static_cast<std::uint32_t>(image->height));
        append_tiff_entry_be(out, 279u, 4u, 1u, static_cast<std::uint32_t>(image->pixels.size()));
        if (has_dpi) {
            append_tiff_entry_be(out, 282u, 5u, 1u, x_resolution_offset);
            append_tiff_entry_be(out, 283u, 5u, 1u, y_resolution_offset);
        }
        append_tiff_entry_be(out, 284u, 3u, 1u, 1u);
        if (has_dpi) {
            append_tiff_entry_be(out, 296u, 3u, 1u, 2u);
        }
        append_ascii_entry(315);
        if (xmp_size > 0u) {
            append_tiff_entry_be(out, 700u, 1u, static_cast<std::uint32_t>(xmp_size), xmp_offset);
        }
        if (icc_profile_size > 0u) {
            append_tiff_entry_be(
                out, 34675u, 7u, static_cast<std::uint32_t>(icc_profile_size), icc_profile_offset);
        }
        append_be32(out, 0u);

        for (std::size_t index = 0u; index < ascii_count; ++index) {
            if (ascii_tags[index] != 270 || ascii_sizes[index] <= 4u) {
                continue;
            }
            out.resize(ascii_offsets[index], 0u);
            out.insert(out.end(), ascii_values[index], ascii_values[index] + ascii_sizes[index]);
        }
        if (has_dpi) {
            out.resize(x_resolution_offset, 0u);
            append_be32(out, x_resolution_numerator);
            append_be32(out, 1u);
            out.resize(y_resolution_offset, 0u);
            append_be32(out, y_resolution_numerator);
            append_be32(out, 1u);
        }
        for (std::size_t index = 0u; index < ascii_count; ++index) {
            if (ascii_tags[index] != 315 || ascii_sizes[index] <= 4u) {
                continue;
            }
            out.resize(ascii_offsets[index], 0u);
            out.insert(out.end(), ascii_values[index], ascii_values[index] + ascii_sizes[index]);
        }
        if (xmp_size > 0u) {
            out.resize(xmp_offset, 0u);
            out.insert(out.end(), xmp, xmp + xmp_size);
        }
        if (icc_profile_size > 0u) {
            out.resize(icc_profile_offset, 0u);
            out.insert(out.end(), icc_profile, icc_profile + icc_profile_size);
        }
        out.resize(strip_offset, 0u);
        out.insert(out.end(), image->pixels.begin(), image->pixels.end());
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_frames_image_with_ascii_entries_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    std::uint16_t compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* ascii_tags,
    const std::uint8_t* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (icc_profile_size > 0u && !icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (xmp_size > 0u && !xmp) {
        return PILLOW_C_NULL_POINTER;
    }
    if (xmp_size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (ascii_count > 0u && (!ascii_tags || !ascii_values || !ascii_sizes)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (ascii_count > 2u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t ascii_order[2] = {0u, 0u};
    for (std::size_t index = 0u; index < ascii_count; ++index) {
        const int tag = ascii_tags[index];
        const std::size_t size = ascii_sizes[index];
        if (!ascii_values[index]) {
            return PILLOW_C_NULL_POINTER;
        }
        if (size == 0u || size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_LENGTH;
        }
        if ((tag != 270 && tag != 315) || ascii_values[index][size - 1u] != 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ascii_order[index] = index;
    }
    std::sort(ascii_order, ascii_order + ascii_count, [ascii_tags](std::size_t left, std::size_t right) {
        return ascii_tags[left] < ascii_tags[right];
    });
    for (std::size_t index = 1u; index < ascii_count; ++index) {
        if (ascii_tags[ascii_order[index - 1u]] == ascii_tags[ascii_order[index]]) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    std::size_t i16b_count = 0u;
    std::size_t i16_family_count = 0u;
    for (std::size_t index = 0; index < image_count; ++index) {
        const int refresh_status = pillow_c_refresh_const_buffer_view_image(images[index]);
        if (refresh_status != PILLOW_C_OK) {
            return refresh_status;
        }
        const int status = validate_tiff_save_image(images[index]);
        if (status != PILLOW_C_OK) {
            return status;
        }
        if (images[index]->mode == PILLOW_C_MODE_I16B) {
            ++i16b_count;
        }
        if (images[index]->mode == PILLOW_C_MODE_I16 || images[index]->mode == PILLOW_C_MODE_I16B) {
            ++i16_family_count;
        }
    }
    if (i16b_count > 0u) {
        if (ascii_count > 0u) {
            const bool supported_ascii =
                (ascii_count == 1u && (ascii_tags[0] == 270 || ascii_tags[0] == 315)) ||
                (ascii_count == 2u &&
                    ascii_tags[ascii_order[0]] == 270 && ascii_tags[ascii_order[1]] == 315);
            const bool has_binary_metadata = icc_profile_size > 0u || xmp_size > 0u;
            const bool supported_uncompressed_icc_ascii =
                compression == TIFF_COMPRESSION_NONE && image_count == 1u && has_dpi &&
                ascii_count > 0u && supported_ascii &&
                icc_profile_size > 0u && xmp_size == 0u;
            const bool supported_uncompressed_xmp_ascii =
                compression == TIFF_COMPRESSION_NONE && image_count == 1u && has_dpi &&
                ascii_count > 0u && supported_ascii &&
                icc_profile_size == 0u && xmp_size > 0u;
            const bool supported_uncompressed_full_ascii =
                compression == TIFF_COMPRESSION_NONE && image_count == 1u && has_dpi &&
                ascii_count == 1u && supported_ascii &&
                icc_profile_size > 0u && xmp_size > 0u;
            const bool supported_full_metadata =
                ascii_count == 2u && icc_profile_size > 0u && xmp_size > 0u;
            if (!supported_ascii ||
                (has_binary_metadata && !supported_uncompressed_icc_ascii &&
                    !supported_uncompressed_xmp_ascii &&
                    !supported_uncompressed_full_ascii && !supported_full_metadata)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        if (compression == TIFF_COMPRESSION_NONE) {
            if ((image_count == 2u || image_count == 3u) && has_dpi &&
                icc_profile_size > 0u && xmp_size > 0u && ascii_count == 0u) {
                return save_tiff_i16b_frames_image(
                    images,
                    image_count,
                    path,
                    has_dpi,
                    dpi_x,
                    dpi_y,
                    icc_profile,
                    icc_profile_size,
                    xmp,
                    xmp_size);
            }
            const bool bare_i16b =
                !has_dpi && icc_profile_size == 0u && xmp_size == 0u && ascii_count == 0u;
            const bool dpi_only_i16b =
                has_dpi && icc_profile_size == 0u && xmp_size == 0u && ascii_count == 0u;
            const bool dpi_icc_i16b =
                has_dpi && icc_profile_size > 0u && xmp_size == 0u && ascii_count == 0u;
            const bool dpi_xmp_i16b =
                has_dpi && icc_profile_size == 0u && xmp_size > 0u && ascii_count == 0u;
            const bool dpi_icc_xmp_i16b =
                has_dpi && icc_profile_size > 0u && xmp_size > 0u && ascii_count == 0u;
            const bool dpi_icc_ascii_i16b =
                has_dpi && icc_profile_size > 0u && xmp_size == 0u &&
                ((ascii_count == 1u && (ascii_tags[0] == 270 || ascii_tags[0] == 315)) ||
                    (ascii_count == 2u &&
                        ascii_tags[ascii_order[0]] == 270 && ascii_tags[ascii_order[1]] == 315));
            const bool dpi_xmp_ascii_i16b =
                has_dpi && icc_profile_size == 0u && xmp_size > 0u &&
                ((ascii_count == 1u && (ascii_tags[0] == 270 || ascii_tags[0] == 315)) ||
                    (ascii_count == 2u &&
                        ascii_tags[ascii_order[0]] == 270 && ascii_tags[ascii_order[1]] == 315));
            const bool dpi_description_i16b =
                has_dpi && icc_profile_size == 0u && xmp_size == 0u &&
                ascii_count == 1u && ascii_tags[0] == 270;
            const bool dpi_artist_i16b =
                has_dpi && icc_profile_size == 0u && xmp_size == 0u &&
                ascii_count == 1u && ascii_tags[0] == 315;
            const bool dpi_ascii_pair_i16b =
                has_dpi && icc_profile_size == 0u && xmp_size == 0u &&
                ascii_count == 2u &&
                ascii_tags[ascii_order[0]] == 270 && ascii_tags[ascii_order[1]] == 315;
            const bool full_ascii_i16b =
                has_dpi && icc_profile_size > 0u && xmp_size > 0u &&
                ascii_count == 1u && (ascii_tags[0] == 270 || ascii_tags[0] == 315);
            const bool full_metadata_i16b =
                has_dpi && icc_profile_size > 0u && xmp_size > 0u && ascii_count == 2u;
            if (image_count != 1u ||
                (!bare_i16b && !dpi_only_i16b && !dpi_icc_i16b && !dpi_xmp_i16b &&
                    !dpi_icc_xmp_i16b && !dpi_icc_ascii_i16b &&
                    !dpi_xmp_ascii_i16b &&
                    !dpi_description_i16b && !dpi_artist_i16b && !dpi_ascii_pair_i16b &&
                    !full_ascii_i16b && !full_metadata_i16b)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            return save_tiff_i16b_image(
                images[0],
                path,
                has_dpi,
                dpi_x,
                dpi_y,
                icc_profile,
                icc_profile_size,
                xmp,
                xmp_size,
                ascii_tags,
                ascii_values,
                ascii_sizes,
                ascii_count);
        }
        if (i16_family_count != image_count) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        try {
            std::vector<PillowCImage> normalized_storage;
            normalized_storage.reserve(image_count);
            for (std::size_t index = 0u; index < image_count; ++index) {
                normalized_storage.push_back(*images[index]);
                normalized_storage.back().mode = PILLOW_C_MODE_I16;
                if (images[index]->mode == PILLOW_C_MODE_I16B) {
                    normalized_storage.back().pixels = tiff_i16b_to_i16_pixels(images[index]);
                }
            }
            std::vector<const PillowCImage*> normalized_images;
            normalized_images.reserve(image_count);
            for (const PillowCImage& normalized : normalized_storage) {
                normalized_images.push_back(&normalized);
            }
            return save_tiff_frames_image_with_ascii_entries_options(
                normalized_images.data(),
                image_count,
                path,
                has_dpi,
                dpi_x,
                dpi_y,
                compression,
                icc_profile,
                icc_profile_size,
                xmp,
                xmp_size,
                ascii_tags,
                ascii_values,
                ascii_sizes,
                ascii_count);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }
    std::uint32_t x_resolution_numerator = 0;
    std::uint32_t y_resolution_numerator = 0;
    int status = tiff_dpi_to_rational(has_dpi, dpi_x, dpi_y, &x_resolution_numerator, &y_resolution_numerator);
    if (status != PILLOW_C_OK) {
        return status;
    }

    try {
        std::vector<TiffFrameLayout> layouts(image_count);
        std::vector<std::vector<std::uint8_t>> prepared_pixels(image_count);
        for (std::size_t index = 0; index < image_count; ++index) {
            const PillowCImage* image = images[index];
            if (image->mode == PILLOW_C_MODE_1) {
                prepared_pixels[index] = tiff_pack_mode_one_pixels(image);
            }
            if (compression == TIFF_COMPRESSION_PACKBITS) {
                const std::uint8_t* source_pixels = image->mode == PILLOW_C_MODE_1
                    ? prepared_pixels[index].data()
                    : image->pixels.data();
                const std::size_t source_row_stride = tiff_uncompressed_row_stride(image);
                prepared_pixels[index] = tiff_packbits_encode_pixels(source_pixels, source_row_stride, image->height);
            }
            if (compression == TIFF_COMPRESSION_LZW) {
                const std::uint8_t* source_pixels = image->mode == PILLOW_C_MODE_1
                    ? prepared_pixels[index].data()
                    : image->pixels.data();
                const std::size_t source_row_stride = tiff_uncompressed_row_stride(image);
                if (!tiff_lzw_encode_pixels(source_pixels, source_row_stride, image->height, &prepared_pixels[index])) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
            if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
                const std::uint8_t* source_pixels = image->mode == PILLOW_C_MODE_1
                    ? prepared_pixels[index].data()
                    : image->pixels.data();
                const std::size_t source_row_stride = tiff_uncompressed_row_stride(image);
                if (!tiff_deflate_encode_pixels(source_pixels, source_row_stride, image->height, &prepared_pixels[index])) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
        }
        std::size_t cursor = 8u;
        for (std::size_t index = 0; index < image_count; ++index) {
            const PillowCImage* image = images[index];
            TiffFrameLayout& layout = layouts[index];
            if (!tiff_u32_offset(cursor, &layout.ifd_offset)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const bool is_mode_one = image->mode == PILLOW_C_MODE_1;
            const bool is_palette = image->mode == PILLOW_C_MODE_P;
            const bool is_numeric = image->mode == PILLOW_C_MODE_I || image->mode == PILLOW_C_MODE_F;
            const bool is_i16 = image->mode == PILLOW_C_MODE_I16;
            const bool has_bits_per_sample = !is_mode_one;
            const bool has_samples_per_pixel =
                image->channels > 1 && !is_mode_one && !is_palette && !is_numeric && !is_i16;
            const bool has_planar_config =
                image->mode == PILLOW_C_MODE_L || image->channels > 1 || is_mode_one || is_palette || is_numeric || is_i16;
            const bool has_extra_samples = image->mode == PILLOW_C_MODE_LA || image->mode == PILLOW_C_MODE_RGBA;
            const bool has_sample_format = is_numeric;
            const bool has_icc_profile = icc_profile_size > 0u;
            const bool has_xmp = xmp_size > 0u;
            const std::size_t base_entry_count =
                7u + (has_bits_per_sample ? 1u : 0u) + (has_samples_per_pixel ? 1u : 0u) +
                (has_dpi ? 3u : 0u) + (has_planar_config ? 1u : 0u) + (has_extra_samples ? 1u : 0u) +
                (is_palette ? 1u : 0u) + (has_sample_format ? 1u : 0u) +
                (has_xmp ? 1u : 0u) + (has_icc_profile ? 1u : 0u);
            if (base_entry_count + ascii_count > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
                return PILLOW_C_INVALID_LENGTH;
            }
            layout.entry_count = static_cast<std::uint16_t>(base_entry_count + ascii_count);
            layout.ascii_offsets.assign(ascii_count, 0u);

            const std::size_t ifd_bytes = 2u + static_cast<std::size_t>(layout.entry_count) * 12u + 4u;
            const std::uint32_t bits_per_sample_count =
                (is_numeric || is_i16) ? 1u : static_cast<std::uint32_t>(image->channels);
            const bool inline_bits_per_sample = bits_per_sample_count <= 2u;
            const std::size_t bits_bytes = bits_per_sample_count > 1u && !inline_bits_per_sample
                ? static_cast<std::size_t>(bits_per_sample_count) * 2u
                : 0u;
            const std::size_t bits_offset = cursor + ifd_bytes;
            const std::size_t x_resolution_offset = bits_offset + bits_bytes;
            const std::size_t y_resolution_offset = x_resolution_offset + 8u;
            const std::size_t resolution_bytes = has_dpi ? 16u : 0u;
            const bool inline_xmp = has_xmp && xmp_size <= 4u;
            const std::size_t xmp_offset = align_tiff_offset(bits_offset + bits_bytes + resolution_bytes);
            const std::size_t xmp_bytes = has_xmp && !inline_xmp ? xmp_size : 0u;
            const bool inline_icc_profile = has_icc_profile && icc_profile_size <= 4u;
            const std::size_t icc_profile_offset = align_tiff_offset(xmp_offset + xmp_bytes);
            const std::size_t icc_profile_bytes = has_icc_profile && !inline_icc_profile ? icc_profile_size : 0u;
            std::size_t ascii_data_cursor = align_tiff_offset(icc_profile_offset + icc_profile_bytes);
            for (std::size_t order_index = 0u; order_index < ascii_count; ++order_index) {
                const std::size_t entry_index = ascii_order[order_index];
                if (ascii_sizes[entry_index] <= 4u) {
                    continue;
                }
                ascii_data_cursor = align_tiff_offset(ascii_data_cursor);
                if (!tiff_u32_offset(ascii_data_cursor, &layout.ascii_offsets[entry_index])) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                ascii_data_cursor += ascii_sizes[entry_index];
            }
            const std::size_t color_map_offset = align_tiff_offset(ascii_data_cursor);
            const std::size_t color_map_bytes = is_palette ? 256u * 3u * 2u : 0u;
            const std::size_t pixel_offset = align_tiff_offset(color_map_offset + color_map_bytes);
            const std::size_t pixel_byte_count = prepared_pixels[index].empty()
                ? image->pixels.size()
                : prepared_pixels[index].size();
            const std::size_t next_cursor = align_tiff_offset(pixel_offset + pixel_byte_count);
            if ((bits_bytes > 0u && !tiff_u32_offset(bits_offset, &layout.bits_offset)) ||
                (has_dpi && (!tiff_u32_offset(x_resolution_offset, &layout.x_resolution_offset) ||
                             !tiff_u32_offset(y_resolution_offset, &layout.y_resolution_offset))) ||
                (has_xmp && !inline_xmp && !tiff_u32_offset(xmp_offset, &layout.xmp_offset)) ||
                (has_icc_profile && !inline_icc_profile &&
                    !tiff_u32_offset(icc_profile_offset, &layout.icc_profile_offset)) ||
                (is_palette && !tiff_u32_offset(color_map_offset, &layout.color_map_offset)) ||
                !tiff_u32_offset(pixel_offset, &layout.pixel_offset) ||
                !tiff_u32_offset(pixel_byte_count, &layout.pixel_byte_count)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            cursor = next_cursor;
        }
        for (std::size_t index = 0; index + 1u < image_count; ++index) {
            layouts[index].next_ifd_offset = layouts[index + 1u].ifd_offset;
        }
        if (cursor > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> out;
        out.reserve(cursor);
        out.push_back('I');
        out.push_back('I');
        append_le16(out, 42u);
        append_le32(out, layouts[0].ifd_offset);

        for (std::size_t index = 0; index < image_count; ++index) {
            const PillowCImage* image = images[index];
            const TiffFrameLayout& layout = layouts[index];
            const bool is_mode_one = image->mode == PILLOW_C_MODE_1;
            const bool is_palette = image->mode == PILLOW_C_MODE_P;
            const bool is_numeric = image->mode == PILLOW_C_MODE_I || image->mode == PILLOW_C_MODE_F;
            const bool is_i16 = image->mode == PILLOW_C_MODE_I16;
            const bool has_samples_per_pixel =
                image->channels > 1 && !is_mode_one && !is_palette && !is_numeric && !is_i16;
            const bool has_planar_config =
                image->mode == PILLOW_C_MODE_L || image->channels > 1 || is_mode_one || is_palette || is_numeric || is_i16;
            const auto append_ascii_entry = [&](int wanted_tag) {
                for (std::size_t order_index = 0u; order_index < ascii_count; ++order_index) {
                    const std::size_t entry_index = ascii_order[order_index];
                    if (ascii_tags[entry_index] != wanted_tag) {
                        continue;
                    }
                    const std::size_t size = ascii_sizes[entry_index];
                    const std::uint8_t* value_bytes = ascii_values[entry_index];
                    std::uint32_t value = layout.ascii_offsets[entry_index];
                    if (size <= 4u) {
                        value = 0u;
                        for (std::size_t byte_index = 0u; byte_index < size; ++byte_index) {
                            value |= static_cast<std::uint32_t>(value_bytes[byte_index]) << (byte_index * 8u);
                        }
                    }
                    append_tiff_entry(
                        out,
                        static_cast<std::uint16_t>(wanted_tag),
                        2u,
                        static_cast<std::uint32_t>(size),
                        value);
                    return;
                }
            };
            if (out.size() < layout.ifd_offset) {
                out.resize(layout.ifd_offset, 0);
            }

            append_le16(out, layout.entry_count);
            append_tiff_entry(out, 256u, 4u, 1u, static_cast<std::uint32_t>(image->width));
            append_tiff_entry(out, 257u, 4u, 1u, static_cast<std::uint32_t>(image->height));
            if (image->mode != PILLOW_C_MODE_1) {
                const std::uint32_t bits_per_sample_count =
                    (is_numeric || is_i16) ? 1u : static_cast<std::uint32_t>(image->channels);
                const std::uint32_t bits_per_sample_value =
                    is_numeric ? 32u :
                        (is_i16 ? 16u :
                        (image->channels == 1 ? 8u :
                            (image->channels == 2 ? (8u | (8u << 16)) : layout.bits_offset)));
                append_tiff_entry(
                    out,
                    258u,
                    3u,
                    bits_per_sample_count,
                    bits_per_sample_value);
            }
            append_tiff_entry(out, 259u, 3u, 1u, compression);
            append_tiff_entry(
                out,
                262u,
                3u,
                1u,
                is_palette ? 3u :
                    (image->mode == PILLOW_C_MODE_CMYK ? 5u :
                        ((image->channels == 1 || image->mode == PILLOW_C_MODE_LA || is_numeric || is_i16) ? 1u : 2u)));
            append_ascii_entry(270);
            append_tiff_entry(out, 273u, 4u, 1u, layout.pixel_offset);
            if (has_samples_per_pixel) {
                append_tiff_entry(out, 277u, 3u, 1u, static_cast<std::uint32_t>(image->channels));
            }
            append_tiff_entry(out, 278u, 4u, 1u, static_cast<std::uint32_t>(image->height));
            append_tiff_entry(out, 279u, 4u, 1u, layout.pixel_byte_count);
            if (has_dpi) {
                append_tiff_entry(out, 282u, 5u, 1u, layout.x_resolution_offset);
                append_tiff_entry(out, 283u, 5u, 1u, layout.y_resolution_offset);
            }
            if (has_planar_config) {
                append_tiff_entry(out, 284u, 3u, 1u, 1u);
            }
            if (has_dpi) {
                append_tiff_entry(out, 296u, 3u, 1u, 2u);
            }
            append_ascii_entry(315);
            if (image->mode == PILLOW_C_MODE_LA || image->mode == PILLOW_C_MODE_RGBA) {
                append_tiff_entry(out, 338u, 3u, 1u, 2u);
            }
            if (image->mode == PILLOW_C_MODE_P) {
                append_tiff_entry(out, 320u, 3u, 768u, layout.color_map_offset);
            }
            if (is_numeric) {
                append_tiff_entry(out, 339u, 3u, 1u, image->mode == PILLOW_C_MODE_I ? 2u : 3u);
            }
            if (xmp_size > 0u) {
                std::uint32_t value = layout.xmp_offset;
                if (xmp_size <= 4u) {
                    value = 0u;
                    for (std::size_t byte_index = 0u; byte_index < xmp_size; ++byte_index) {
                        value |= static_cast<std::uint32_t>(xmp[byte_index]) << (byte_index * 8u);
                    }
                }
                append_tiff_entry(out, 700u, 1u, static_cast<std::uint32_t>(xmp_size), value);
            }
            if (icc_profile_size > 0u) {
                std::uint32_t value = layout.icc_profile_offset;
                if (icc_profile_size <= 4u) {
                    value = 0u;
                    for (std::size_t byte_index = 0u; byte_index < icc_profile_size; ++byte_index) {
                        value |= static_cast<std::uint32_t>(icc_profile[byte_index]) << (byte_index * 8u);
                    }
                }
                append_tiff_entry(
                    out,
                    34675u,
                    7u,
                    static_cast<std::uint32_t>(icc_profile_size),
                    value);
            }
            append_le32(out, layout.next_ifd_offset);

            if (!is_numeric && image->channels > 2) {
                for (int channel = 0; channel < image->channels; ++channel) {
                    append_le16(out, 8u);
                }
            }
            if (has_dpi) {
                append_le32(out, x_resolution_numerator);
                append_le32(out, 1u);
                append_le32(out, y_resolution_numerator);
                append_le32(out, 1u);
            }
            if (xmp_size > 4u) {
                if (out.size() < layout.xmp_offset) {
                    out.resize(layout.xmp_offset, 0);
                }
                out.insert(out.end(), xmp, xmp + xmp_size);
            }
            if (icc_profile_size > 4u) {
                if (out.size() < layout.icc_profile_offset) {
                    out.resize(layout.icc_profile_offset, 0);
                }
                out.insert(out.end(), icc_profile, icc_profile + icc_profile_size);
            }
            for (std::size_t order_index = 0u; order_index < ascii_count; ++order_index) {
                const std::size_t entry_index = ascii_order[order_index];
                const std::size_t size = ascii_sizes[entry_index];
                if (size <= 4u) {
                    continue;
                }
                const std::uint32_t offset = layout.ascii_offsets[entry_index];
                if (out.size() < offset) {
                    out.resize(offset, 0);
                }
                const std::uint8_t* value = ascii_values[entry_index];
                out.insert(out.end(), value, value + size);
            }
            if (image->mode == PILLOW_C_MODE_P) {
                if (out.size() < layout.color_map_offset) {
                    out.resize(layout.color_map_offset, 0);
                }
                append_tiff_palette_color_map(out, image->palette_rgb);
            }
            if (out.size() < layout.pixel_offset) {
                out.resize(layout.pixel_offset, 0);
            }
            if (!prepared_pixels[index].empty()) {
                out.insert(out.end(), prepared_pixels[index].begin(), prepared_pixels[index].end());
            } else {
                out.insert(out.end(), image->pixels.begin(), image->pixels.end());
            }
        }
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_frames_image_with_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    std::uint16_t compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    int ascii_tag,
    const std::uint8_t* ascii_value,
    std::size_t ascii_size)
{
    if (ascii_size == 0u && ascii_tag != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int ascii_tags[] = { ascii_tag };
    const std::uint8_t* ascii_values[] = { ascii_value };
    const std::size_t ascii_sizes[] = { ascii_size };
    const std::size_t ascii_count = ascii_size > 0u ? 1u : 0u;
    return save_tiff_frames_image_with_ascii_entries_options(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        ascii_tags,
        ascii_values,
        ascii_sizes,
        ascii_count);
}

int save_tiff_frames_image(const PillowCImage* const* images, std::size_t image_count, const char* path)
{
    return save_tiff_frames_image_with_options(
        images, image_count, path, false, 0.0, 0.0, TIFF_COMPRESSION_NONE,
        nullptr, 0u, nullptr, 0u, 0, nullptr, 0u);
}

int save_tiff_frames_image_with_compression_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int compression)
{
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_frames_image_with_options(
        images,
        image_count,
        path,
        false,
        0.0,
        0.0,
        normalized_compression,
        nullptr,
        0u,
        nullptr,
        0u,
        0,
        nullptr,
        0u);
}

int save_tiff_frames_image_with_save_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compression)
{
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_frames_image_with_options(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        nullptr,
        0u,
        nullptr,
        0u,
        0,
        nullptr,
        0u);
}

int save_tiff_frames_image_with_metadata_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    if (!icc_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (icc_profile_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_frames_image_with_options(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        icc_profile,
        icc_profile_size,
        nullptr,
        0u,
        0,
        nullptr,
        0u);
}

int save_tiff_frames_image_with_metadata_ex_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size)
{
    if (icc_profile_size == 0u && xmp_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_frames_image_with_options(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        0,
        nullptr,
        0u);
}

int save_tiff_frames_image_with_metadata_ascii_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    int ascii_tag,
    const std::uint8_t* ascii_value,
    std::size_t ascii_size)
{
    if (!ascii_value) {
        return PILLOW_C_NULL_POINTER;
    }
    if (ascii_size == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_frames_image_with_options(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        ascii_tag,
        ascii_value,
        ascii_size);
}

int save_tiff_frames_image_with_metadata_ascii_entries_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* ascii_tags,
    const std::uint8_t* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count)
{
    if (ascii_count == 0u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_frames_image_with_ascii_entries_options(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        ascii_tags,
        ascii_values,
        ascii_sizes,
        ascii_count);
}

int save_tiff_image(const PillowCImage* image, const char* path)
{
    const PillowCImage* images[] = { image };
    return save_tiff_frames_image(images, 1u, path);
}

int save_tiff_image_with_options(const PillowCImage* image, const char* path, bool has_dpi, double dpi_x, double dpi_y)
{
    const PillowCImage* images[] = { image };
    return save_tiff_frames_image_with_options(
        images, 1u, path, has_dpi, dpi_x, dpi_y, TIFF_COMPRESSION_NONE,
        nullptr, 0u, nullptr, 0u, 0, nullptr, 0u);
}

int save_tiff_image_with_compression_options(
    const PillowCImage* image,
    const char* path,
    bool has_dpi,
    double dpi_x,
    double dpi_y,
    int compression)
{
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const PillowCImage* images[] = { image };
    return save_tiff_frames_image_with_options(
        images,
        1u,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        nullptr,
        0u,
        nullptr,
        0u,
        0,
        nullptr,
        0u);
}

struct TiffPatchExifEntry
{
    std::uint16_t tag;
    std::uint16_t type;
    std::uint32_t count;
    std::uint32_t value;
    bool has_blob;
    std::vector<std::uint8_t> blob;
};

int patch_tiff_ifd0_exif_entries(
    const char* path,
    const int* ascii_tags,
    const std::uint8_t* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    const int* rational_array_tags,
    const std::uint32_t* rational_array_numerators,
    const std::uint32_t* rational_array_denominators,
    std::size_t rational_array_value_count,
    const std::size_t* rational_array_offsets,
    const std::size_t* rational_array_counts,
    std::size_t rational_array_count,
    const int* short_array_tags,
    const std::uint32_t* short_array_values,
    std::size_t short_array_value_count,
    const std::size_t* short_array_offsets,
    const std::size_t* short_array_counts,
    std::size_t short_array_count,
    const int* byte_array_tags,
    const std::uint8_t* byte_array_values,
    std::size_t byte_array_value_count,
    const std::size_t* byte_array_offsets,
    const std::size_t* byte_array_counts,
    std::size_t byte_array_count,
    const int* uint_array_tags,
    const std::uint32_t* uint_array_values,
    std::size_t uint_array_value_count,
    const std::size_t* uint_array_offsets,
    const std::size_t* uint_array_counts,
    std::size_t uint_array_count,
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count)
{
    if (!path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (ascii_count > 0u && (!ascii_tags || !ascii_values || !ascii_sizes)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (uint_count > 0u && (!uint_tags || !uint_values || !uint_types)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (rational_count > 0u && (!rational_tags || !rational_numerators || !rational_denominators)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (rational_array_count > 0u &&
        (!rational_array_tags || !rational_array_numerators || !rational_array_denominators ||
            !rational_array_offsets || !rational_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (short_array_count > 0u &&
        (!short_array_tags || !short_array_values || !short_array_offsets || !short_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (byte_array_count > 0u &&
        (!byte_array_tags || !byte_array_values || !byte_array_offsets || !byte_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (uint_array_count > 0u &&
        (!uint_array_tags || !uint_array_values || !uint_array_offsets || !uint_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (signed_rational_count > 0u &&
        (!signed_rational_tags || !signed_rational_numerators || !signed_rational_denominators)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (undefined_count > 0u &&
        (!undefined_tags || !undefined_values || !undefined_offsets || !undefined_counts)) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data) || data.size() < 8u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (data[0] != 'I' || data[1] != 'I' || read_le16(data.data() + 2u) != 42u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint32_t ifd_offset = read_le32(data.data() + 4u);
    if (ifd_offset > data.size() || data.size() - ifd_offset < 6u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint16_t old_count = read_le16(data.data() + ifd_offset);
    const std::size_t old_ifd_bytes = 2u + static_cast<std::size_t>(old_count) * 12u + 4u;
    if (old_ifd_bytes > data.size() - ifd_offset) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint32_t next_ifd = read_le32(data.data() + ifd_offset + 2u + static_cast<std::size_t>(old_count) * 12u);
    if (next_ifd != 0u) {
        // Bounded: patch only the single-frame save layout.
        return PILLOW_C_INVALID_ARGUMENT;
    }

    struct OldEntry
    {
        std::uint16_t tag;
        std::uint16_t type;
        std::uint32_t count;
        std::uint32_t value;
    };
    std::vector<OldEntry> old_entries;
    old_entries.reserve(old_count);
    for (std::uint16_t index = 0u; index < old_count; ++index) {
        const std::uint8_t* entry = data.data() + ifd_offset + 2u + static_cast<std::size_t>(index) * 12u;
        OldEntry parsed;
        parsed.tag = read_le16(entry);
        parsed.type = read_le16(entry + 2u);
        parsed.count = read_le32(entry + 4u);
        parsed.value = read_le32(entry + 8u);
        old_entries.push_back(parsed);
    }
    const auto old_type_size = [](std::uint16_t type) -> std::size_t {
        switch (type) {
        case 1u:
        case 2u:
        case 7u:
            return 1u;
        case 3u:
            return 2u;
        case 4u:
        case 11u:
            return 4u;
        case 5u:
        case 10u:
        case 12u:
        case 16u:
            return 8u;
        default:
            return 0u;
        }
    };
    const auto tag_in_old = [&old_entries](std::uint16_t tag) {
        for (const OldEntry& entry : old_entries) {
            if (entry.tag == tag) {
                return true;
            }
        }
        return false;
    };

    std::vector<TiffPatchExifEntry> new_entries;
    const auto add_blob_entry = [&new_entries, &tag_in_old](
                                    std::uint16_t tag,
                                    std::uint16_t type,
                                    std::uint32_t count,
                                    const std::uint8_t* bytes,
                                    std::size_t size) {
        if (tag_in_old(tag)) {
            return;
        }
        TiffPatchExifEntry entry;
        entry.tag = tag;
        entry.type = type;
        entry.count = count;
        entry.value = 0u;
        if (size <= 4u) {
            for (std::size_t byte_index = 0u; byte_index < size; ++byte_index) {
                entry.value |= static_cast<std::uint32_t>(bytes[byte_index]) << (byte_index * 8u);
            }
            entry.has_blob = false;
        } else {
            entry.has_blob = true;
            entry.blob.assign(bytes, bytes + size);
        }
        new_entries.push_back(entry);
    };

    try {
        for (std::size_t index = 0u; index < ascii_count; ++index) {
            const std::uint8_t* value = ascii_values[index];
            const std::size_t size = ascii_sizes[index];
            if (!value || size == 0u || size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint8_t> terminated(value, value + size);
            if (terminated.back() != 0u) {
                terminated.push_back(0u);
            }
            add_blob_entry(
                static_cast<std::uint16_t>(ascii_tags[index]),
                2u,
                static_cast<std::uint32_t>(terminated.size()),
                terminated.data(),
                terminated.size());
        }
        for (std::size_t index = 0u; index < uint_count; ++index) {
            const int type = uint_types[index];
            const std::uint32_t value = uint_values[index];
            if (type == 3 && value <= static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max())) {
                const std::uint8_t bytes[2] = {
                    static_cast<std::uint8_t>(value & 0xFFu),
                    static_cast<std::uint8_t>((value >> 8u) & 0xFFu)};
                add_blob_entry(static_cast<std::uint16_t>(uint_tags[index]), 3u, 1u, bytes, 2u);
            } else if (value <= static_cast<std::uint32_t>(std::numeric_limits<std::uint32_t>::max())) {
                const std::uint8_t bytes[4] = {
                    static_cast<std::uint8_t>(value & 0xFFu),
                    static_cast<std::uint8_t>((value >> 8u) & 0xFFu),
                    static_cast<std::uint8_t>((value >> 16u) & 0xFFu),
                    static_cast<std::uint8_t>((value >> 24u) & 0xFFu)};
                add_blob_entry(static_cast<std::uint16_t>(uint_tags[index]), 4u, 1u, bytes, 4u);
            }
        }
        for (std::size_t index = 0u; index < rational_count; ++index) {
            const std::uint32_t numerator = rational_numerators[index];
            const std::uint32_t denominator = rational_denominators[index];
            if (denominator == 0u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint8_t bytes[8] = {
                static_cast<std::uint8_t>(numerator & 0xFFu),
                static_cast<std::uint8_t>((numerator >> 8u) & 0xFFu),
                static_cast<std::uint8_t>((numerator >> 16u) & 0xFFu),
                static_cast<std::uint8_t>((numerator >> 24u) & 0xFFu),
                static_cast<std::uint8_t>(denominator & 0xFFu),
                static_cast<std::uint8_t>((denominator >> 8u) & 0xFFu),
                static_cast<std::uint8_t>((denominator >> 16u) & 0xFFu),
                static_cast<std::uint8_t>((denominator >> 24u) & 0xFFu)};
            add_blob_entry(static_cast<std::uint16_t>(rational_tags[index]), 5u, 1u, bytes, 8u);
        }
        for (std::size_t index = 0u; index < rational_array_count; ++index) {
            const std::size_t offset = rational_array_offsets[index];
            const std::size_t count = rational_array_counts[index];
            if (offset > rational_array_value_count || count > rational_array_value_count - offset ||
                count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint8_t> bytes;
            bytes.reserve(count * 8u);
            for (std::size_t value_index = 0u; value_index < count; ++value_index) {
                const std::uint32_t numerator = rational_array_numerators[offset + value_index];
                const std::uint32_t denominator = rational_array_denominators[offset + value_index];
                if (denominator == 0u) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                bytes.push_back(static_cast<std::uint8_t>(numerator & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((numerator >> 8u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((numerator >> 16u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((numerator >> 24u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>(denominator & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((denominator >> 8u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((denominator >> 16u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((denominator >> 24u) & 0xFFu));
            }
            add_blob_entry(
                static_cast<std::uint16_t>(rational_array_tags[index]),
                5u,
                static_cast<std::uint32_t>(count),
                bytes.data(),
                bytes.size());
        }
        for (std::size_t index = 0u; index < signed_rational_count; ++index) {
            const std::int32_t numerator = signed_rational_numerators[index];
            const std::int32_t denominator = signed_rational_denominators[index];
            if (denominator == 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint32_t raw_numerator = static_cast<std::uint32_t>(numerator);
            const std::uint32_t raw_denominator = static_cast<std::uint32_t>(denominator);
            const std::uint8_t bytes[8] = {
                static_cast<std::uint8_t>(raw_numerator & 0xFFu),
                static_cast<std::uint8_t>((raw_numerator >> 8u) & 0xFFu),
                static_cast<std::uint8_t>((raw_numerator >> 16u) & 0xFFu),
                static_cast<std::uint8_t>((raw_numerator >> 24u) & 0xFFu),
                static_cast<std::uint8_t>(raw_denominator & 0xFFu),
                static_cast<std::uint8_t>((raw_denominator >> 8u) & 0xFFu),
                static_cast<std::uint8_t>((raw_denominator >> 16u) & 0xFFu),
                static_cast<std::uint8_t>((raw_denominator >> 24u) & 0xFFu)};
            add_blob_entry(static_cast<std::uint16_t>(signed_rational_tags[index]), 10u, 1u, bytes, 8u);
        }
        for (std::size_t index = 0u; index < short_array_count; ++index) {
            const std::size_t offset = short_array_offsets[index];
            const std::size_t count = short_array_counts[index];
            if (offset > short_array_value_count || count > short_array_value_count - offset ||
                count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint8_t> bytes;
            bytes.reserve(count * 2u);
            for (std::size_t value_index = 0u; value_index < count; ++value_index) {
                const std::uint32_t value = short_array_values[offset + value_index];
                bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
            }
            add_blob_entry(
                static_cast<std::uint16_t>(short_array_tags[index]),
                3u,
                static_cast<std::uint32_t>(count),
                bytes.data(),
                bytes.size());
        }
        for (std::size_t index = 0u; index < byte_array_count; ++index) {
            const std::size_t offset = byte_array_offsets[index];
            const std::size_t count = byte_array_counts[index];
            if (offset > byte_array_value_count || count > byte_array_value_count - offset ||
                count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            add_blob_entry(
                static_cast<std::uint16_t>(byte_array_tags[index]),
                1u,
                static_cast<std::uint32_t>(count),
                byte_array_values + offset,
                count);
        }
        for (std::size_t index = 0u; index < uint_array_count; ++index) {
            const std::size_t offset = uint_array_offsets[index];
            const std::size_t count = uint_array_counts[index];
            if (offset > uint_array_value_count || count > uint_array_value_count - offset ||
                count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint8_t> bytes;
            bytes.reserve(count * 4u);
            for (std::size_t value_index = 0u; value_index < count; ++value_index) {
                const std::uint32_t value = uint_array_values[offset + value_index];
                bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xFFu));
            }
            add_blob_entry(
                static_cast<std::uint16_t>(uint_array_tags[index]),
                4u,
                static_cast<std::uint32_t>(count),
                bytes.data(),
                bytes.size());
        }
        for (std::size_t index = 0u; index < undefined_count; ++index) {
            const std::size_t offset = undefined_offsets[index];
            const std::size_t count = undefined_counts[index];
            if (offset > undefined_value_count || count > undefined_value_count - offset ||
                count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            add_blob_entry(
                static_cast<std::uint16_t>(undefined_tags[index]),
                7u,
                static_cast<std::uint32_t>(count),
                undefined_values + offset,
                count);
        }

        std::sort(new_entries.begin(), new_entries.end(), [](const TiffPatchExifEntry& left, const TiffPatchExifEntry& right) {
            return left.tag < right.tag;
        });
        const std::size_t total_count = static_cast<std::size_t>(old_count) + new_entries.size();
        if (total_count > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
            return PILLOW_C_INVALID_LENGTH;
        }
        const std::size_t new_ifd_bytes = 2u + total_count * 12u + 4u;
        std::size_t exif_blob_bytes = 0u;
        for (TiffPatchExifEntry& entry : new_entries) {
            if (entry.has_blob) {
                entry.value = static_cast<std::uint32_t>(8u + new_ifd_bytes + exif_blob_bytes);
                exif_blob_bytes += entry.blob.size();
            }
        }
        const std::size_t old_region_offset = ifd_offset + old_ifd_bytes;
        const std::size_t old_region_size = data.size() - old_region_offset;
        const std::ptrdiff_t delta = static_cast<std::ptrdiff_t>(new_ifd_bytes + exif_blob_bytes) -
            static_cast<std::ptrdiff_t>(old_ifd_bytes);

        std::vector<std::uint8_t> out;
        out.reserve(8u + new_ifd_bytes + exif_blob_bytes + old_region_size);
        out.push_back('I');
        out.push_back('I');
        append_le16(out, 42u);
        append_le32(out, 8u);
        append_le16(out, static_cast<std::uint16_t>(total_count));
        std::size_t old_index = 0u;
        std::size_t new_index = 0u;
        while (old_index < old_entries.size() || new_index < new_entries.size()) {
            const bool take_old =
                new_index >= new_entries.size() ||
                (old_index < old_entries.size() && old_entries[old_index].tag <= new_entries[new_index].tag);
            if (take_old) {
                const OldEntry& entry = old_entries[old_index];
                std::uint32_t value = entry.value;
                const std::size_t type_size = old_type_size(entry.type);
                if (type_size == 0u) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const bool inline_file_offset =
                    entry.tag == 273u && entry.type == 4u && entry.count == 1u;
                if (entry.count > 0u &&
                    ((inline_file_offset) ||
                        (entry.count > static_cast<std::uint32_t>(std::numeric_limits<std::size_t>::max() / type_size) ||
                            static_cast<std::size_t>(entry.count) * type_size > 4u))) {
                    if (delta > 0) {
                        if (entry.value > static_cast<std::uint32_t>(std::numeric_limits<std::uint32_t>::max()) - static_cast<std::uint32_t>(delta)) {
                            return PILLOW_C_INVALID_ARGUMENT;
                        }
                        value += static_cast<std::uint32_t>(delta);
                    } else if (value < static_cast<std::uint32_t>(-delta)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    } else {
                        value -= static_cast<std::uint32_t>(-delta);
                    }
                }
                append_tiff_entry(out, entry.tag, entry.type, entry.count, value);
                ++old_index;
            } else {
                const TiffPatchExifEntry& entry = new_entries[new_index];
                append_tiff_entry(out, entry.tag, entry.type, entry.count, entry.value);
                ++new_index;
            }
        }
        append_le32(out, 0u);
        for (const TiffPatchExifEntry& entry : new_entries) {
            if (entry.has_blob) {
                out.insert(out.end(), entry.blob.begin(), entry.blob.end());
            }
        }
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(old_region_offset), data.end());
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int patch_tiff_ifd0_exif_blob(
    const char* path,
    const std::uint8_t* exif_bytes,
    std::size_t exif_size)
{
    if (!path || !exif_bytes || exif_size < 8u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint8_t* tiff = exif_bytes;
    std::size_t tiff_size = exif_size;
    if (tiff_size >= 6u && tiff[0] == 'E' && tiff[1] == 'x' && tiff[2] == 'i' && tiff[3] == 'f' &&
        tiff[4] == 0u && tiff[5] == 0u) {
        tiff += 6u;
        tiff_size -= 6u;
    }
    if (tiff_size < 8u || tiff[0] != 'M' || tiff[1] != 'M' || read_be16(tiff + 2u) != 42u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint32_t ifd_offset = read_be32(tiff + 4u);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 6u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint16_t entry_count = read_be16(tiff + ifd_offset);
    if (entry_count == 0u || entry_count > 4096u ||
        static_cast<std::size_t>(2u + static_cast<std::size_t>(entry_count) * 12u + 4u) >
            tiff_size - ifd_offset) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const auto blob_type_size = [](std::uint16_t type) -> std::size_t {
        switch (type) {
        case 1u:
        case 2u:
        case 7u:
            return 1u;
        case 3u:
            return 2u;
        case 4u:
        case 11u:
            return 4u;
        case 5u:
        case 10u:
        case 12u:
        case 16u:
            return 8u;
        default:
            return 0u;
        }
    };

    std::vector<int> ascii_tags;
    std::vector<std::vector<std::uint8_t>> ascii_values;
    std::vector<int> uint_tags;
    std::vector<std::uint32_t> uint_values;
    std::vector<int> uint_types;
    std::vector<int> rational_tags;
    std::vector<std::uint32_t> rational_numerators;
    std::vector<std::uint32_t> rational_denominators;
    std::vector<int> rational_array_tags;
    std::vector<std::uint32_t> rational_array_numerators;
    std::vector<std::uint32_t> rational_array_denominators;
    std::vector<std::size_t> rational_array_offsets;
    std::vector<std::size_t> rational_array_counts;
    std::vector<int> short_array_tags;
    std::vector<std::uint32_t> short_array_values;
    std::vector<std::size_t> short_array_offsets;
    std::vector<std::size_t> short_array_counts;
    std::vector<int> byte_array_tags;
    std::vector<std::uint8_t> byte_array_values;
    std::vector<std::size_t> byte_array_offsets;
    std::vector<std::size_t> byte_array_counts;
    std::vector<int> uint_array_tags;
    std::vector<std::uint32_t> uint_array_values;
    std::vector<std::size_t> uint_array_offsets;
    std::vector<std::size_t> uint_array_counts;
    std::vector<int> signed_rational_tags;
    std::vector<std::int32_t> signed_rational_numerators;
    std::vector<std::int32_t> signed_rational_denominators;
    std::vector<int> undefined_tags;
    std::vector<std::uint8_t> undefined_values;
    std::vector<std::size_t> undefined_offsets;
    std::vector<std::size_t> undefined_counts;

    try {
        for (std::uint16_t index = 0u; index < entry_count; ++index) {
            const std::uint8_t* entry = tiff + ifd_offset + 2u + static_cast<std::size_t>(index) * 12u;
            const std::uint16_t tag = read_be16(entry);
            const std::uint16_t type = read_be16(entry + 2u);
            const std::uint32_t count = read_be32(entry + 4u);
            const std::uint32_t value = read_be32(entry + 8u);
            const std::size_t type_size = blob_type_size(type);
            if (type_size == 0u || count == 0u || count > 0xFFFFFFu) {
                continue;
            }
            const std::uint64_t byte_count = static_cast<std::uint64_t>(count) * type_size;
            const std::uint8_t* data = nullptr;
            if (byte_count <= 4u) {
                data = entry + 8u;
            } else {
                if (value > tiff_size || byte_count > tiff_size - static_cast<std::size_t>(value)) {
                    continue;
                }
                data = tiff + value;
            }
            const std::size_t count_size = static_cast<std::size_t>(count);
            if (type == 2u) {
                std::size_t text_size = count_size;
                while (text_size > 0u && data[text_size - 1u] == 0u) {
                    --text_size;
                }
                if (text_size == 0u) {
                    continue;
                }
                ascii_tags.push_back(static_cast<int>(tag));
                ascii_values.emplace_back(data, data + text_size);
            } else if (type == 3u && count == 1u) {
                uint_tags.push_back(static_cast<int>(tag));
                uint_values.push_back(static_cast<std::uint32_t>(read_be16(data)));
                uint_types.push_back(3);
            } else if (type == 4u && count == 1u) {
                uint_tags.push_back(static_cast<int>(tag));
                uint_values.push_back(read_be32(data));
                uint_types.push_back(4);
            } else if (type == 5u && count == 1u) {
                const std::uint32_t numerator = read_be32(data);
                const std::uint32_t denominator = read_be32(data + 4u);
                if (denominator == 0u) {
                    continue;
                }
                rational_tags.push_back(static_cast<int>(tag));
                rational_numerators.push_back(numerator);
                rational_denominators.push_back(denominator);
            } else if (type == 10u && count == 1u) {
                const std::int32_t numerator = static_cast<std::int32_t>(read_be32(data));
                const std::int32_t denominator = static_cast<std::int32_t>(read_be32(data + 4u));
                if (denominator == 0) {
                    continue;
                }
                signed_rational_tags.push_back(static_cast<int>(tag));
                signed_rational_numerators.push_back(numerator);
                signed_rational_denominators.push_back(denominator);
            } else if (type == 5u) {
                rational_array_tags.push_back(static_cast<int>(tag));
                rational_array_offsets.push_back(rational_array_numerators.size());
                rational_array_counts.push_back(count_size);
                for (std::size_t value_index = 0u; value_index < count_size; ++value_index) {
                    const std::uint32_t denominator = read_be32(data + value_index * 8u + 4u);
                    if (denominator == 0u) {
                        rational_array_tags.pop_back();
                        rational_array_offsets.pop_back();
                        rational_array_counts.pop_back();
                        rational_array_numerators.resize(rational_array_offsets.back());
                        rational_array_denominators.resize(rational_array_offsets.back());
                        break;
                    }
                    rational_array_numerators.push_back(read_be32(data + value_index * 8u));
                    rational_array_denominators.push_back(denominator);
                }
            } else if (type == 3u) {
                short_array_tags.push_back(static_cast<int>(tag));
                short_array_offsets.push_back(short_array_values.size());
                short_array_counts.push_back(count_size);
                for (std::size_t value_index = 0u; value_index < count_size; ++value_index) {
                    short_array_values.push_back(static_cast<std::uint32_t>(read_be16(data + value_index * 2u)));
                }
            } else if (type == 4u) {
                uint_array_tags.push_back(static_cast<int>(tag));
                uint_array_offsets.push_back(uint_array_values.size());
                uint_array_counts.push_back(count_size);
                for (std::size_t value_index = 0u; value_index < count_size; ++value_index) {
                    uint_array_values.push_back(read_be32(data + value_index * 4u));
                }
            } else if (type == 1u) {
                byte_array_tags.push_back(static_cast<int>(tag));
                byte_array_offsets.push_back(byte_array_values.size());
                byte_array_counts.push_back(count_size);
                byte_array_values.insert(byte_array_values.end(), data, data + count_size);
            } else if (type == 7u) {
                undefined_tags.push_back(static_cast<int>(tag));
                undefined_offsets.push_back(undefined_values.size());
                undefined_counts.push_back(count_size);
                undefined_values.insert(undefined_values.end(), data, data + count_size);
            }
        }

        std::vector<const std::uint8_t*> ascii_ptrs;
        std::vector<std::size_t> ascii_sizes;
        ascii_ptrs.reserve(ascii_values.size());
        ascii_sizes.reserve(ascii_values.size());
        for (const std::vector<std::uint8_t>& ascii_value : ascii_values) {
            ascii_ptrs.push_back(ascii_value.data());
            ascii_sizes.push_back(ascii_value.size());
        }
        return patch_tiff_ifd0_exif_entries(
            path,
            ascii_tags.empty() ? nullptr : ascii_tags.data(),
            ascii_ptrs.empty() ? nullptr : ascii_ptrs.data(),
            ascii_sizes.empty() ? nullptr : ascii_sizes.data(),
            ascii_tags.size(),
            uint_tags.empty() ? nullptr : uint_tags.data(),
            uint_values.empty() ? nullptr : uint_values.data(),
            uint_types.empty() ? nullptr : uint_types.data(),
            uint_tags.size(),
            rational_tags.empty() ? nullptr : rational_tags.data(),
            rational_numerators.empty() ? nullptr : rational_numerators.data(),
            rational_denominators.empty() ? nullptr : rational_denominators.data(),
            rational_tags.size(),
            rational_array_tags.empty() ? nullptr : rational_array_tags.data(),
            rational_array_numerators.empty() ? nullptr : rational_array_numerators.data(),
            rational_array_denominators.empty() ? nullptr : rational_array_denominators.data(),
            rational_array_numerators.size(),
            rational_array_offsets.empty() ? nullptr : rational_array_offsets.data(),
            rational_array_counts.empty() ? nullptr : rational_array_counts.data(),
            rational_array_tags.size(),
            short_array_tags.empty() ? nullptr : short_array_tags.data(),
            short_array_values.empty() ? nullptr : short_array_values.data(),
            short_array_values.size(),
            short_array_offsets.empty() ? nullptr : short_array_offsets.data(),
            short_array_counts.empty() ? nullptr : short_array_counts.data(),
            short_array_tags.size(),
            byte_array_tags.empty() ? nullptr : byte_array_tags.data(),
            byte_array_values.empty() ? nullptr : byte_array_values.data(),
            byte_array_values.size(),
            byte_array_offsets.empty() ? nullptr : byte_array_offsets.data(),
            byte_array_counts.empty() ? nullptr : byte_array_counts.data(),
            byte_array_tags.size(),
            uint_array_tags.empty() ? nullptr : uint_array_tags.data(),
            uint_array_values.empty() ? nullptr : uint_array_values.data(),
            uint_array_values.size(),
            uint_array_offsets.empty() ? nullptr : uint_array_offsets.data(),
            uint_array_counts.empty() ? nullptr : uint_array_counts.data(),
            uint_array_tags.size(),
            signed_rational_tags.empty() ? nullptr : signed_rational_tags.data(),
            signed_rational_numerators.empty() ? nullptr : signed_rational_numerators.data(),
            signed_rational_denominators.empty() ? nullptr : signed_rational_denominators.data(),
            signed_rational_tags.size(),
            undefined_tags.empty() ? nullptr : undefined_tags.data(),
            undefined_values.empty() ? nullptr : undefined_values.data(),
            undefined_values.size(),
            undefined_offsets.empty() ? nullptr : undefined_offsets.data(),
            undefined_counts.empty() ? nullptr : undefined_counts.data(),
            undefined_tags.size());
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_bigtiff_image_with_compression(
    const PillowCImage* image,
    const char* path,
    std::uint16_t compression)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (compression != TIFF_COMPRESSION_NONE &&
        compression != TIFF_COMPRESSION_PACKBITS &&
        compression != TIFF_COMPRESSION_LZW &&
        compression != TIFF_COMPRESSION_ADOBE_DEFLATE) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    const int validate_status = validate_tiff_save_image(image);
    if (validate_status != PILLOW_C_OK) {
        return validate_status;
    }
    int channels = 0;
    std::uint64_t photometric = 0u;
    bool has_extra_samples = false;
    switch (image->mode) {
    case PILLOW_C_MODE_L:
        channels = 1;
        photometric = 1u;
        break;
    case PILLOW_C_MODE_RGB:
        channels = 3;
        photometric = 2u;
        break;
    case PILLOW_C_MODE_RGBA:
        channels = 4;
        photometric = 2u;
        has_extra_samples = true;
        break;
    case PILLOW_C_MODE_LA:
        channels = 2;
        photometric = 1u;
        has_extra_samples = true;
        break;
    default:
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->channels != channels) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const auto append_le64 = [](std::vector<std::uint8_t>& out, std::uint64_t value) {
        for (int byte_index = 0; byte_index < 8; ++byte_index) {
            out.push_back(static_cast<std::uint8_t>((value >> (byte_index * 8)) & 0xFFu));
        }
    };

    const std::size_t entry_count = 9u + (channels > 1 ? 1u : 0u) + (has_extra_samples ? 1u : 0u);
    const std::uint64_t pixel_offset = 16u + 8u + static_cast<std::uint64_t>(entry_count) * 20u + 8u;
    std::uint64_t bits_value = 0u;
    for (int channel = 0; channel < channels; ++channel) {
        bits_value |= static_cast<std::uint64_t>(8u) << (channel * 16);
    }

    try {
        std::vector<std::uint8_t> strip_bytes;
        const std::uint8_t* source_pixels = image->pixels.data();
        const std::size_t source_row_stride = static_cast<std::size_t>(image->width) * static_cast<std::size_t>(channels);
        if (compression == TIFF_COMPRESSION_PACKBITS) {
            strip_bytes = tiff_packbits_encode_pixels(source_pixels, source_row_stride, image->height);
        } else if (compression == TIFF_COMPRESSION_LZW) {
            if (!tiff_lzw_encode_pixels(source_pixels, source_row_stride, image->height, &strip_bytes)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        } else if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
            if (!tiff_deflate_encode_pixels(source_pixels, source_row_stride, image->height, &strip_bytes)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        } else {
            strip_bytes.assign(image->pixels.begin(), image->pixels.end());
        }
        const std::uint64_t byte_count = static_cast<std::uint64_t>(strip_bytes.size());

        std::vector<std::uint8_t> out;
        out.reserve(static_cast<std::size_t>(pixel_offset) + strip_bytes.size());
        out.push_back('I');
        out.push_back('I');
        append_le16(out, 43u);
        append_le16(out, 8u);
        append_le16(out, 0u);
        append_le64(out, 16u);
        append_le64(out, static_cast<std::uint64_t>(entry_count));
        const auto append_entry = [&out, &append_le64](std::uint16_t tag, std::uint16_t type, std::uint64_t count, std::uint64_t value) {
            append_le16(out, tag);
            append_le16(out, type);
            append_le64(out, count);
            append_le64(out, value);
        };
        append_entry(256u, 4u, 1u, static_cast<std::uint64_t>(image->width));
        append_entry(257u, 4u, 1u, static_cast<std::uint64_t>(image->height));
        append_entry(258u, 3u, static_cast<std::uint64_t>(channels), bits_value);
        append_entry(259u, 3u, 1u, compression);
        append_entry(262u, 3u, 1u, photometric);
        append_entry(273u, 4u, 1u, pixel_offset);
        if (channels > 1) {
            append_entry(277u, 3u, 1u, static_cast<std::uint64_t>(channels));
        }
        append_entry(278u, 4u, 1u, static_cast<std::uint64_t>(image->height));
        append_entry(279u, 4u, 1u, byte_count);
        append_entry(284u, 3u, 1u, 1u);
        if (has_extra_samples) {
            append_entry(338u, 3u, 1u, 2u);
        }
        append_le64(out, 0u);
        out.insert(out.end(), strip_bytes.begin(), strip_bytes.end());
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_bigtiff_frames_image_metadata_with_compression(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    std::uint16_t compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* ascii_tags,
    const char* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count == 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (compression != TIFF_COMPRESSION_NONE &&
        compression != TIFF_COMPRESSION_PACKBITS &&
        compression != TIFF_COMPRESSION_LZW &&
        compression != TIFF_COMPRESSION_ADOBE_DEFLATE) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (ascii_count > 0u && (!ascii_tags || !ascii_values || !ascii_sizes)) {
        return PILLOW_C_NULL_POINTER;
    }
    for (std::size_t index = 0u; index < ascii_count; ++index) {
        if (!ascii_values[index] || ascii_sizes[index] == 0u) {
            return PILLOW_C_NULL_POINTER;
        }
    }
    if ((icc_profile_size > 0u && !icc_profile) || (xmp_size > 0u && !xmp)) {
        return PILLOW_C_NULL_POINTER;
    }
    std::uint32_t x_resolution_numerator = 0u;
    std::uint32_t y_resolution_numerator = 0u;
    const int dpi_status = tiff_dpi_to_rational(
        has_dpi != 0,
        dpi_x,
        dpi_y,
        &x_resolution_numerator,
        &y_resolution_numerator);
    if (dpi_status != PILLOW_C_OK) {
        return dpi_status;
    }
    const bool has_xmp = xmp_size > 0u;
    const bool has_icc_profile = icc_profile_size > 0u;
    const bool has_dpi_tag = has_dpi != 0;

    struct BigTiffFrame
    {
        const PillowCImage* image;
        std::uint64_t photometric;
        bool has_extra_samples;
        std::uint16_t bits_per_sample;
        bool has_sample_format;
        std::uint16_t sample_format;
        bool is_i16;
        std::uint64_t bits_value;
        std::size_t entry_count;
        std::size_t ifd_bytes;
        std::vector<std::uint8_t> strip_bytes;
        std::vector<std::uint64_t> ascii_blob_offsets;
        std::uint64_t xmp_blob_offset;
        std::uint64_t icc_blob_offset;
    };
    std::vector<BigTiffFrame> frames;
    try {
        frames.reserve(image_count);
        for (std::size_t index = 0u; index < image_count; ++index) {
            const PillowCImage* image = images[index];
            const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
            if (refresh_status != PILLOW_C_OK) {
                return refresh_status;
            }
            const int validate_status = validate_tiff_save_image(image);
            if (validate_status != PILLOW_C_OK) {
                return validate_status;
            }
            if (image->mode != images[0]->mode || image->channels != images[0]->channels) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            BigTiffFrame frame;
            frame.image = image;
            frame.photometric = 0u;
            frame.has_extra_samples = false;
            frame.bits_per_sample = 8u;
            frame.has_sample_format = false;
            frame.sample_format = 1u;
            frame.is_i16 = false;
            frame.bits_value = 0u;
            frame.entry_count = 0u;
            frame.ifd_bytes = 0u;
            frame.ascii_blob_offsets.assign(ascii_count, 0u);
            frame.xmp_blob_offset = 0u;
            frame.icc_blob_offset = 0u;
            int channels = 0;
            switch (image->mode) {
            case PILLOW_C_MODE_L:
                channels = 1;
                frame.photometric = 1u;
                break;
            case PILLOW_C_MODE_RGB:
                channels = 3;
                frame.photometric = 2u;
                break;
            case PILLOW_C_MODE_RGBA:
                channels = 4;
                frame.photometric = 2u;
                frame.has_extra_samples = true;
                break;
            case PILLOW_C_MODE_LA:
                channels = 2;
                frame.photometric = 1u;
                frame.has_extra_samples = true;
                break;
            case PILLOW_C_MODE_I16:
                channels = 2;
                frame.photometric = 1u;
                frame.bits_per_sample = 16u;
                frame.is_i16 = true;
                break;
            case PILLOW_C_MODE_I16B:
                channels = 2;
                frame.photometric = 1u;
                frame.bits_per_sample = 16u;
                frame.is_i16 = true;
                break;
            case PILLOW_C_MODE_I:
                channels = 4;
                frame.photometric = 1u;
                frame.bits_per_sample = 32u;
                frame.has_sample_format = true;
                frame.sample_format = 2u;
                break;
            case PILLOW_C_MODE_F:
                channels = 4;
                frame.photometric = 1u;
                frame.bits_per_sample = 32u;
                frame.has_sample_format = true;
                frame.sample_format = 3u;
                break;
            case PILLOW_C_MODE_CMYK:
                channels = 4;
                frame.photometric = 5u;
                break;
            default:
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (image->channels != channels) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if ((frame.has_sample_format || frame.is_i16 || image->mode == PILLOW_C_MODE_CMYK) &&
                compression != TIFF_COMPRESSION_NONE) {
                // Pillow falls back to classic TIFF when big_tiff combines with
                // compression; numeric BigTIFF strips stay uncompressed.
                return PILLOW_C_INVALID_ARGUMENT;
            }
            for (int channel = 0; channel < channels; ++channel) {
                frame.bits_value |= static_cast<std::uint64_t>(frame.bits_per_sample) << (channel * 16);
            }
            const bool has_samples_per_pixel =
                channels > 1 && !frame.is_i16 && !frame.has_sample_format;
            frame.entry_count = 9u + (has_samples_per_pixel ? 1u : 0u) +
                (frame.has_extra_samples ? 1u : 0u) + (frame.has_sample_format ? 1u : 0u) +
                (has_dpi_tag ? 3u : 0u) + (has_icc_profile ? 1u : 0u) +
                (has_xmp ? 1u : 0u) + ascii_count;
            frame.ifd_bytes = 8u + frame.entry_count * 20u + 8u;

            std::vector<std::uint8_t> swapped_pixels;
            const std::uint8_t* source_pixels = image->pixels.data();
            if (image->mode == PILLOW_C_MODE_I16B) {
                swapped_pixels = tiff_i16b_to_i16_pixels(image);
                source_pixels = swapped_pixels.data();
            }
            const std::size_t source_row_stride =
                static_cast<std::size_t>(image->width) * static_cast<std::size_t>(channels);
            if (compression == TIFF_COMPRESSION_PACKBITS) {
                frame.strip_bytes = tiff_packbits_encode_pixels(source_pixels, source_row_stride, image->height);
            } else if (compression == TIFF_COMPRESSION_LZW) {
                if (!tiff_lzw_encode_pixels(source_pixels, source_row_stride, image->height, &frame.strip_bytes)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            } else if (compression == TIFF_COMPRESSION_ADOBE_DEFLATE) {
                if (!tiff_deflate_encode_pixels(source_pixels, source_row_stride, image->height, &frame.strip_bytes)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            } else {
                frame.strip_bytes.assign(source_pixels, source_pixels + image->pixels.size());
            }
            frames.push_back(std::move(frame));
        }

        std::uint64_t cursor = 16u;
        std::vector<std::uint64_t> ifd_offsets(image_count, 0u);
        std::vector<std::uint64_t> strip_offsets(image_count, 0u);
        for (std::size_t index = 0u; index < image_count; ++index) {
            ifd_offsets[index] = cursor;
            std::uint64_t blob_cursor = cursor + static_cast<std::uint64_t>(frames[index].ifd_bytes);
            BigTiffFrame& frame = frames[index];
            for (std::size_t ascii_index = 0u; ascii_index < ascii_count; ++ascii_index) {
                if (ascii_sizes[ascii_index] > 8u) {
                    blob_cursor = align_tiff_offset(blob_cursor);
                    frame.ascii_blob_offsets[ascii_index] = blob_cursor;
                    blob_cursor += static_cast<std::uint64_t>(ascii_sizes[ascii_index]);
                }
            }
            if (has_xmp && xmp_size > 8u) {
                blob_cursor = align_tiff_offset(blob_cursor);
                frame.xmp_blob_offset = blob_cursor;
                blob_cursor += static_cast<std::uint64_t>(xmp_size);
            }
            if (has_icc_profile && icc_profile_size > 8u) {
                blob_cursor = align_tiff_offset(blob_cursor);
                frame.icc_blob_offset = blob_cursor;
                blob_cursor += static_cast<std::uint64_t>(icc_profile_size);
            }
            strip_offsets[index] = blob_cursor;
            cursor = blob_cursor + static_cast<std::uint64_t>(frames[index].strip_bytes.size());
        }

        const auto append_le64 = [](std::vector<std::uint8_t>& out, std::uint64_t value) {
            for (int byte_index = 0; byte_index < 8; ++byte_index) {
                out.push_back(static_cast<std::uint8_t>((value >> (byte_index * 8)) & 0xFFu));
            }
        };
        std::vector<std::uint8_t> out;
        out.reserve(static_cast<std::size_t>(cursor));
        out.push_back('I');
        out.push_back('I');
        append_le16(out, 43u);
        append_le16(out, 8u);
        append_le16(out, 0u);
        append_le64(out, 16u);
        const auto append_entry = [&out, &append_le64](std::uint16_t tag, std::uint16_t type, std::uint64_t count, std::uint64_t value) {
            append_le16(out, tag);
            append_le16(out, type);
            append_le64(out, count);
            append_le64(out, value);
        };
        for (std::size_t index = 0u; index < image_count; ++index) {
            const BigTiffFrame& frame = frames[index];
            const PillowCImage* image = frame.image;
            const auto append_ascii_entry = [&](int wanted_tag) {
                for (std::size_t ascii_index = 0u; ascii_index < ascii_count; ++ascii_index) {
                    if (ascii_tags[ascii_index] != wanted_tag) {
                        continue;
                    }
                    const std::size_t size = ascii_sizes[ascii_index];
                    const char* value_bytes = ascii_values[ascii_index];
                    std::uint64_t value = frame.ascii_blob_offsets[ascii_index];
                    if (size <= 8u) {
                        value = 0u;
                        for (std::size_t byte_index = 0u; byte_index < size; ++byte_index) {
                            value |= static_cast<std::uint64_t>(
                                static_cast<std::uint8_t>(value_bytes[byte_index])) << (byte_index * 8u);
                        }
                    }
                    append_entry(static_cast<std::uint16_t>(wanted_tag), 2u, static_cast<std::uint64_t>(size), value);
                    return;
                }
            };
            const auto blob_entry_value = [](const std::uint8_t* bytes, std::size_t size, std::uint64_t offset) -> std::uint64_t {
                if (size > 8u) {
                    return offset;
                }
                std::uint64_t value = 0u;
                for (std::size_t byte_index = 0u; byte_index < size; ++byte_index) {
                    value |= static_cast<std::uint64_t>(bytes[byte_index]) << (byte_index * 8u);
                }
                return value;
            };
            append_le64(out, static_cast<std::uint64_t>(frame.entry_count));
            append_entry(256u, 4u, 1u, static_cast<std::uint64_t>(image->width));
            append_entry(257u, 4u, 1u, static_cast<std::uint64_t>(image->height));
            append_entry(
                258u,
                3u,
                (frame.is_i16 || frame.has_sample_format)
                    ? 1u
                    : static_cast<std::uint64_t>(image->channels),
                frame.bits_value);
            append_entry(259u, 3u, 1u, compression);
            append_entry(262u, 3u, 1u, frame.photometric);
            append_ascii_entry(270);
            append_entry(273u, 4u, 1u, strip_offsets[index]);
            if (image->channels > 1 && !frame.is_i16 && !frame.has_sample_format) {
                append_entry(277u, 3u, 1u, static_cast<std::uint64_t>(image->channels));
            }
            append_entry(278u, 4u, 1u, static_cast<std::uint64_t>(image->height));
            append_entry(279u, 4u, 1u, static_cast<std::uint64_t>(frame.strip_bytes.size()));
            if (has_dpi_tag) {
                append_entry(
                    282u,
                    5u,
                    1u,
                    static_cast<std::uint64_t>(x_resolution_numerator) | (std::uint64_t{1u} << 32u));
                append_entry(
                    283u,
                    5u,
                    1u,
                    static_cast<std::uint64_t>(y_resolution_numerator) | (std::uint64_t{1u} << 32u));
            }
            append_entry(284u, 3u, 1u, 1u);
            if (has_dpi_tag) {
                append_entry(296u, 3u, 1u, 2u);
            }
            append_ascii_entry(315);
            if (frame.has_extra_samples) {
                append_entry(338u, 3u, 1u, 2u);
            }
            if (frame.has_sample_format) {
                append_entry(339u, 3u, 1u, static_cast<std::uint64_t>(frame.sample_format));
            }
            if (has_xmp) {
                append_entry(
                    700u,
                    1u,
                    static_cast<std::uint64_t>(xmp_size),
                    blob_entry_value(xmp, xmp_size, frame.xmp_blob_offset));
            }
            if (has_icc_profile) {
                append_entry(
                    34675u,
                    7u,
                    static_cast<std::uint64_t>(icc_profile_size),
                    blob_entry_value(icc_profile, icc_profile_size, frame.icc_blob_offset));
            }
            append_le64(out, index + 1u < image_count ? ifd_offsets[index + 1u] : 0u);
            for (std::size_t ascii_index = 0u; ascii_index < ascii_count; ++ascii_index) {
                if (ascii_sizes[ascii_index] > 8u) {
                    if (out.size() < static_cast<std::size_t>(frame.ascii_blob_offsets[ascii_index])) {
                        out.resize(static_cast<std::size_t>(frame.ascii_blob_offsets[ascii_index]), 0);
                    }
                    out.insert(
                        out.end(),
                        ascii_values[ascii_index],
                        ascii_values[ascii_index] + ascii_sizes[ascii_index]);
                }
            }
            if (has_xmp && xmp_size > 8u) {
                if (out.size() < static_cast<std::size_t>(frame.xmp_blob_offset)) {
                    out.resize(static_cast<std::size_t>(frame.xmp_blob_offset), 0);
                }
                out.insert(out.end(), xmp, xmp + xmp_size);
            }
            if (has_icc_profile && icc_profile_size > 8u) {
                if (out.size() < static_cast<std::size_t>(frame.icc_blob_offset)) {
                    out.resize(static_cast<std::size_t>(frame.icc_blob_offset), 0);
                }
                out.insert(out.end(), icc_profile, icc_profile + icc_profile_size);
            }
            if (out.size() < static_cast<std::size_t>(strip_offsets[index])) {
                out.resize(static_cast<std::size_t>(strip_offsets[index]), 0);
            }
            out.insert(out.end(), frame.strip_bytes.begin(), frame.strip_bytes.end());
        }
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tiff_bigtiff_frames_image_with_compression(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    std::uint16_t compression)
{
    return save_tiff_bigtiff_frames_image_metadata_with_compression(
        images,
        image_count,
        path,
        0,
        0.0,
        0.0,
        compression,
        nullptr,
        0u,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        0u);
}

int save_tiff_bigtiff_image(const PillowCImage* image, const char* path)
{
    const PillowCImage* images[] = { image };
    return save_tiff_bigtiff_frames_image_with_compression(images, 1u, path, TIFF_COMPRESSION_NONE);
}

} // namespace

int pillow_c_tiff_parse_orientation(const std::uint8_t* tiff, std::size_t tiff_size)
{
    return parse_tiff_orientation(tiff, tiff_size);
}

std::uint16_t pillow_c_tiff_read16(const std::uint8_t* data, bool little_endian)
{
    return read_tiff16(data, little_endian);
}

std::uint32_t pillow_c_tiff_read32(const std::uint8_t* data, bool little_endian)
{
    return read_tiff32(data, little_endian);
}

std::uint64_t pillow_c_tiff_read64(const std::uint8_t* data, bool little_endian)
{
    return read_tiff64(data, little_endian);
}

bool pillow_c_tiff_read_signed_rational_array_entry_value(
    const std::uint8_t* tiff,
    std::size_t tiff_size,
    bool little_endian,
    const std::uint8_t* entry,
    std::vector<std::int32_t>* out_numerators,
    std::vector<std::int32_t>* out_denominators)
{
    return read_tiff_signed_rational_array_entry_value(
        tiff,
        tiff_size,
        little_endian,
        entry,
        out_numerators,
        out_denominators);
}

bool count_tiff_ifds(const std::uint8_t* tiff, std::size_t tiff_size, int* out_count)
{
    if (!tiff || !out_count || tiff_size < 8u) {
        return false;
    }
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || read_tiff16(tiff + 2u, little_endian) != 42u) {
        return false;
    }

    std::uint32_t ifd_offset = read_tiff32(tiff + 4u, little_endian);
    std::unordered_map<std::uint32_t, bool> visited;
    int count = 0;
    while (ifd_offset != 0u) {
        if (!visited.emplace(ifd_offset, true).second ||
            ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
            return false;
        }
        const std::uint8_t* ifd = tiff + ifd_offset;
        const std::uint16_t entry_count = read_tiff16(ifd, little_endian);
        const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
        if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
            return false;
        }
        if (count == std::numeric_limits<int>::max()) {
            return false;
        }
        ++count;
        const std::size_t next_offset_location =
            entries_offset + static_cast<std::size_t>(entry_count) * 12u;
        if (next_offset_location > tiff_size || tiff_size - next_offset_location < 4u) {
            return false;
        }
        ifd_offset = read_tiff32(tiff + next_offset_location, little_endian);
    }
    *out_count = count;
    return count > 0;
}

bool count_tiff_bigtiff_ifds(const std::uint8_t* tiff, std::size_t tiff_size, int* out_count)
{
    if (!tiff || !out_count || tiff_size < 16u) {
        return false;
    }

    bool little_endian = false;
    std::uint64_t first_ifd_offset = 0u;
    if (!parse_tiff_bigtiff_header(
            tiff,
            tiff_size,
            &little_endian,
            &first_ifd_offset)) {
        return false;
    }
    const std::uint64_t tiff_size_u64 = static_cast<std::uint64_t>(tiff_size);
    std::uint64_t ifd_offset = first_ifd_offset;
    std::unordered_map<std::uint64_t, bool> visited;
    int count = 0;
    while (ifd_offset != 0u) {
        if (!visited.emplace(ifd_offset, true).second ||
            ifd_offset > tiff_size_u64 || tiff_size_u64 - ifd_offset < 8u) {
            return false;
        }

        const std::uint64_t entry_count = read_tiff64(
            tiff + static_cast<std::size_t>(ifd_offset),
            little_endian);
        const std::uint64_t entries_offset = ifd_offset + 8u;
        if (entries_offset > tiff_size_u64 ||
            entry_count > (tiff_size_u64 - entries_offset) / 20u) {
            return false;
        }
        if (count == std::numeric_limits<int>::max()) {
            return false;
        }
        ++count;

        const std::uint64_t next_offset_location = entries_offset + entry_count * 20u;
        if (next_offset_location > tiff_size_u64 ||
            tiff_size_u64 - next_offset_location < 8u) {
            return false;
        }
        ifd_offset = read_tiff64(
            tiff + static_cast<std::size_t>(next_offset_location),
            little_endian);
    }

    *out_count = count;
    return count > 0;
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tiff(
    const char* path,
    PillowCImage** out_image)
{
    return open_tiff_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tiff_frame(
    const char* path,
    int frame_index,
    PillowCImage** out_image)
{
    return open_tiff_frame_image(path, frame_index, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_frame_count_tiff(
    const char* path,
    int* out_count)
{
    if (!path || !out_count) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_count = 0;
    try {
        std::vector<std::uint8_t> tiff_bytes;
        if (read_binary_file(path, &tiff_bytes)) {
            bool is_tiff_bigtiff = false;
            bool is_tiff_bigtiff_shape = false;
            PillowCImage* bigtiff_image = nullptr;
            const int bigtiff_status = parse_tiff_bigtiff_tiled_image_for_ifd(
                tiff_bytes.data(),
                tiff_bytes.size(),
                0,
                &is_tiff_bigtiff,
                &is_tiff_bigtiff_shape,
                &bigtiff_image);
            delete bigtiff_image;
            if (bigtiff_status != PILLOW_C_OK && !is_tiff_bigtiff) {
                return bigtiff_status;
            }
            if (is_tiff_bigtiff) {
                if (bigtiff_status != PILLOW_C_OK || !is_tiff_bigtiff_shape) {
                    bool strip_is_bigtiff = false;
                    bool strip_recognized = false;
                    PillowCImage* strip_image = nullptr;
                    const int strip_status = parse_tiff_bigtiff_strip_image_for_ifd(
                        tiff_bytes.data(),
                        tiff_bytes.size(),
                        0,
                        &strip_is_bigtiff,
                        &strip_recognized,
                        &strip_image);
                    delete strip_image;
                    if (strip_status != PILLOW_C_OK) {
                        return strip_status;
                    }
                    if (!strip_is_bigtiff || !strip_recognized) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                }
                if (!count_tiff_bigtiff_ifds(tiff_bytes.data(), tiff_bytes.size(), out_count)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                return PILLOW_C_OK;
            }
            bool is_tiff_chunky_shape = false;
            bool is_tiff_tiled_storage = false;
            PillowCImage* tile_shape_image = nullptr;
            const int status = parse_tiff_chunky_image_for_ifd(
                tiff_bytes.data(),
                tiff_bytes.size(),
                0,
                &is_tiff_chunky_shape,
                &is_tiff_tiled_storage,
                &tile_shape_image);
            delete tile_shape_image;
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (is_tiff_chunky_shape && is_tiff_tiled_storage) {
                if (!count_tiff_ifds(tiff_bytes.data(), tiff_bytes.size(), out_count)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                return PILLOW_C_OK;
            }
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    return wic_container_frame_count(path, GUID_ContainerFormatTiff, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff(
    const PillowCImage* image,
    const char* path)
{
    return save_tiff_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_bigtiff(
    const PillowCImage* image,
    const char* path)
{
    return save_tiff_bigtiff_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_bigtiff_compression_options(
    const PillowCImage* image,
    const char* path,
    int compression)
{
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_bigtiff_image_with_compression(image, path, normalized_compression);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_bigtiff_frames_compression_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int compression)
{
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_bigtiff_frames_image_with_compression(images, image_count, path, normalized_compression);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_bigtiff_frames_metadata_ascii_entries_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* ascii_tags,
    const char* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count)
{
    std::uint16_t normalized_compression = TIFF_COMPRESSION_NONE;
    const int status = normalize_tiff_save_compression(compression, &normalized_compression);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return save_tiff_bigtiff_frames_image_metadata_with_compression(
        images,
        image_count,
        path,
        has_dpi,
        dpi_x,
        dpi_y,
        normalized_compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        ascii_tags,
        ascii_values,
        ascii_sizes,
        ascii_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_options(
    const PillowCImage* image,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y)
{
    return save_tiff_image_with_options(image, path, has_dpi != 0, dpi_x, dpi_y);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_compression_options(
    const PillowCImage* image,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression)
{
    return save_tiff_image_with_compression_options(image, path, has_dpi != 0, dpi_x, dpi_y, compression);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path)
{
    return save_tiff_frames_image(images, image_count, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames_compression_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int compression)
{
    return save_tiff_frames_image_with_compression_options(images, image_count, path, compression);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression)
{
    return save_tiff_frames_image_with_save_options(
        images,
        image_count,
        path,
        has_dpi != 0,
        dpi_x,
        dpi_y,
        compression);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames_metadata_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size)
{
    return save_tiff_frames_image_with_metadata_options(
        images,
        image_count,
        path,
        has_dpi != 0,
        dpi_x,
        dpi_y,
        compression,
        icc_profile,
        icc_profile_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames_metadata_ex_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size)
{
    return save_tiff_frames_image_with_metadata_ex_options(
        images,
        image_count,
        path,
        has_dpi != 0,
        dpi_x,
        dpi_y,
        compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames_metadata_ascii_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    int ascii_tag,
    const std::uint8_t* ascii_value,
    std::size_t ascii_size)
{
    return save_tiff_frames_image_with_metadata_ascii_options(
        images,
        image_count,
        path,
        has_dpi != 0,
        dpi_x,
        dpi_y,
        compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        ascii_tag,
        ascii_value,
        ascii_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tiff_frames_metadata_ascii_entries_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    int has_dpi,
    double dpi_x,
    double dpi_y,
    int compression,
    const std::uint8_t* icc_profile,
    std::size_t icc_profile_size,
    const std::uint8_t* xmp,
    std::size_t xmp_size,
    const int* ascii_tags,
    const std::uint8_t* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count)
{
    return save_tiff_frames_image_with_metadata_ascii_entries_options(
        images,
        image_count,
        path,
        has_dpi != 0,
        dpi_x,
        dpi_y,
        compression,
        icc_profile,
        icc_profile_size,
        xmp,
        xmp_size,
        ascii_tags,
        ascii_values,
        ascii_sizes,
        ascii_count);
}


extern "C" __declspec(dllexport) int pillow_c_image_patch_tiff_exif_entries(
    const char* path,
    const int* ascii_tags,
    const std::uint8_t* const* ascii_values,
    const std::size_t* ascii_sizes,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    const int* rational_array_tags,
    const std::uint32_t* rational_array_numerators,
    const std::uint32_t* rational_array_denominators,
    std::size_t rational_array_value_count,
    const std::size_t* rational_array_offsets,
    const std::size_t* rational_array_counts,
    std::size_t rational_array_count,
    const int* short_array_tags,
    const std::uint32_t* short_array_values,
    std::size_t short_array_value_count,
    const std::size_t* short_array_offsets,
    const std::size_t* short_array_counts,
    std::size_t short_array_count,
    const int* byte_array_tags,
    const std::uint8_t* byte_array_values,
    std::size_t byte_array_value_count,
    const std::size_t* byte_array_offsets,
    const std::size_t* byte_array_counts,
    std::size_t byte_array_count,
    const int* uint_array_tags,
    const std::uint32_t* uint_array_values,
    std::size_t uint_array_value_count,
    const std::size_t* uint_array_offsets,
    const std::size_t* uint_array_counts,
    std::size_t uint_array_count,
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count)
{
    return patch_tiff_ifd0_exif_entries(
        path,
        ascii_tags,
        ascii_values,
        ascii_sizes,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
        rational_array_tags,
        rational_array_numerators,
        rational_array_denominators,
        rational_array_value_count,
        rational_array_offsets,
        rational_array_counts,
        rational_array_count,
        short_array_tags,
        short_array_values,
        short_array_value_count,
        short_array_offsets,
        short_array_counts,
        short_array_count,
        byte_array_tags,
        byte_array_values,
        byte_array_value_count,
        byte_array_offsets,
        byte_array_counts,
        byte_array_count,
        uint_array_tags,
        uint_array_values,
        uint_array_value_count,
        uint_array_offsets,
        uint_array_counts,
        uint_array_count,
        signed_rational_tags,
        signed_rational_numerators,
        signed_rational_denominators,
        signed_rational_count,
        undefined_tags,
        undefined_values,
        undefined_value_count,
        undefined_offsets,
        undefined_counts,
        undefined_count);
}


extern "C" __declspec(dllexport) int pillow_c_image_patch_tiff_exif_bytes(
    const char* path,
    const std::uint8_t* exif_bytes,
    std::size_t exif_size)
{
    return patch_tiff_ifd0_exif_blob(path, exif_bytes, exif_size);
}


extern "C" __declspec(dllexport) int pillow_c_image_metadata_tiff_exif(
    const PillowCImage* image,
    int* out_has_exif,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    return copy_metadata_blob(image->tiff_exif, out_has_exif, out_exif, out_exif_size, out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_tiff_icc_profile(
    const PillowCImage* image,
    int* out_has_profile,
    std::uint8_t* out_profile,
    std::size_t out_profile_size,
    std::size_t* out_profile_required)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    return copy_metadata_blob(image->tiff_icc_profile, out_has_profile, out_profile, out_profile_size, out_profile_required);
}
