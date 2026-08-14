#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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
constexpr int PILLOW_C_LEGACY_RESAMPLE_LANCZOS = 1;
constexpr int PILLOW_C_LEGACY_RESAMPLE_NEAREST = 0;
constexpr int PILLOW_C_LEGACY_RESAMPLE_BICUBIC = 3;

bool ppm_is_space(std::uint8_t value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

bool ppm_skip_space_and_comments(const std::vector<std::uint8_t>& data, std::size_t* offset)
{
    if (!offset) {
        return false;
    }
    while (*offset < data.size()) {
        if (ppm_is_space(data[*offset])) {
            ++(*offset);
            continue;
        }
        if (data[*offset] == '#') {
            while (*offset < data.size() && data[*offset] != '\n' && data[*offset] != '\r') {
                ++(*offset);
            }
            continue;
        }
        break;
    }
    return *offset < data.size();
}

bool ppm_read_positive_int(const std::vector<std::uint8_t>& data, std::size_t* offset, int* out)
{
    if (!offset || !out || !ppm_skip_space_and_comments(data, offset)) {
        return false;
    }
    std::uint64_t value = 0;
    bool has_digit = false;
    while (*offset < data.size()) {
        const std::uint8_t ch = data[*offset];
        if (ch < '0' || ch > '9') {
            break;
        }
        has_digit = true;
        value = value * 10u + static_cast<std::uint64_t>(ch - '0');
        if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        ++(*offset);
    }
    if (!has_digit || value == 0) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool ppm_read_nonnegative_int(const std::vector<std::uint8_t>& data, std::size_t* offset, int* out)
{
    if (!offset || !out || !ppm_skip_space_and_comments(data, offset)) {
        return false;
    }
    std::uint64_t value = 0;
    bool has_digit = false;
    while (*offset < data.size()) {
        const std::uint8_t ch = data[*offset];
        if (ch < '0' || ch > '9') {
            break;
        }
        has_digit = true;
        value = value * 10u + static_cast<std::uint64_t>(ch - '0');
        if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        ++(*offset);
    }
    if (!has_digit) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

std::uint8_t ppm_scale_sample_to_u8(int value, int max_value)
{
    if (value <= 0) {
        return 0;
    }
    if (max_value <= 0) {
        return 0;
    }
    if (max_value == 255) {
        return static_cast<std::uint8_t>(std::min(value, 255));
    }

    const std::uint64_t numerator = static_cast<std::uint64_t>(value) * 255u;
    const std::uint64_t denominator = static_cast<std::uint64_t>(max_value);
    std::uint64_t quotient = numerator / denominator;
    const std::uint64_t remainder = numerator % denominator;
    const std::uint64_t twice_remainder = remainder * 2u;
    if (twice_remainder > denominator || (twice_remainder == denominator && (quotient & 1u) != 0u)) {
        ++quotient;
    }
    if (quotient > 255u) {
        return 255;
    }
    return static_cast<std::uint8_t>(quotient);
}

std::uint32_t ppm_scale_sample_to_u16(int value, int max_value)
{
    if (value <= 0) {
        return 0;
    }
    if (max_value <= 0) {
        return 0;
    }
    if (max_value == 65535) {
        return static_cast<std::uint32_t>(std::min(value, 65535));
    }

    const std::uint64_t numerator = static_cast<std::uint64_t>(value) * 65535u;
    const std::uint64_t denominator = static_cast<std::uint64_t>(max_value);
    std::uint64_t quotient = numerator / denominator;
    const std::uint64_t remainder = numerator % denominator;
    const std::uint64_t twice_remainder = remainder * 2u;
    if (twice_remainder > denominator || (twice_remainder == denominator && (quotient & 1u) != 0u)) {
        ++quotient;
    }
    if (quotient > 65535u) {
        return 65535u;
    }
    return static_cast<std::uint32_t>(quotient);
}

void write_le32_pixel(std::uint8_t* dst, std::uint32_t value)
{
    dst[0] = static_cast<std::uint8_t>(value & 0xFFu);
    dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    dst[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    dst[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

int ppm_data_offset_after_header(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t* out_offset)
{
    if (!out_offset || offset >= data.size() || !ppm_is_space(data[offset])) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (data[offset] == '\r' && offset + 1u < data.size() && data[offset + 1u] == '\n') {
        offset += 2u;
    } else {
        ++offset;
    }
    if (offset >= data.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    *out_offset = offset;
    return PILLOW_C_OK;
}

void append_ascii_int(std::vector<std::uint8_t>& out, int value)
{
    char buf[32] = {};
    const int written = std::snprintf(buf, sizeof(buf), "%d", value);
    for (int i = 0; i < written; ++i) {
        out.push_back(static_cast<std::uint8_t>(buf[i]));
    }
}

int open_ppm_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data) || data.size() < 3u || data[0] != 'P' ||
            (data[1] < '1' || data[1] > '6')) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const bool is_plain = data[1] == '1' || data[1] == '2' || data[1] == '3';
        const bool is_pbm = data[1] == '1' || data[1] == '4';
        const bool is_gray = data[1] == '2' || data[1] == '5';
        int mode = is_pbm ? PILLOW_C_MODE_1 : (is_gray ? PILLOW_C_MODE_L : PILLOW_C_MODE_RGB);
        std::size_t offset = 2u;
        int width = 0;
        int height = 0;
        int max_value = 0;
        if (!ppm_read_positive_int(data, &offset, &width) ||
            !ppm_read_positive_int(data, &offset, &height)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (!is_pbm) {
            if (!ppm_read_positive_int(data, &offset, &max_value)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (max_value >= 65536) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (is_gray && max_value > 255) {
                mode = PILLOW_C_MODE_I;
            }
        }

        const int channels = channels_for_mode(mode);
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        if (is_plain) {
            const std::size_t sample_count = mode == PILLOW_C_MODE_I ? size / 4u : size;
            for (std::size_t i = 0; i < sample_count; ++i) {
                int value = 0;
                if (!ppm_read_nonnegative_int(data, &offset, &value)) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                const int limit = is_pbm ? 1 : max_value;
                if (value > limit) {
                    delete image;
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                if (is_pbm) {
                    image->pixels[i] = value == 0 ? 255 : 0;
                } else if (mode == PILLOW_C_MODE_I) {
                    write_le32_pixel(image->pixels.data() + i * 4u, ppm_scale_sample_to_u16(value, max_value));
                } else {
                    image->pixels[i] = ppm_scale_sample_to_u8(value, max_value);
                }
            }
        } else if (is_pbm) {
            std::size_t pixel_offset = 0;
            int status = ppm_data_offset_after_header(data, offset, &pixel_offset);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
            const std::size_t packed_row_bytes = (static_cast<std::size_t>(width) + 7u) / 8u;
            const std::size_t packed_size = packed_row_bytes * static_cast<std::size_t>(height);
            if (packed_size > data.size() - pixel_offset) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = data.data() + pixel_offset + static_cast<std::size_t>(y) * packed_row_bytes;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
                for (int x = 0; x < width; ++x) {
                    const std::uint8_t packed = src_row[static_cast<std::size_t>(x) / 8u];
                    const int bit = (packed >> (7 - (x & 7))) & 1;
                    dst_row[x] = bit ? 0 : 255;
                }
            }
        } else {
            std::size_t pixel_offset = 0;
            int status = ppm_data_offset_after_header(data, offset, &pixel_offset);
            if (status != PILLOW_C_OK) {
                delete image;
                return status;
            }
            const int bytes_per_sample = max_value < 256 ? 1 : 2;
            const std::size_t sample_count = mode == PILLOW_C_MODE_I ? size / 4u : size;
            if (sample_count > (data.size() - pixel_offset) / static_cast<std::size_t>(bytes_per_sample)) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            if (max_value == 255 && mode != PILLOW_C_MODE_I) {
                std::memcpy(image->pixels.data(), data.data() + pixel_offset, size);
            } else {
                for (std::size_t i = 0; i < sample_count; ++i) {
                    int value = 0;
                    if (bytes_per_sample == 1) {
                        value = data[pixel_offset + i];
                    } else {
                        const std::size_t src_offset = pixel_offset + i * 2u;
                        value = (static_cast<int>(data[src_offset]) << 8) | static_cast<int>(data[src_offset + 1u]);
                    }
                    if (mode == PILLOW_C_MODE_I) {
                        write_le32_pixel(image->pixels.data() + i * 4u, ppm_scale_sample_to_u16(value, max_value));
                    } else {
                        image->pixels[i] = ppm_scale_sample_to_u8(value, max_value);
                    }
                }
            }
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ppm_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!((image->mode == PILLOW_C_MODE_1 && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_I && image->channels == 4) ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        if (image->mode == PILLOW_C_MODE_1) {
            std::size_t packed_row_bytes = 0;
            std::size_t packed_size = 0;
if (!pillow_c_checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            std::vector<std::uint8_t> out;
            out.reserve(32u + packed_size);
            out.push_back('P');
            out.push_back('4');
            out.push_back('\n');
            append_ascii_int(out, image->width);
            out.push_back(' ');
            append_ascii_int(out, image->height);
            out.push_back('\n');
            const std::size_t pixel_offset = out.size();
            out.resize(pixel_offset + packed_size, 0);
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                std::uint8_t* dst_row = out.data() + pixel_offset + static_cast<std::size_t>(y) * packed_row_bytes;
                for (int x = 0; x < image->width; ++x) {
                    if (src_row[x] == 0) {
                        dst_row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
                    }
                }
            }
            return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> out;
        const std::size_t pixel_count =
            static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
        out.reserve(32u + (image->mode == PILLOW_C_MODE_I ? pixel_count * 2u : image->pixels.size()));
        out.push_back('P');
        out.push_back(image->mode == PILLOW_C_MODE_RGB ? '6' : '5');
        out.push_back('\n');
        append_ascii_int(out, image->width);
        out.push_back(' ');
        append_ascii_int(out, image->height);
        out.push_back('\n');
        if (image->mode == PILLOW_C_MODE_I) {
            out.push_back('6');
            out.push_back('5');
            out.push_back('5');
            out.push_back('3');
            out.push_back('5');
        } else {
            out.push_back('2');
            out.push_back('5');
            out.push_back('5');
        }
        out.push_back('\n');
        if (image->mode == PILLOW_C_MODE_I) {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint8_t* src = image->pixels.data() + i * 4u;
const std::uint16_t value = pillow_c_clip_i32_to_u16(pillow_c_read_i32_le(src));
                out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
                out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            }
        } else {
            out.insert(out.end(), image->pixels.begin(), image->pixels.end());
        }
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

std::size_t qoi_hash_pixel(const std::uint8_t pixel[4])
{
    return (static_cast<std::size_t>(pixel[0]) * 3u +
            static_cast<std::size_t>(pixel[1]) * 5u +
            static_cast<std::size_t>(pixel[2]) * 7u +
            static_cast<std::size_t>(pixel[3]) * 11u) %
           64u;
}

int qoi_signed_delta(std::uint8_t left, std::uint8_t right)
{
    int result = (static_cast<int>(left) - static_cast<int>(right)) & 255;
    if (result >= 128) {
        result -= 256;
    }
    return result;
}

void qoi_write_run(std::vector<std::uint8_t>& out, int* run)
{
    out.push_back(static_cast<std::uint8_t>(0xc0u | static_cast<std::uint8_t>(*run - 1)));
    *run = 0;
}

int open_qoi_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 4u || std::memcmp(data.data(), "qoif", 4u) != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 14u) {
            return PILLOW_C_INVALID_LENGTH;
        }

        const std::uint32_t width_u32 = read_be32(data.data() + 4u);
        const std::uint32_t height_u32 = read_be32(data.data() + 8u);
        const int channels = data[12] == 3u ? 3 : (data[12] == 4u ? 4 : 0);
        if (width_u32 == 0 || height_u32 == 0 || width_u32 > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
            height_u32 > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) || channels == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const int width = static_cast<int>(width_u32);
        const int height = static_cast<int>(height_u32);
        const int mode = channels == 3 ? PILLOW_C_MODE_RGB : PILLOW_C_MODE_RGBA;
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};

        std::uint8_t index[64][4] = {};
        std::uint8_t previous[4] = {0, 0, 0, 255};
        std::size_t offset = 14u;
        const std::size_t pixel_count = size / static_cast<std::size_t>(channels);
        std::size_t out_pixel = 0;
        while (out_pixel < pixel_count) {
            if (offset >= data.size()) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }

            const std::uint8_t byte = data[offset++];
            std::uint8_t pixel[4] = {previous[0], previous[1], previous[2], previous[3]};
            if (byte == 0xfeu) {
                if (offset + 3u > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                pixel[0] = data[offset++];
                pixel[1] = data[offset++];
                pixel[2] = data[offset++];
            } else if (byte == 0xffu) {
                if (offset + 4u > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                pixel[0] = data[offset++];
                pixel[1] = data[offset++];
                pixel[2] = data[offset++];
                pixel[3] = data[offset++];
            } else {
                const std::uint8_t op = byte >> 6;
                if (op == 0u) {
                    const std::uint8_t slot = byte & 0x3fu;
                    pixel[0] = index[slot][0];
                    pixel[1] = index[slot][1];
                    pixel[2] = index[slot][2];
                    pixel[3] = index[slot][3];
                } else if (op == 1u) {
                    pixel[0] = static_cast<std::uint8_t>(previous[0] + ((byte >> 4) & 0x03u) - 2);
                    pixel[1] = static_cast<std::uint8_t>(previous[1] + ((byte >> 2) & 0x03u) - 2);
                    pixel[2] = static_cast<std::uint8_t>(previous[2] + (byte & 0x03u) - 2);
                } else if (op == 2u) {
                    if (offset >= data.size()) {
                        delete image;

                        return PILLOW_C_INVALID_LENGTH;
                    }
                    const std::uint8_t second = data[offset++];
                    const int dg = static_cast<int>(byte & 0x3fu) - 32;
                    const int dr = static_cast<int>((second >> 4) & 0x0fu) - 8;
                    const int db = static_cast<int>(second & 0x0fu) - 8;
                    pixel[0] = static_cast<std::uint8_t>(previous[0] + dg + dr);
                    pixel[1] = static_cast<std::uint8_t>(previous[1] + dg);
                    pixel[2] = static_cast<std::uint8_t>(previous[2] + dg + db);
                } else {
                    const std::size_t run = static_cast<std::size_t>(byte & 0x3fu) + 1u;
                    if (run > pixel_count - out_pixel) {
                        delete image;
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    for (std::size_t i = 0; i < run; ++i) {
                        std::uint8_t* dst = image->pixels.data() + (out_pixel + i) * static_cast<std::size_t>(channels);
                        dst[0] = previous[0];
                        dst[1] = previous[1];
                        dst[2] = previous[2];
                        if (channels == 4) {
                            dst[3] = previous[3];
                        }
                    }
                    out_pixel += run;
                    continue;
                }
            }

            previous[0] = pixel[0];
            previous[1] = pixel[1];
            previous[2] = pixel[2];
            previous[3] = pixel[3];
            const std::size_t slot = qoi_hash_pixel(pixel);
            index[slot][0] = pixel[0];
            index[slot][1] = pixel[1];
            index[slot][2] = pixel[2];
            index[slot][3] = pixel[3];

            std::uint8_t* dst = image->pixels.data() + out_pixel * static_cast<std::size_t>(channels);
            dst[0] = pixel[0];
            dst[1] = pixel[1];
            dst[2] = pixel[2];
            if (channels == 4) {
                dst[3] = pixel[3];
            }
            ++out_pixel;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_qoi_image_with_colorspace(const PillowCImage* image, const char* path, int colorspace);

int save_qoi_image(const PillowCImage* image, const char* path)
{
    return save_qoi_image_with_colorspace(image, path, 1);
}

// BEHAV-SAVEOPTS-001: Pillow 11.3.0's QOI save writes the colorspace byte
// as 0 when colorspace == "sRGB" and 1 otherwise (the default is linear).
int save_qoi_image_with_colorspace(const PillowCImage* image, const char* path, int colorspace)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!((image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
          (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4)) ||
        image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(14u + image->pixels.size() + 8u);
        out.insert(out.end(), {'q', 'o', 'i', 'f'});
        append_be32(out, static_cast<std::uint32_t>(image->width));
        append_be32(out, static_cast<std::uint32_t>(image->height));
        out.push_back(static_cast<std::uint8_t>(image->channels));
        out.push_back(static_cast<std::uint8_t>(colorspace ? 1 : 0));

        std::uint8_t index[64][4] = {};
        bool valid[64] = {};
        valid[0] = true;
        std::uint8_t previous[4] = {0, 0, 0, 255};
        int run = 0;
        for (int y = 0; y < image->height; ++y) {
            const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < image->width; ++x) {
                const std::uint8_t* src = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels);
                std::uint8_t pixel[4] = {src[0], src[1], src[2], image->channels == 4 ? src[3] : static_cast<std::uint8_t>(255)};
                if (std::memcmp(pixel, previous, 4u) == 0) {
                    ++run;
                    if (run == 62) {
                        qoi_write_run(out, &run);
                    }
                } else {
                    if (run != 0) {
                        qoi_write_run(out, &run);
                    }

                    const std::size_t slot = qoi_hash_pixel(pixel);
                    if (valid[slot] && std::memcmp(index[slot], pixel, 4u) == 0) {
                        out.push_back(static_cast<std::uint8_t>(slot));
                    } else {
                        valid[slot] = true;
                        index[slot][0] = pixel[0];
                        index[slot][1] = pixel[1];
                        index[slot][2] = pixel[2];
                        index[slot][3] = pixel[3];

                        if (previous[3] == pixel[3]) {
                            const int dr = qoi_signed_delta(pixel[0], previous[0]);
                            const int dg = qoi_signed_delta(pixel[1], previous[1]);
                            const int db = qoi_signed_delta(pixel[2], previous[2]);
                            if (dr >= -2 && dr < 2 && dg >= -2 && dg < 2 && db >= -2 && db < 2) {
                                out.push_back(static_cast<std::uint8_t>(
                                    0x40u |
                                    static_cast<std::uint8_t>((dr + 2) << 4) |
                                    static_cast<std::uint8_t>((dg + 2) << 2) |
                                    static_cast<std::uint8_t>(db + 2)));
                            } else {
                                const int dgr = qoi_signed_delta(static_cast<std::uint8_t>(dr), static_cast<std::uint8_t>(dg));
                                const int dgb = qoi_signed_delta(static_cast<std::uint8_t>(db), static_cast<std::uint8_t>(dg));
                                if (dgr >= -8 && dgr < 8 && dg >= -32 && dg < 32 && dgb >= -8 && dgb < 8) {
                                    out.push_back(static_cast<std::uint8_t>(0x80u | static_cast<std::uint8_t>(dg + 32)));
                                    out.push_back(static_cast<std::uint8_t>(
                                        static_cast<std::uint8_t>((dgr + 8) << 4) |
                                        static_cast<std::uint8_t>(dgb + 8)));
                                } else {
                                    out.push_back(0xfeu);
                                    out.push_back(pixel[0]);
                                    out.push_back(pixel[1]);
                                    out.push_back(pixel[2]);
                                }
                            }
                        } else {
                            out.push_back(0xffu);
                            out.push_back(pixel[0]);
                            out.push_back(pixel[1]);
                            out.push_back(pixel[2]);
                            out.push_back(pixel[3]);
                        }
                    }
                }
                previous[0] = pixel[0];
                previous[1] = pixel[1];
                previous[2] = pixel[2];
                previous[3] = pixel[3];
            }
        }
        if (run != 0) {
            qoi_write_run(out, &run);
        }
        out.insert(out.end(), {0, 0, 0, 0, 0, 0, 0, 1});
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void append_tga_footer(std::vector<std::uint8_t>& out)
{
    out.insert(out.end(), {
        0, 0, 0, 0, 0, 0, 0, 0,
        'T', 'R', 'U', 'E', 'V', 'I', 'S', 'I', 'O', 'N',
        '-', 'X', 'F', 'I', 'L', 'E', '.', 0,
    });
}

bool tga_file_pixel_equal(
    const std::vector<std::uint8_t>& pixels,
    std::size_t left_index,
    std::size_t right_index,
    int file_pixel_bytes)
{
    const std::size_t left = left_index * static_cast<std::size_t>(file_pixel_bytes);
    const std::size_t right = right_index * static_cast<std::size_t>(file_pixel_bytes);
    return std::memcmp(pixels.data() + left, pixels.data() + right, static_cast<std::size_t>(file_pixel_bytes)) == 0;
}

bool tga_file_pixel_equal(
    const std::uint8_t* pixels,
    std::size_t left_index,
    std::size_t right_index,
    int file_pixel_bytes)
{
    const std::size_t left = left_index * static_cast<std::size_t>(file_pixel_bytes);
    const std::size_t right = right_index * static_cast<std::size_t>(file_pixel_bytes);
    return std::memcmp(pixels + left, pixels + right, static_cast<std::size_t>(file_pixel_bytes)) == 0;
}

void encode_tga_rle_block(
    const std::uint8_t* pixels,
    int file_pixel_bytes,
    std::size_t pixel_count,
    std::vector<std::uint8_t>& out)
{
    std::size_t index = 0;
    while (index < pixel_count) {
        std::size_t run = 1;
        while (index + run < pixel_count &&
               run < 128u &&
               tga_file_pixel_equal(pixels, index, index + run, file_pixel_bytes)) {
            ++run;
        }

        if (run >= 2u) {
            out.push_back(static_cast<std::uint8_t>(0x80u | (run - 1u)));
            const std::size_t pixel_offset = index * static_cast<std::size_t>(file_pixel_bytes);
            out.insert(out.end(), pixels + pixel_offset, pixels + pixel_offset + static_cast<std::size_t>(file_pixel_bytes));
            index += run;
            continue;
        }

        const std::size_t raw_start = index;
        ++index;
        while (index < pixel_count && index - raw_start < 128u) {
            std::size_t next_run = 1;
            while (index + next_run < pixel_count &&
                   next_run < 128u &&
                   tga_file_pixel_equal(pixels, index, index + next_run, file_pixel_bytes)) {
                ++next_run;
            }
            if (next_run >= 2u) {
                break;
            }
            ++index;
        }

        const std::size_t raw_count = index - raw_start;
        out.push_back(static_cast<std::uint8_t>(raw_count - 1u));
        const std::size_t start_offset = raw_start * static_cast<std::size_t>(file_pixel_bytes);
        const std::size_t end_offset = index * static_cast<std::size_t>(file_pixel_bytes);
        out.insert(out.end(), pixels + start_offset, pixels + end_offset);
    }
}

void encode_tga_rle(
    const std::vector<std::uint8_t>& pixels,
    int file_pixel_bytes,
    std::size_t pixel_count,
    std::vector<std::uint8_t>& out)
{
    encode_tga_rle_block(pixels.data(), file_pixel_bytes, pixel_count, out);
}

void encode_tga_rle_rows(
    const std::vector<std::uint8_t>& pixels,
    int file_pixel_bytes,
    int width,
    int height,
    std::vector<std::uint8_t>& out)
{
    const std::size_t row_pixels = static_cast<std::size_t>(width);
    const std::size_t row_bytes = row_pixels * static_cast<std::size_t>(file_pixel_bytes);
    for (int row = 0; row < height; ++row) {
        encode_tga_rle_block(pixels.data() + static_cast<std::size_t>(row) * row_bytes, file_pixel_bytes, row_pixels, out);
    }
}

int open_tga_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 18u) {
            return PILLOW_C_INVALID_LENGTH;
        }

        const std::uint8_t id_length = data[0];
        const std::uint8_t color_map_type = data[1];
        const std::uint8_t image_type = data[2];
        const std::uint16_t color_map_first = read_le16(data.data() + 3u);
        const std::uint16_t color_map_length = read_le16(data.data() + 5u);
        const std::uint8_t color_map_depth = data[7];
        const int width = static_cast<int>(read_le16(data.data() + 12u));
        const int height = static_cast<int>(read_le16(data.data() + 14u));
        const std::uint8_t bits_per_pixel = data[16];
        const std::uint8_t descriptor = data[17];
        if (width <= 0 || height <= 0 || (color_map_type != 0 && color_map_type != 1)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        int mode = 0;
        int channels = 0;
        int file_pixel_bytes = 0;
        const bool is_rle = image_type == 9 || image_type == 10 || image_type == 11;
        const bool is_palette = color_map_type == 1 && (image_type == 1 || image_type == 9);
        if (is_palette && bits_per_pixel == 8) {
            if (color_map_depth != 24 ||
                static_cast<std::uint32_t>(color_map_first) + static_cast<std::uint32_t>(color_map_length) > 256u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            mode = PILLOW_C_MODE_P;
            channels = 1;
            file_pixel_bytes = 1;
        } else if (color_map_type == 0 && (image_type == 3 || image_type == 11) && bits_per_pixel == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
            file_pixel_bytes = 1;
        } else if (color_map_type == 0 && (image_type == 2 || image_type == 10) && bits_per_pixel == 24) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
            file_pixel_bytes = 3;
        } else if (color_map_type == 0 && (image_type == 2 || image_type == 10) && bits_per_pixel == 32) {
            mode = PILLOW_C_MODE_RGBA;
            channels = 4;
            file_pixel_bytes = 4;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::size_t palette_offset = 18u + static_cast<std::size_t>(id_length);
        const std::size_t palette_entry_bytes = is_palette ? 3u : 0u;
        const std::size_t palette_size = static_cast<std::size_t>(color_map_length) * palette_entry_bytes;
        if (palette_offset > data.size() || palette_size > data.size() - palette_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::vector<std::uint8_t> palette_rgb;
        if (is_palette) {
            palette_rgb.assign(
                (static_cast<std::size_t>(color_map_first) + static_cast<std::size_t>(color_map_length)) * 3u,
                std::uint8_t{0});
            for (std::size_t i = 0; i < static_cast<std::size_t>(color_map_length); ++i) {
                const std::size_t src = palette_offset + i * 3u;
                const std::size_t dst = (static_cast<std::size_t>(color_map_first) + i) * 3u;
                palette_rgb[dst + 0u] = data[src + 2u];
                palette_rgb[dst + 1u] = data[src + 1u];
                palette_rgb[dst + 2u] = data[src + 0u];
            }
        }

        const std::size_t pixel_offset = palette_offset + palette_size;
        const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        const std::size_t file_pixel_size = pixel_count * static_cast<std::size_t>(file_pixel_bytes);
        if (pixel_offset > data.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::vector<std::uint8_t> file_pixels(file_pixel_size);
        if (is_rle) {
            std::size_t input = pixel_offset;
            std::size_t decoded_pixels = 0;
            while (decoded_pixels < pixel_count) {
                if (input >= data.size()) {
                    return PILLOW_C_INVALID_LENGTH;
                }
                const std::uint8_t packet = data[input++];
                const std::size_t packet_count = static_cast<std::size_t>((packet & 0x7fu) + 1u);
                if (packet_count > pixel_count - decoded_pixels) {
                    return PILLOW_C_INVALID_LENGTH;
                }
                if ((packet & 0x80u) != 0) {
                    if (static_cast<std::size_t>(file_pixel_bytes) > data.size() - input) {
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    for (std::size_t i = 0; i < packet_count; ++i) {
                        const std::size_t dst = (decoded_pixels + i) * static_cast<std::size_t>(file_pixel_bytes);
                        std::memcpy(file_pixels.data() + dst, data.data() + input, static_cast<std::size_t>(file_pixel_bytes));
                    }
                    input += static_cast<std::size_t>(file_pixel_bytes);
                } else {
                    const std::size_t raw_size = packet_count * static_cast<std::size_t>(file_pixel_bytes);
                    if (raw_size > data.size() - input) {
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    const std::size_t dst = decoded_pixels * static_cast<std::size_t>(file_pixel_bytes);
                    std::memcpy(file_pixels.data() + dst, data.data() + input, raw_size);
                    input += raw_size;
                }
                decoded_pixels += packet_count;
            }
        } else {
            if (file_pixel_size > data.size() - pixel_offset) {
                return PILLOW_C_INVALID_LENGTH;
            }
            std::memcpy(file_pixels.data(), data.data() + pixel_offset, file_pixel_size);
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, channels, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        if (is_palette) {
            image->palette_rgb = std::move(palette_rgb);
            image->palette_alpha.clear();
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        }

        const bool origin_top = (descriptor & 0x20u) != 0;
        const bool origin_right = (descriptor & 0x10u) != 0;
        for (int y = 0; y < height; ++y) {
            const int file_y = origin_top ? y : (height - 1 - y);
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int file_x = origin_right ? (width - 1 - x) : x;
                const std::uint8_t* src = file_pixels.data() +
                    (static_cast<std::size_t>(file_y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(file_x)) * static_cast<std::size_t>(file_pixel_bytes);
                std::uint8_t* dst = dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
                if (channels == 1) {
                    dst[0] = src[0];
                } else {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    if (channels == 4) {
                        dst[3] = src[3];
                    }
                }
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tga_image_with_full_options(
    const PillowCImage* image,
    const char* path,
    bool rle,
    const std::uint8_t* id_section,
    std::size_t id_size,
    int orientation);

int save_tga_image_with_options(const PillowCImage* image, const char* path, bool rle)
{
    return save_tga_image_with_full_options(image, path, rle, nullptr, 0, -1);
}

// BEHAV-SAVEOPTS-001: Pillow 11.3.0's TGA save options: id_section (up to
// 255 bytes, written after the header; longer values are trimmed with a
// warning -- silently here) and orientation (positive values flip the rows
// to top-down and set the 0x20 descriptor flag; the default -1 keeps the
// bottom-up layout). compression values other than "tga_rle" are ignored.
int save_tga_image_with_full_options(
    const PillowCImage* image,
    const char* path,
    bool rle,
    const std::uint8_t* id_section,
    std::size_t id_size,
    int orientation)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 ||
        image->width > 65535 || image->height > 65535 ||
        !((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_P && image->channels == 1) ||
          (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
          (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode == PILLOW_C_MODE_P &&
        (image->palette_rgb.size() > 256u * 3u || image->palette_rgb.size() % 3u != 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (id_size > 255u) {
        id_size = 255u;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        const bool is_l = image->mode == PILLOW_C_MODE_L;
        const bool is_palette = image->mode == PILLOW_C_MODE_P;
        const int file_pixel_bytes = (is_l || is_palette) ? 1 : image->channels;
        const std::size_t palette_entries = is_palette ? image->palette_rgb.size() / 3u : 0u;
        const std::size_t pixel_count =
            static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
        std::vector<std::uint8_t> out;
        out.reserve(18u + id_size + palette_entries * 3u + pixel_count * static_cast<std::size_t>(file_pixel_bytes) + 26u);

        out.push_back(static_cast<std::uint8_t>(id_size));
        out.push_back(static_cast<std::uint8_t>(is_palette ? 1 : 0));
        out.push_back(static_cast<std::uint8_t>((is_palette ? 1 : (is_l ? 3 : 2)) + (rle ? 8 : 0)));
        append_le16(out, 0);
        append_le16(out, static_cast<std::uint16_t>(palette_entries));
        out.push_back(static_cast<std::uint8_t>(is_palette ? 24 : 0));
        append_le16(out, 0);
        append_le16(out, 0);
        append_le16(out, static_cast<std::uint16_t>(image->width));
        append_le16(out, static_cast<std::uint16_t>(image->height));
        out.push_back(static_cast<std::uint8_t>((is_l || is_palette) ? 8 : image->channels * 8));
        out.push_back(static_cast<std::uint8_t>(
            (image->mode == PILLOW_C_MODE_RGBA ? 8 : 0) | (orientation > 0 ? 0x20 : 0)));
        if (id_size > 0) {
            if (!id_section) {
                return PILLOW_C_NULL_POINTER;
            }
            out.insert(out.end(), id_section, id_section + id_size);
        }
        if (is_palette) {
            for (std::size_t i = 0; i < palette_entries; ++i) {
                const std::size_t offset = i * 3u;
                out.push_back(image->palette_rgb[offset + 2u]);
                out.push_back(image->palette_rgb[offset + 1u]);
                out.push_back(image->palette_rgb[offset + 0u]);
            }
        }

        std::vector<std::uint8_t> file_pixels;
        if (rle) {
            file_pixels.reserve(pixel_count * static_cast<std::size_t>(file_pixel_bytes));
        }
        const bool top_down = orientation > 0;
        for (int row = 0; row < image->height; ++row) {
            const int y = top_down ? row : (image->height - 1 - row);
            const std::uint8_t* pixels_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < image->width; ++x) {
                const std::uint8_t* src = pixels_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels);
                if (is_l || is_palette) {
                    (rle ? file_pixels : out).push_back(src[0]);
                } else {
                    std::vector<std::uint8_t>& target = rle ? file_pixels : out;

                    target.push_back(src[2]);
                    target.push_back(src[1]);
                    target.push_back(src[0]);
                    if (image->mode == PILLOW_C_MODE_RGBA) {
                        target.push_back(src[3]);
                    }
                }
            }
        }
        if (rle) {
            encode_tga_rle_rows(file_pixels, file_pixel_bytes, image->width, image->height, out);
        }

        append_tga_footer(out);
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_tga_image(const PillowCImage* image, const char* path)
{
    return save_tga_image_with_options(image, path, false);
}

bool xbm_is_space(std::uint8_t value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

bool xbm_is_digit(std::uint8_t value)
{
    return value >= '0' && value <= '9';
}

int xbm_hex_value(std::uint8_t value)
{
    if (value >= '0' && value <= '9') {
        return static_cast<int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<int>(value - 'a') + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<int>(value - 'A') + 10;
    }
    return -1;
}

bool xbm_match_at(const std::vector<std::uint8_t>& data, std::size_t pos, const char* text)
{
    const std::size_t length = std::strlen(text);
    return pos <= data.size() && length <= data.size() - pos &&
           std::memcmp(data.data() + pos, text, length) == 0;
}

bool xbm_identifier_ends_with(
    const std::vector<std::uint8_t>& data,
    std::size_t begin,
    std::size_t end,
    const char* suffix)
{
    const std::size_t suffix_length = std::strlen(suffix);
    return end >= begin && suffix_length <= end - begin &&
           std::memcmp(data.data() + end - suffix_length, suffix, suffix_length) == 0;
}

bool xbm_read_decimal_int(const std::vector<std::uint8_t>& data, std::size_t* pos, int* out_value)
{
    if (!pos || !out_value) {
        return false;
    }
    while (*pos < data.size() && xbm_is_space(data[*pos])) {
        ++*pos;
    }
    if (*pos >= data.size() || !xbm_is_digit(data[*pos])) {
        return false;
    }
    std::uint64_t value = 0;
    while (*pos < data.size() && xbm_is_digit(data[*pos])) {
        value = value * 10u + static_cast<std::uint64_t>(data[*pos] - '0');
        if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        ++*pos;
    }
    if (*pos < data.size() && !xbm_is_space(data[*pos])) {
        return false;
    }
    *out_value = static_cast<int>(value);
    return true;
}

bool xbm_read_byte_literal(const std::vector<std::uint8_t>& data, std::size_t* pos, std::uint8_t* out_value)
{
    if (!pos || !out_value) {
        return false;
    }
    while (*pos < data.size() && (xbm_is_space(data[*pos]) || data[*pos] == ',')) {
        ++*pos;
    }
    if (*pos >= data.size() || data[*pos] == '}') {
        return false;
    }

    int base = 10;
    if (data[*pos] == '0' && *pos + 1u < data.size() && (data[*pos + 1u] == 'x' || data[*pos + 1u] == 'X')) {
        base = 16;
        *pos += 2u;
    }

    int value = 0;
    int digits = 0;
    while (*pos < data.size()) {
        const int digit = base == 16 ? xbm_hex_value(data[*pos]) : (xbm_is_digit(data[*pos]) ? static_cast<int>(data[*pos] - '0') : -1);
        if (digit < 0 || digit >= base) {
            break;
        }
        value = value * base + digit;
        if (value > 255) {
            return false;
        }
        ++digits;
        ++*pos;
    }
    if (digits == 0) {
        return false;
    }
    *out_value = static_cast<std::uint8_t>(value);
    return true;
}

bool xbm_parse_header(
    const std::vector<std::uint8_t>& data,
    int* out_width,
    int* out_height,
    std::size_t* out_bits_offset,
    bool* out_has_hotspot,
    int* out_hotspot_x,
    int* out_hotspot_y)
{
    if (!out_width || !out_height || !out_bits_offset || !out_has_hotspot || !out_hotspot_x || !out_hotspot_y) {
        return false;
    }
    *out_width = 0;
    *out_height = 0;
    *out_bits_offset = 0;
    *out_has_hotspot = false;
    *out_hotspot_x = 0;
    *out_hotspot_y = 0;
    bool has_x_hot = false;
    bool has_y_hot = false;

    std::size_t pos = 0;
    while (pos < data.size()) {
        if (xbm_match_at(data, pos, "#define")) {
            pos += 7u;
            if (pos < data.size() && !(data[pos] == ' ' || data[pos] == '\t')) {
                continue;
            }
            while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t')) {
                ++pos;
            }
            const std::size_t name_start = pos;
            while (pos < data.size() && !xbm_is_space(data[pos])) {
                ++pos;
            }
            const std::size_t name_end = pos;
            int value = 0;
            if (!xbm_read_decimal_int(data, &pos, &value)) {
                continue;
            }
            if (xbm_identifier_ends_with(data, name_start, name_end, "_width")) {
                *out_width = value;
            } else if (xbm_identifier_ends_with(data, name_start, name_end, "_height")) {
                *out_height = value;
            } else if (xbm_identifier_ends_with(data, name_start, name_end, "_x_hot")) {
                *out_hotspot_x = value;
                has_x_hot = true;
                *out_has_hotspot = has_y_hot;
            } else if (xbm_identifier_ends_with(data, name_start, name_end, "_y_hot")) {
                *out_hotspot_y = value;
                has_y_hot = true;
                *out_has_hotspot = has_x_hot;
            }
            continue;
        }
        if (data[pos] == '_' && xbm_match_at(data, pos, "_bits[]")) {
            pos += 7u;
            while (pos < data.size() && xbm_is_space(data[pos])) {
                ++pos;
            }
            if (pos >= data.size() || data[pos] != '=') {
                return false;
            }
            ++pos;
            while (pos < data.size() && xbm_is_space(data[pos])) {
                ++pos;
            }
            if (pos >= data.size() || data[pos] != '{') {
                return false;
            }
            *out_bits_offset = pos + 1u;
            return *out_width > 0 && *out_height > 0;
        }
        ++pos;
    }
    return false;
}

int open_xbm_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        int width = 0;
        int height = 0;
        std::size_t bits_offset = 0;
        bool has_hotspot = false;
        int hotspot_x = 0;
        int hotspot_y = 0;
        if (!xbm_parse_header(data, &width, &height, &bits_offset, &has_hotspot, &hotspot_x, &hotspot_y)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t packed_row_bytes = 0;
        std::size_t packed_size = 0;
        PillowCImage size_probe{width, height, PILLOW_C_MODE_1, 1, static_cast<std::size_t>(width), {}};
if (!pillow_c_checked_mode1_raw_size(&size_probe, &packed_row_bytes, &packed_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> packed(packed_size, 0);
        std::size_t pos = bits_offset;
        for (std::size_t i = 0; i < packed_size; ++i) {
            if (!xbm_read_byte_literal(data, &pos, &packed[i])) {
                return PILLOW_C_INVALID_LENGTH;
            }
        }

        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_1,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        image->has_hotspot = has_hotspot;
        image->hotspot_x = hotspot_x;
        image->hotspot_y = hotspot_y;
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* src_row = packed.data() + static_cast<std::size_t>(y) * packed_row_bytes;
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const std::uint8_t packed_byte = src_row[static_cast<std::size_t>(x) / 8u];
                const int bit = (packed_byte >> (x & 7)) & 1;
                dst_row[x] = bit ? 255 : 0;
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void append_xbm_hex_byte(std::vector<std::uint8_t>& out, std::uint8_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    out.push_back('0');
    out.push_back('x');
    out.push_back(static_cast<std::uint8_t>(digits[(value >> 4) & 0x0fu]));
    out.push_back(static_cast<std::uint8_t>(digits[value & 0x0fu]));
}

void append_ascii_text(std::vector<std::uint8_t>& out, const char* text)
{
    while (*text) {
        out.push_back(static_cast<std::uint8_t>(*text));
        ++text;
    }
}

bool read_ascii_name(const char* text, std::string* out)
{
    if (!text || !out) {
        return false;
    }
    out->clear();
    while (*text) {
        const auto ch = static_cast<unsigned char>(*text);
        if (ch > 0x7fu) {
            return false;
        }
        out->push_back(static_cast<char>(ch));
        ++text;
    }
    return true;
}

int append_xbm_bitmap_data(const PillowCImage* image, std::vector<std::uint8_t>* out)
{
    if (!image || !out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1 || image->width < 0 || image->height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t packed_row_bytes = 0;
    std::size_t packed_size = 0;
if (!pillow_c_checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t written = 0;
    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (std::size_t byte_index = 0; byte_index < packed_row_bytes; ++byte_index) {
            std::uint8_t packed = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int x = static_cast<int>(byte_index * 8u) + bit;
                if (x < image->width && src_row[x] != 0) {
                    packed |= static_cast<std::uint8_t>(1u << bit);
                }
            }
            append_xbm_hex_byte(*out, packed);
            ++written;
            if (written < packed_size) {
                out->push_back(',');
                if (written % 15u == 0u) {
                    out->push_back('\n');
                }
            }
        }
    }
    if (packed_size > 0) {
        out->push_back('\n');
    }
    return PILLOW_C_OK;
}

int tobitmap_image(
    const PillowCImage* image,
    const char* name,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !name || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1 || image->width < 0 || image->height < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::string ascii_name;
    if (!read_ascii_name(name, &ascii_name)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::size_t packed_row_bytes = 0;
        std::size_t packed_size = 0;
if (!pillow_c_checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> encoded;
        encoded.reserve(80u + ascii_name.size() * 3u + packed_size * 5u);
        append_ascii_text(encoded, "#define ");
        encoded.insert(encoded.end(), ascii_name.begin(), ascii_name.end());
        append_ascii_text(encoded, "_width ");
        append_ascii_int(encoded, image->width);
        encoded.push_back('\n');
        append_ascii_text(encoded, "#define ");
        encoded.insert(encoded.end(), ascii_name.begin(), ascii_name.end());
        append_ascii_text(encoded, "_height ");
        append_ascii_int(encoded, image->height);
        encoded.push_back('\n');
        append_ascii_text(encoded, "static char ");
        encoded.insert(encoded.end(), ascii_name.begin(), ascii_name.end());
        append_ascii_text(encoded, "_bits[] = {\n");
        const int status = append_xbm_bitmap_data(image, &encoded);
        if (status != PILLOW_C_OK) {
            return status;
        }
        encoded.push_back('}');
        encoded.push_back(';');

        *out_required = encoded.size();
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < encoded.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }
        if (!encoded.empty()) {
            std::memcpy(out, encoded.data(), encoded.size());
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_xbm_image_with_options(
    const PillowCImage* image,
    const char* path,
    bool has_hotspot,
    int hotspot_x,
    int hotspot_y)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1 || image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_hotspot && (hotspot_x < 0 || hotspot_y < 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::size_t packed_row_bytes = 0;
        std::size_t packed_size = 0;
if (!pillow_c_checked_mode1_raw_size(image, &packed_row_bytes, &packed_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> out;
        out.reserve(120u + packed_size * 5u);
        out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'w', 'i', 'd', 't', 'h', ' '});
        append_ascii_int(out, image->width);
        out.push_back('\n');
        out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'h', 'e', 'i', 'g', 'h', 't', ' '});
        append_ascii_int(out, image->height);
        out.push_back('\n');
        if (has_hotspot) {
            out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'x', '_', 'h', 'o', 't', ' '});
            append_ascii_int(out, hotspot_x);
            out.push_back('\n');
            out.insert(out.end(), {'#', 'd', 'e', 'f', 'i', 'n', 'e', ' ', 'i', 'm', '_', 'y', '_', 'h', 'o', 't', ' '});
            append_ascii_int(out, hotspot_y);
            out.push_back('\n');
        }
        out.insert(out.end(), {
            's', 't', 'a', 't', 'i', 'c', ' ', 'c', 'h', 'a', 'r', ' ',
            'i', 'm', '_', 'b', 'i', 't', 's', '[', ']', ' ', '=', ' ', '{', '\n',
        });

        const int status = append_xbm_bitmap_data(image, &out);
        if (status != PILLOW_C_OK) {
            return status;
        }
        out.push_back('}');
        out.push_back(';');
        out.push_back('\n');
        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_xbm_image(const PillowCImage* image, const char* path)
{
    return save_xbm_image_with_options(image, path, false, 0, 0);
}

int bmp_row_stride(int width, int bits_per_pixel, std::size_t* out_stride)
{
    if (width <= 0 || bits_per_pixel <= 0 || !out_stride) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint64_t bits = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(bits_per_pixel);
    const std::uint64_t stride = ((bits + 31u) / 32u) * 4u;
    if (stride > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_stride = static_cast<std::size_t>(stride);
    return PILLOW_C_OK;
}


int open_bmp_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 54 || data[0] != 'B' || data[1] != 'M') {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::uint32_t file_size = read_le32(data.data() + 2);
        const std::uint32_t pixel_offset = read_le32(data.data() + 10);
        const std::uint32_t dib_size = read_le32(data.data() + 14);
        if (dib_size < 40 || pixel_offset > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (file_size != 0 && file_size > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (14u + dib_size > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::int32_t width_i32 = read_le_i32(data.data() + 18);
        const std::int32_t height_i32 = read_le_i32(data.data() + 22);
        const std::uint16_t planes = read_le16(data.data() + 26);
        const std::uint16_t bits_per_pixel = read_le16(data.data() + 28);
        const std::uint32_t compression = read_le32(data.data() + 30);
        const std::uint32_t colors_used = read_le32(data.data() + 46);
        if (width_i32 <= 0 || height_i32 == 0 || planes != 1 || compression != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const bool top_down = height_i32 < 0;
        if (top_down && height_i32 == std::numeric_limits<std::int32_t>::min()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = width_i32;
        const int height = top_down ? -height_i32 : height_i32;
        if (height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        int mode = 0;
        int channels = 0;
        if (bits_per_pixel == 1) {
            mode = PILLOW_C_MODE_1;
            channels = 1;
        } else if (bits_per_pixel == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
        } else if (bits_per_pixel == 24 || bits_per_pixel == 32) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t source_stride = 0;
        int status = bmp_row_stride(width, bits_per_pixel, &source_stride);
        if (status != PILLOW_C_OK) {
            return status;
        }
        std::size_t target_stride = 0;
        std::size_t target_size = 0;
        if (!checked_image_size(width, height, channels, &target_stride, &target_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint64_t required_end =
            static_cast<std::uint64_t>(pixel_offset) +
            static_cast<std::uint64_t>(source_stride) * static_cast<std::uint64_t>(height);
        if (required_end > data.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }

        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            target_stride,
            std::vector<std::uint8_t>(target_size)};

        if (bits_per_pixel == 8) {
            const std::uint32_t palette_entries =
                colors_used != 0 ? colors_used : ((pixel_offset > 14u + dib_size) ? (pixel_offset - 14u - dib_size) / 4u : 0u);
            if (palette_entries > 0) {
                bool grayscale = true;
                const std::size_t palette_offset = static_cast<std::size_t>(14u + dib_size);
                if (palette_offset + static_cast<std::size_t>(palette_entries) * 4u > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                for (std::uint32_t i = 0; i < palette_entries; ++i) {
                    const std::uint8_t b = data[palette_offset + static_cast<std::size_t>(i) * 4u + 0u];
                    const std::uint8_t g = data[palette_offset + static_cast<std::size_t>(i) * 4u + 1u];
                    const std::uint8_t r = data[palette_offset + static_cast<std::size_t>(i) * 4u + 2u];
                    if (r != g || g != b || r != static_cast<std::uint8_t>(i & 0xffu)) {
                        grayscale = false;
                        break;
                    }
                }
                if (!grayscale) {
                    image->mode = PILLOW_C_MODE_P;
                    image->palette_rgb.assign(256u * 3u, std::uint8_t{0});
                    image->palette_alpha.clear();
                    image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
                    const std::uint32_t copy_entries = std::min<std::uint32_t>(palette_entries, 256u);
                    for (std::uint32_t i = 0; i < copy_entries; ++i) {
                        const std::size_t src = palette_offset + static_cast<std::size_t>(i) * 4u;
                        image->palette_rgb[static_cast<std::size_t>(i) * 3u + 0u] = data[src + 2u];
                        image->palette_rgb[static_cast<std::size_t>(i) * 3u + 1u] = data[src + 1u];
                        image->palette_rgb[static_cast<std::size_t>(i) * 3u + 2u] = data[src + 0u];
                    }
                }
            }
        }

        for (int y = 0; y < height; ++y) {
            const int source_y = top_down ? y : (height - 1 - y);
            const std::uint8_t* src_row =
                data.data() + static_cast<std::size_t>(pixel_offset) + static_cast<std::size_t>(source_y) * source_stride;
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (bits_per_pixel == 1) {
                for (int x = 0; x < width; ++x) {
                    const std::uint8_t packed_byte = src_row[static_cast<std::size_t>(x) / 8u];
                    dst_row[x] = (packed_byte & static_cast<std::uint8_t>(0x80u >> (x & 7))) ? 255 : 0;
                }
            } else if (bits_per_pixel == 8) {
                std::memcpy(dst_row, src_row, static_cast<std::size_t>(width));
            } else if (bits_per_pixel == 24) {
                for (int x = 0; x < width; ++x) {
                    dst_row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                }
            } else {
                for (int x = 0; x < width; ++x) {
                    dst_row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x) * 4u + 2u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(x) * 4u + 1u];
                    dst_row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(x) * 4u + 0u];
                }
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_bmp_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode != PILLOW_C_MODE_1 &&
        image->mode != PILLOW_C_MODE_L &&
        image->mode != PILLOW_C_MODE_RGB &&
        image->mode != PILLOW_C_MODE_RGBA) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    const int bits_per_pixel = image->mode == PILLOW_C_MODE_1 ? 1 : (image->mode == PILLOW_C_MODE_L ? 8 : (image->mode == PILLOW_C_MODE_RGBA ? 32 : 24));
    std::size_t row_stride = 0;
    int status = bmp_row_stride(image->width, bits_per_pixel, &row_stride);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const std::size_t palette_size = image->mode == PILLOW_C_MODE_L ? 256u * 4u : (image->mode == PILLOW_C_MODE_1 ? 2u * 4u : 0u);
    const std::uint64_t pixel_size_u64 = static_cast<std::uint64_t>(row_stride) * static_cast<std::uint64_t>(image->height);
    const std::uint64_t pixel_offset_u64 = 14u + 40u + palette_size;
    const std::uint64_t file_size_u64 = pixel_offset_u64 + pixel_size_u64;
    if (file_size_u64 > std::numeric_limits<std::uint32_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(static_cast<std::size_t>(file_size_u64));
        out.push_back('B');
        out.push_back('M');
        append_le32(out, static_cast<std::uint32_t>(file_size_u64));
        append_le16(out, 0);
        append_le16(out, 0);
        append_le32(out, static_cast<std::uint32_t>(pixel_offset_u64));
        append_le32(out, 40);
        append_le32(out, static_cast<std::uint32_t>(image->width));
        append_le32(out, static_cast<std::uint32_t>(image->height));
        append_le16(out, 1);
        append_le16(out, static_cast<std::uint16_t>(bits_per_pixel));
        append_le32(out, 0);
        append_le32(out, static_cast<std::uint32_t>(pixel_size_u64));
        append_le32(out, 3780);
        append_le32(out, 3780);
        append_le32(out, image->mode == PILLOW_C_MODE_L ? 256u : (image->mode == PILLOW_C_MODE_1 ? 2u : 0u));
        append_le32(out, image->mode == PILLOW_C_MODE_L ? 256u : (image->mode == PILLOW_C_MODE_1 ? 2u : 0u));

        if (image->mode == PILLOW_C_MODE_L) {
            for (int i = 0; i < 256; ++i) {
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(0);
            }
        } else if (image->mode == PILLOW_C_MODE_1) {
            for (int i = 0; i < 2; ++i) {
                const std::uint8_t value = i == 0 ? 0u : 255u;
                out.push_back(value);
                out.push_back(value);
                out.push_back(value);
                out.push_back(0);
            }
        }

        const std::size_t bytes_per_pixel = static_cast<std::size_t>(bits_per_pixel / 8);
        std::vector<std::uint8_t> row(row_stride, std::uint8_t{0});
        for (int y = image->height - 1; y >= 0; --y) {
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (image->mode == PILLOW_C_MODE_L) {
                std::memcpy(row.data(), src_row, static_cast<std::size_t>(image->width));
            } else if (image->mode == PILLOW_C_MODE_1) {
                for (int x = 0; x < image->width; ++x) {
                    if (src_row[x] != 0) {
                        row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
                    }
                }
            } else if (image->mode == PILLOW_C_MODE_RGB) {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 0u] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 1u] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 2u] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                }
            } else {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 0u] = src_row[static_cast<std::size_t>(x) * 4u + 2u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 1u] = src_row[static_cast<std::size_t>(x) * 4u + 1u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 2u] = src_row[static_cast<std::size_t>(x) * 4u + 0u];
                    row[static_cast<std::size_t>(x) * bytes_per_pixel + 3u] = src_row[static_cast<std::size_t>(x) * 4u + 3u];
                }
            }
            out.insert(out.end(), row.begin(), row.end());
        }

        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_msp_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode != PILLOW_C_MODE_1 || image->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(32u + static_cast<std::size_t>(image->width / 8u + 1u) * static_cast<std::size_t>(image->height));
        std::uint16_t words[16] = {0};
        words[0] = 0x6144u; // "Da" little-endian
        words[1] = 0x4D6Eu; // "nM"
        words[2] = static_cast<std::uint16_t>(image->width);
        words[3] = static_cast<std::uint16_t>(image->height);
        words[4] = 1;
        words[5] = 1;
        words[6] = 1;
        words[7] = 1;
        words[8] = static_cast<std::uint16_t>(image->width);
        words[9] = static_cast<std::uint16_t>(image->height);
        std::uint16_t checksum = 0;
        for (int i = 0; i < 16; ++i) {
            checksum ^= words[i];
        }
        words[12] = checksum;
        for (int i = 0; i < 16; ++i) {
            append_le16(out, words[i]);
        }
        const std::size_t row_bytes = (static_cast<std::size_t>(image->width) + 7u) / 8u;
        std::vector<std::uint8_t> row(row_bytes, std::uint8_t{0});
        for (int y = 0; y < image->height; ++y) {
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < image->width; ++x) {
                if (src_row[x] != 0) {
                    row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
                }
            }
            out.insert(out.end(), row.begin(), row.end());
        }
        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_msp_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 32) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const bool is_danm = data[0] == 'D' && data[1] == 'a' && data[2] == 'n' && data[3] == 'M';
        const bool is_lins = data[0] == 'L' && data[1] == 'i' && data[2] == 'n' && data[3] == 'S';
        if (!is_danm && !is_lins) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::uint16_t words[16];
        std::uint16_t checksum = 0;
        for (int i = 0; i < 16; ++i) {
            words[i] = read_le16(data.data() + static_cast<std::size_t>(i) * 2u);
            checksum ^= words[i];
        }
        if (checksum != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = words[2];
        const int height = words[3];
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_1,
            1,
            stride,
            std::vector<std::uint8_t>(size)};

        const std::size_t row_bytes = (static_cast<std::size_t>(width) + 7u) / 8u;
        std::vector<std::uint8_t> packed(row_bytes * static_cast<std::size_t>(height), std::uint8_t{0});
        if (is_danm) {
            const std::size_t required = 32u + packed.size();
            if (required > data.size()) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            std::memcpy(packed.data(), data.data() + 32u, packed.size());
        } else {
            if (32u + static_cast<std::size_t>(height) * 2u > data.size()) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            std::size_t data_pos = 32u + static_cast<std::size_t>(height) * 2u;
            std::size_t packed_pos = 0;
            for (int y = 0; y < height; ++y) {
                const std::uint16_t rowlen = read_le16(data.data() + 32u + static_cast<std::size_t>(y) * 2u);
                if (rowlen == 0) {
                    std::fill(packed.begin() + static_cast<std::ptrdiff_t>(packed_pos),
                              packed.begin() + static_cast<std::ptrdiff_t>(packed_pos + row_bytes),
                              std::uint8_t{0xFFu});
                    packed_pos += row_bytes;
                    continue;
                }
                if (data_pos + rowlen > data.size()) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
                std::size_t idx = 0;
                while (idx < rowlen) {
                    const std::uint8_t runtype = data[data_pos + idx];
                    ++idx;
                    if (runtype == 0) {
                        if (idx + 2u > rowlen) {
                            delete image;
                            return PILLOW_C_INVALID_LENGTH;
                        }
                        const std::uint8_t runcount = data[data_pos + idx];
                        const std::uint8_t runval = data[data_pos + idx + 1u];
                        idx += 2u;
                        if (packed_pos + runcount > packed.size()) {
                            delete image;
                            return PILLOW_C_INVALID_LENGTH;
                        }
                        std::fill(packed.begin() + static_cast<std::ptrdiff_t>(packed_pos),
                                  packed.begin() + static_cast<std::ptrdiff_t>(packed_pos + runcount),
                                  runval);
                        packed_pos += runcount;
                    } else {
                        const std::uint8_t runcount = runtype;
                        if (idx + runcount > rowlen || packed_pos + runcount > packed.size()) {
                            delete image;
                            return PILLOW_C_INVALID_LENGTH;
                        }
                        std::memcpy(packed.data() + packed_pos, data.data() + data_pos + idx, runcount);
                        idx += runcount;
                        packed_pos += runcount;
                    }
                }
                data_pos += rowlen;
                if (packed_pos % row_bytes != 0) {
                    delete image;
                    return PILLOW_C_INVALID_LENGTH;
                }
            }
        }
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* src_row = packed.data() + static_cast<std::size_t>(y) * row_bytes;
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const std::uint8_t packed_byte = src_row[static_cast<std::size_t>(x) / 8u];
                dst_row[x] = (packed_byte & static_cast<std::uint8_t>(0x80u >> (x & 7))) ? 255 : 0;
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_blp_image(const PillowCImage* image, const char* path, int blp1)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->palette_alpha_mode != PILLOW_C_PALETTE_ALPHA_NONE) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(128u + 1024u + static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) + 64u);
        if (blp1) {
            append_ascii_text(out, "BLP1");
            append_le32(out, 1);
            append_le32(out, 0); // alpha depth
            append_le32(out, static_cast<std::uint32_t>(image->width));
            append_le32(out, static_cast<std::uint32_t>(image->height));
            append_le32(out, 5);
            append_le32(out, 0);
        } else {
            append_ascii_text(out, "BLP2");
            append_le32(out, 1);
            out.push_back(1); // encoding: uncompressed
            out.push_back(0); // alpha depth
            out.push_back(0); // alpha encoding
            out.push_back(0); // mips
            append_le32(out, static_cast<std::uint32_t>(image->width));
            append_le32(out, static_cast<std::uint32_t>(image->height));
        }
        const std::size_t header_size = out.size();
        // Pillow's C encoder writes 1172 (the BLP2-style offset = 20 + 128
        // + 1024) for BLP1 files too, even though the BLP1 header is 28
        // bytes; the actual indices still follow the 128-byte preamble.
        const std::uint32_t pixel_offset = 1172u;
        (void)header_size;
        append_le32(out, pixel_offset);
        for (int i = 0; i < 60; ++i) {
            out.push_back(0);
        }
        append_le32(out, static_cast<std::uint32_t>(image->width) * static_cast<std::uint32_t>(image->height));
        for (int i = 0; i < 60; ++i) {
            out.push_back(0);
        }
        // Pillow quirk: the writer walks the planar RGB blob linearly with
        // a BGR swap, so entry k = (blob[3k+2], blob[3k+1], blob[3k+0]).
        const auto blob_byte = [&image](std::size_t i) -> std::uint8_t {
            if (i < 256u) {
                return image->palette_rgb[i * 3u + 0u];
            }
            if (i < 512u) {
                return image->palette_rgb[(i - 256u) * 3u + 1u];
            }
            if (i < 768u) {
                return image->palette_rgb[(i - 512u) * 3u + 2u];
            }
            return 0;
        };
        for (int k = 0; k < 256; ++k) {
            const std::size_t index = static_cast<std::size_t>(k) * 3u;
            out.push_back(blob_byte(index + 2u));
            out.push_back(blob_byte(index + 1u));
            out.push_back(blob_byte(index));
            out.push_back(255);
        }
        out.insert(out.end(), image->pixels.begin(), image->pixels.end());
        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_blp_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const bool is_blp1 = data.size() >= 4 && data[0] == 'B' && data[1] == 'L' && data[2] == 'P' && data[3] == '1';
        const bool is_blp2 = data.size() >= 4 && data[0] == 'B' && data[1] == 'L' && data[2] == 'P' && data[3] == '2';
        if (!is_blp1 && !is_blp2) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::size_t header_size = is_blp1 ? 28u : 20u;
        if (data.size() < header_size + 4u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t compression = read_le32(data.data() + 4u);
        if (compression != 1) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t width = read_le32(data.data() + (is_blp1 ? 12u : 12u));
        const std::uint32_t height = read_le32(data.data() + (is_blp1 ? 16u : 16u));
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t pixel_offset = read_le32(data.data() + header_size);
        const std::uint32_t pixel_count = data.size() >= header_size + 68u ? read_le32(data.data() + header_size + 64u) : 0u;
        if (pixel_offset > data.size() || pixel_count != width * height) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::size_t palette_offset = header_size + 128u;
        if (palette_offset + 1024u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // The offset field carries Pillow's BLP2-style 1172 quirk even for
        // BLP1 files; the real index data starts after the palette block.
        const std::size_t indices_offset = data.size() - static_cast<std::size_t>(pixel_count);
        if (indices_offset < palette_offset + 1024u) {
            return PILLOW_C_INVALID_LENGTH;
        }

        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(static_cast<int>(width), static_cast<int>(height), 3, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            static_cast<int>(width),
            static_cast<int>(height),
            PILLOW_C_MODE_RGB,
            3,
            stride,
            std::vector<std::uint8_t>(size)};
        for (std::uint32_t i = 0; i < pixel_count; ++i) {
            const std::uint8_t index = data[indices_offset + i];
            const std::size_t entry = palette_offset + static_cast<std::size_t>(index) * 4u;
            if (entry + 4u > data.size()) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::uint8_t b = data[entry];
            const std::uint8_t g = data[entry + 1u];
            const std::uint8_t r = data[entry + 2u];
            image->pixels[static_cast<std::size_t>(i) * 3u + 0u] = r;
            image->pixels[static_cast<std::size_t>(i) * 3u + 1u] = g;
            image->pixels[static_cast<std::size_t>(i) * 3u + 2u] = b;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_pcx_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int version = 0;
    int bits = 0;
    int planes = 0;
    if (image->mode == PILLOW_C_MODE_1 && image->channels == 1) {
        version = 2;
        bits = 1;
        planes = 1;
    } else if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        version = 5;
        bits = 8;
        planes = 1;
    } else if (image->mode == PILLOW_C_MODE_P && image->channels == 1) {
        version = 5;
        bits = 8;
        planes = 1;
    } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        version = 5;
        bits = 8;
        planes = 3;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::size_t stride = (static_cast<std::size_t>(image->width) * static_cast<std::size_t>(bits) + 7u) / 8u;
        stride += stride % 2u;
        std::vector<std::uint8_t> out;
        out.reserve(128u + static_cast<std::size_t>(planes) * stride * static_cast<std::size_t>(image->height) * 2u + 1024u);

        out.push_back(10);
        out.push_back(static_cast<std::uint8_t>(version));
        out.push_back(1);
        out.push_back(static_cast<std::uint8_t>(bits));
        append_le16(out, 0);
        append_le16(out, 0);
        append_le16(out, static_cast<std::uint16_t>(image->width - 1));
        append_le16(out, static_cast<std::uint16_t>(image->height - 1));
        append_le16(out, 100);
        append_le16(out, 100);
        for (int i = 0; i < 24; ++i) {
            out.push_back(0);
        }
        for (int i = 0; i < 24; ++i) {
            out.push_back(0xFF);
        }
        out.push_back(0);
        out.push_back(static_cast<std::uint8_t>(planes));
        append_le16(out, static_cast<std::uint16_t>(stride));
        append_le16(out, 1);
        append_le16(out, static_cast<std::uint16_t>(image->width));
        append_le16(out, static_cast<std::uint16_t>(image->height));
        for (int i = 0; i < 54; ++i) {
            out.push_back(0);
        }

        const std::size_t row_bytes = static_cast<std::size_t>(planes) * stride;
        std::vector<std::uint8_t> row(row_bytes, std::uint8_t{0});
        for (int y = 0; y < image->height; ++y) {
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (image->mode == PILLOW_C_MODE_1) {
                for (int x = 0; x < image->width; ++x) {
                    if (src_row[x] != 0) {
                        row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
                    }
                }
            } else if (image->mode == PILLOW_C_MODE_L || image->mode == PILLOW_C_MODE_P) {
                std::memcpy(row.data(), src_row, static_cast<std::size_t>(image->width));
            } else {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x)] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                    row[stride + static_cast<std::size_t>(x)] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    row[stride * 2u + static_cast<std::size_t>(x)] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                }
            }
            std::size_t pos = 0;
            while (pos < row_bytes) {
                std::size_t run = 1;
                while (pos + run < row_bytes && row[pos + run] == row[pos] && run < 63u) {
                    ++run;
                }
                if (run > 1 || row[pos] >= 0xC0u) {
                    out.push_back(static_cast<std::uint8_t>(0xC0u | run));
                    out.push_back(row[pos]);
                } else {
                    out.push_back(row[pos]);
                }
                pos += run;
            }
        }

        if (image->mode == PILLOW_C_MODE_P) {
            // Pillow's putpalette pads short palettes with zeros and the C
            // codec always writes the full 256-entry RGB LUT; reproduce that
            // padding instead of reading past the stored palette vector.
            out.push_back(12);
            const std::size_t stored = image->palette_rgb.size();
            for (int i = 0; i < 256; ++i) {
                for (int c = 0; c < 3; ++c) {
                    const std::size_t off = static_cast<std::size_t>(i) * 3u + static_cast<std::size_t>(c);
                    out.push_back(off < stored ? image->palette_rgb[off] : std::uint8_t{0});
                }
            }
        } else if (image->mode == PILLOW_C_MODE_L) {
            out.push_back(12);
            for (int i = 0; i < 256; ++i) {
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(static_cast<std::uint8_t>(i));
                out.push_back(static_cast<std::uint8_t>(i));
            }
        }

        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_pcx_from_data(const std::vector<std::uint8_t>& data, PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        if (data.size() < 128 || data[0] != 10) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int version = data[1];
        const int bits = data[3];
        const int planes = data[65];
        const int xmin = read_le16(data.data() + 4);
        const int ymin = read_le16(data.data() + 6);
        const int xmax = read_le16(data.data() + 8);
        const int ymax = read_le16(data.data() + 10);
        const int width = xmax - xmin + 1;
        const int height = ymax - ymin + 1;
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        int mode = 0;
        std::string rawmode;
        if (bits == 1 && planes == 1) {
            mode = PILLOW_C_MODE_1;
            rawmode = "1";
        } else if (version == 5 && bits == 8 && planes == 1) {
            mode = PILLOW_C_MODE_L;
            rawmode = "L";
        } else if (version == 5 && bits == 8 && planes == 3) {
            mode = PILLOW_C_MODE_RGB;
            rawmode = "RGB;L";
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t stride = (static_cast<std::size_t>(width) * static_cast<std::size_t>(bits) + 7u) / 8u;
        if (read_le16(data.data() + 66) != static_cast<std::uint16_t>(stride)) {
            stride += stride % 2u;
        }

        std::size_t row_bytes = static_cast<std::size_t>(planes) * stride;
        std::vector<std::uint8_t> packed;
        packed.reserve(row_bytes * static_cast<std::size_t>(height));
        std::size_t pos = 128u;
        while (packed.size() < row_bytes * static_cast<std::size_t>(height)) {
            if (pos >= data.size()) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::uint8_t byte = data[pos++];
            if ((byte & 0xC0u) == 0xC0u) {
                const std::size_t count = byte & 0x3Fu;
                if (pos >= data.size() || count == 0) {
                    return PILLOW_C_INVALID_LENGTH;
                }
                const std::uint8_t value = data[pos++];
                for (std::size_t i = 0; i < count; ++i) {
                    if (packed.size() < row_bytes * static_cast<std::size_t>(height)) {
                        packed.push_back(value);
                    }
                }
            } else {
                packed.push_back(byte);
            }
        }

        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        int channels = mode == PILLOW_C_MODE_RGB ? 3 : 1;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};

        if (mode == PILLOW_C_MODE_1) {
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = packed.data() + static_cast<std::size_t>(y) * row_bytes;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                for (int x = 0; x < width; ++x) {
                    const std::uint8_t packed_byte = src_row[static_cast<std::size_t>(x) / 8u];
                    dst_row[x] = (packed_byte & static_cast<std::uint8_t>(0x80u >> (x & 7))) ? 255 : 0;
                }
            }
        } else if (mode == PILLOW_C_MODE_L || mode == PILLOW_C_MODE_P) {
            for (int y = 0; y < height; ++y) {
                std::memcpy(
                    image->pixels.data() + static_cast<std::size_t>(y) * image_stride,
                    packed.data() + static_cast<std::size_t>(y) * row_bytes,
                    static_cast<std::size_t>(width));
            }
        } else {
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = packed.data() + static_cast<std::size_t>(y) * row_bytes;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                for (int x = 0; x < width; ++x) {
                    // Pillow's raw RGB;L decoder reads tight width-sized
                    // channel blocks, so odd-width rows (stride > width)
                    // misread the padding -- reproduced exactly here.
                    dst_row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x)];
                    dst_row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
                    dst_row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(width) * 2u + static_cast<std::size_t>(x)];
                }
            }
        }

        if (mode == PILLOW_C_MODE_L && version == 5 && data.size() >= 769u && data[data.size() - 769u] == 12) {
            const std::size_t palette_start = data.size() - 768u;
            bool linear = true;
            for (int i = 0; i < 256; ++i) {
                const std::size_t off = palette_start + static_cast<std::size_t>(i) * 3u;
                if (off + 2u >= data.size() || data[off] != static_cast<std::uint8_t>(i) || data[off + 1u] != static_cast<std::uint8_t>(i) || data[off + 2u] != static_cast<std::uint8_t>(i)) {
                    linear = false;
                    break;
                }
            }
            if (!linear) {
                image->mode = PILLOW_C_MODE_P;
                image->palette_rgb.assign(256u * 3u, std::uint8_t{0});
                for (int i = 0; i < 256; ++i) {
                    const std::size_t off = palette_start + static_cast<std::size_t>(i) * 3u;
                    if (off + 2u >= data.size()) {
                        break;
                    }
                    image->palette_rgb[static_cast<std::size_t>(i) * 3u + 0u] = data[off];
                    image->palette_rgb[static_cast<std::size_t>(i) * 3u + 1u] = data[off + 1u];
                    image->palette_rgb[static_cast<std::size_t>(i) * 3u + 2u] = data[off + 2u];
                }
            }
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_pcx_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return open_pcx_from_data(data, out_image);
}

// BEHAV-OPEN-001: PIXAR/XVTHUMB/DCX open status codes local to those
// routes. -29 means the raw payload is short; the facade computes
// Pillow's per-row "bytes not processed" count from the file itself.
constexpr int PILLOW_C_OPEN_TRUNCATED = -29;
// BEHAV-OPEN-002: FTEX/SUN/GBR/FITS/XPM local status codes.
constexpr int PILLOW_C_OPEN_MULTI_FORMAT = -30;   // FTEX AssertionError
constexpr int PILLOW_C_OPEN_NOT_ENOUGH = -31;     // "not enough image data"
constexpr int PILLOW_C_OPEN_RLE_TRUNCATED = -32;  // "(0 bytes not processed)"
constexpr int PILLOW_C_OPEN_FITS_TRUNCATED = -33; // "Truncated FITS file"
constexpr int PILLOW_C_OPEN_FITS_NO_DATA = -34;   // "No image data"
constexpr int PILLOW_C_OPEN_XPM_BAD = -35;        // "cannot read this XPM file"
constexpr int PILLOW_C_OPEN_XPM_KEY = -36;        // "tuple.index(x): x not in tuple"
constexpr int PILLOW_C_OPEN_XPM_RGB_KEY = -39;    // RGB-mode KeyError (facade rescan)
constexpr int PILLOW_C_OPEN_FTEX_FORMAT = -38;    // "Invalid texture compression format"
// BEHAV-OPEN-004: PSD local status codes.
constexpr int PILLOW_C_OPEN_PSD_CHANNELS = -47;   // "not enough channels"

int open_pixar_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 512u || data[0] != 0x80u || data[1] != 0xE8u || data[2] != 0x00u || data[3] != 0x00u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int height = read_le16(data.data() + 416);
        const int width = read_le16(data.data() + 418);
        if (read_le16(data.data() + 424) != 14 || read_le16(data.data() + 426) != 2) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint64_t expected = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 3u;
        const std::size_t payload = data.size() > 1024u ? data.size() - 1024u : 0u;
        if (payload < expected) {
            return PILLOW_C_OPEN_TRUNCATED;
        }
        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, 3, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_RGB,
            3,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        std::memcpy(image->pixels.data(), data.data() + 1024u, static_cast<std::size_t>(expected));
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_xvthumb_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 6u || std::memcmp(data.data(), "P7 332", 6u) != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // Skip to the beginning of the next line (Pillow: readline()).
        std::size_t pos = 6u;
        while (pos < data.size() && data[pos] != '\n') {
            ++pos;
        }
        if (pos >= data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ++pos;

        // Skip "#" comment lines; the first non-comment line is "W H".
        int width = 0;
        int height = 0;
        for (;;) {
            const std::size_t line_start = pos;
            while (pos < data.size() && data[pos] != '\n') {
                ++pos;
            }
            if (pos >= data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t line_length = pos - line_start;
            if (line_length > 0u && data[line_start] == '#') {
                ++pos;
                continue;
            }
            const std::uint8_t* line = data.data() + line_start;
            std::size_t cursor = 0u;
            while (cursor < line_length && (line[cursor] == ' ' || line[cursor] == '\t')) {
                ++cursor;
            }
            const std::uint8_t* w_start = line + cursor;
            while (cursor < line_length && line[cursor] >= '0' && line[cursor] <= '9') {
                ++cursor;
            }
            while (cursor < line_length && (line[cursor] == ' ' || line[cursor] == '\t')) {
                ++cursor;
            }
            const std::uint8_t* h_start = line + cursor;
            while (cursor < line_length && line[cursor] >= '0' && line[cursor] <= '9') {
                ++cursor;
            }
            width = 0;
            height = 0;
            for (const std::uint8_t* p = w_start; p < line + line_length && *p >= '0' && *p <= '9'; ++p) {
                width = width * 10 + (*p - '0');
                if (width > 1000000) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
            for (const std::uint8_t* p = h_start; p < line + line_length && *p >= '0' && *p <= '9'; ++p) {
                height = height * 10 + (*p - '0');
                if (height > 1000000) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
            ++pos;
            break;
        }
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::uint64_t expected = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        const std::size_t payload = data.size() - pos;
        if (payload < expected) {
            return PILLOW_C_OPEN_TRUNCATED;
        }
        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, 1, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_P,
            1,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        std::memcpy(image->pixels.data(), data.data() + pos, static_cast<std::size_t>(expected));

        // Pillow's RGB332 thumbnail palette: r*255//7, g*255//7, b*255//3.
        image->palette_rgb.assign(256u * 3u, std::uint8_t{0});
        for (int r = 0; r < 8; ++r) {
            for (int g = 0; g < 8; ++g) {
                for (int b = 0; b < 4; ++b) {
                    const std::size_t index = static_cast<std::size_t>(r * 32 + g * 4 + b);
                    image->palette_rgb[index * 3u + 0u] = static_cast<std::uint8_t>((r * 255) / 7);
                    image->palette_rgb[index * 3u + 1u] = static_cast<std::uint8_t>((g * 255) / 7);
                    image->palette_rgb[index * 3u + 2u] = static_cast<std::uint8_t>((b * 255) / 3);
                }
            }
        }
        image->palette_alpha.clear();
        image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_dcx_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (data.size() < 4u || read_le32(data.data()) != 0x3ADE68B1u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    // Component directory: LE32 offsets, 0 terminates, up to 1024.
    if (data.size() < 8u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::uint32_t first_offset = 0u;
    for (int i = 0; i < 1024; ++i) {
        const std::size_t entry = 4u + static_cast<std::size_t>(i) * 4u;
        if (entry + 4u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t offset = read_le32(data.data() + entry);
        if (offset == 0u) {
            break;
        }
        if (first_offset == 0u) {
            first_offset = offset;
        }
    }
    if (first_offset == 0u || first_offset > data.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> inner(data.begin() + static_cast<std::ptrdiff_t>(first_offset), data.end());
    return open_pcx_from_data(inner, out_image);
}

// ---------------------------------------------------------------------------
// BEHAV-OPEN-002: FTEX / SUN / GBR / FITS / XPM openers.
// ---------------------------------------------------------------------------

void decode_bc1_block(const std::uint8_t* block, int dst_x, int dst_y,
                      int width, int height, std::uint8_t* pixels, std::size_t stride)
{
    const std::uint16_t c0 = read_le16(block);
    const std::uint16_t c1 = read_le16(block + 2);
    const std::uint32_t indices = read_le32(block + 4);
    const auto expand5 = [](std::uint32_t v) {
        return static_cast<std::uint8_t>((v << 3) | (v >> 2));
    };
    const auto expand6 = [](std::uint32_t v) {
        return static_cast<std::uint8_t>((v << 2) | (v >> 4));
    };
    const std::uint8_t color0[4] = {
        expand5((c0 >> 11) & 31u),
        expand6((c0 >> 5) & 63u),
        expand5(c0 & 31u),
        255};
    const std::uint8_t color1[4] = {
        expand5((c1 >> 11) & 31u),
        expand6((c1 >> 5) & 63u),
        expand5(c1 & 31u),
        255};
    std::uint8_t color2[4] = {0, 0, 0, 255};
    std::uint8_t color3[4] = {0, 0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        color2[i] = static_cast<std::uint8_t>((color0[i] + color1[i]) / 2u);
    }
    if (c0 > c1) {
        for (int i = 0; i < 3; ++i) {
            color2[i] = static_cast<std::uint8_t>((2u * color0[i] + color1[i] + 1u) / 3u);
            color3[i] = static_cast<std::uint8_t>((color0[i] + 2u * color1[i] + 1u) / 3u);
        }
        color3[3] = 255;
    }
    const std::uint8_t* table[4] = {color0, color1, color2, color3};
    for (int i = 0; i < 16; ++i) {
        const int x = dst_x + (i & 3);
        const int y = dst_y + (i >> 2);
        if (x < width && y < height) {
            const std::uint32_t index = (indices >> (i * 2u)) & 3u;
            std::uint8_t* dst = pixels + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4u;
            for (int c = 0; c < 4; ++c) {
                dst[c] = table[index][c];
            }
        }
    }
}

int open_ftex_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 24u || std::memcmp(data.data(), "FTEX", 4u) != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::int32_t width = static_cast<std::int32_t>(read_le32(data.data() + 8));
        const std::int32_t height = static_cast<std::int32_t>(read_le32(data.data() + 12));
        const std::int32_t format_count = static_cast<std::int32_t>(read_le32(data.data() + 20));
        if (format_count != 1) {
            return PILLOW_C_OPEN_MULTI_FORMAT;
        }
        if (data.size() < 32u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::int32_t format = static_cast<std::int32_t>(read_le32(data.data() + 24));
        const std::uint32_t where = read_le32(data.data() + 28);
        if (format != 0 && format != 1) {
            return PILLOW_C_OPEN_FTEX_FORMAT;
        }
        if (where + 4u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t mipmap_size = read_le32(data.data() + where);
        std::size_t payload_start = static_cast<std::size_t>(where) + 4u;
        if (payload_start > data.size()) {
            payload_start = data.size();
        }
        const std::size_t payload = data.size() - payload_start;
        const std::size_t present = payload < mipmap_size ? payload : mipmap_size;

        int mode = 0;
        int channels = 0;
        std::size_t expected = 0;
        if (format == 1) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
            expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u;
        } else {
            mode = PILLOW_C_MODE_RGBA;
            channels = 4;
            expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        }
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (format == 0) {
            if (present < static_cast<std::size_t>(((width + 3) / 4) * ((height + 3) / 4) * 8)) {
                return PILLOW_C_OPEN_RLE_TRUNCATED;
            }
        } else {
            if (present < expected) {
                return PILLOW_C_OPEN_TRUNCATED;
            }
        }
        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        if (format == 1) {
            std::memcpy(image->pixels.data(), data.data() + payload_start, expected);
        } else {
            for (int block_y = 0; block_y < (height + 3) / 4; ++block_y) {
                for (int block_x = 0; block_x < (width + 3) / 4; ++block_x) {
                    const std::size_t offset = (static_cast<std::size_t>(block_y) * static_cast<std::size_t>((width + 3) / 4) + static_cast<std::size_t>(block_x)) * 8u;
                    decode_bc1_block(data.data() + payload_start + offset, block_x * 4, block_y * 4,
                                     width, height, image->pixels.data(), image_stride);
                }
            }
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_sun_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 32u || read_be32(data.data()) != 0x59A66A95u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::int32_t width = static_cast<std::int32_t>(read_be32(data.data() + 4));
        const std::int32_t height = static_cast<std::int32_t>(read_be32(data.data() + 8));
        const std::int32_t depth = static_cast<std::int32_t>(read_be32(data.data() + 12));
        const std::int32_t file_type = static_cast<std::int32_t>(read_be32(data.data() + 20));
        const std::int32_t palette_type = static_cast<std::int32_t>(read_be32(data.data() + 24));
        const std::int32_t palette_length = static_cast<std::int32_t>(read_be32(data.data() + 28));

        std::size_t offset = 32u;
        int mode = 0;
        int channels = 0;
        if (depth == 1) {
            mode = PILLOW_C_MODE_1;
            channels = 1;
        } else if (depth == 4 || depth == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
        } else if (depth == 24 || depth == 32) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        bool has_palette = false;
        if (palette_length != 0) {
            if (palette_length > 1024 || palette_type != 1) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            offset += static_cast<std::size_t>(palette_length);
            if (offset > data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            has_palette = true;
            if (mode == PILLOW_C_MODE_L) {
                mode = PILLOW_C_MODE_P;
            }
        }
        const std::size_t stride = static_cast<std::size_t>(((width * depth + 15) / 16) * 2);
        const std::size_t expected = stride * static_cast<std::size_t>(height);
        if (width <= 0 || height <= 0 || expected == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (file_type != 2 && file_type != 0 && file_type != 1 && file_type != 3 && file_type != 4 && file_type != 5) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::size_t present = data.size() > offset ? data.size() - offset : 0u;
        if (file_type != 2 && present < expected) {
            return PILLOW_C_OPEN_TRUNCATED;
        }

        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};

        if (file_type == 2) {
            // Pillow's sun_rle: literal bytes; 0x80 escapes a run -- the
            // next byte is the count; count 0 emits the 0x80 itself,
            // otherwise the following byte repeats count+1 times. EOF
            // inside a literal stretch -> "(0 bytes not processed)".
            std::size_t src = offset;
            std::size_t dst = 0;
            const std::size_t tight = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            bool hit_eof = false;
            while (dst < tight) {
                if (src >= data.size()) {
                    hit_eof = true;
                    break;
                }
                const std::uint8_t byte = data[src++];
                if (byte == 0x80) {
                    if (src >= data.size()) {
                        hit_eof = true;
                        break;
                    }
                    const std::uint8_t count = data[src++];
                    if (count == 0) {
                        image->pixels[dst++] = 0x80;
                    } else {
                        if (src >= data.size()) {
                            hit_eof = true;
                            break;
                        }
                        const std::uint8_t value = data[src++];
                        for (std::uint8_t i = 0; i <= count && dst < tight; ++i) {
                            image->pixels[dst++] = value;
                        }
                    }
                } else {
                    image->pixels[dst++] = byte;
                }
            }
            if (hit_eof) {
                delete image;
                return PILLOW_C_OPEN_RLE_TRUNCATED;
            }
            if (has_palette) {
                // Pillow stores the SUN palette with rawmode "RGB;L"
                // (plane major); de-interleave the R/G/B planes into the
                // facade's interleaved palette.
                const std::size_t entries = static_cast<std::size_t>(palette_length) / 3u;
                image->palette_rgb.assign(entries * 3u, std::uint8_t{0});
                const std::uint8_t* pal_src = data.data() + 32u;
                for (std::size_t i = 0u; i < entries; ++i) {
                    image->palette_rgb[i * 3u + 0u] = pal_src[i];
                    image->palette_rgb[i * 3u + 1u] = pal_src[entries + i];
                    image->palette_rgb[i * 3u + 2u] = pal_src[entries * 2u + i];
                }
                image->palette_alpha.clear();
                image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
            }
            *out_image = image;
            return PILLOW_C_OK;
        }

        // Raw modes: depth 1 -> inverted 1-bit (rawmode "1;I"), depth 4 ->
        // "L;4" (high nibble first), depth 8 -> L, depth 24/32 -> RGB/BGR
        // with the 32-bit X byte skipped (type 3 = RGB order).
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* row = data.data() + offset + static_cast<std::size_t>(y) * stride;
            if (depth == 1) {
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                for (int x = 0; x < width; ++x) {
                    const std::uint8_t byte = row[static_cast<std::size_t>(x) / 8u];
                    const bool bit = (byte & static_cast<std::uint8_t>(0x80u >> (x & 7))) != 0;
                    dst_row[x] = bit ? 0 : 255;
                }
            } else if (depth == 4) {
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                for (int x = 0; x < width; ++x) {
                    const std::uint8_t byte = row[static_cast<std::size_t>(x) / 2u];
                    const std::uint8_t nibble = (x & 1) ? static_cast<std::uint8_t>(byte & 15u) : static_cast<std::uint8_t>(byte >> 4);
                    dst_row[x] = static_cast<std::uint8_t>(nibble * 17u);
                }
            } else if (depth == 8) {
                std::memcpy(image->pixels.data() + static_cast<std::size_t>(y) * image_stride, row, static_cast<std::size_t>(width));
            } else {
                const bool rgb_order = file_type == 3;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                for (int x = 0; x < width; ++x) {
                    const std::size_t src = static_cast<std::size_t>(x) * (depth == 32 ? 4 : 3);
                    if (rgb_order) {
                        dst_row[static_cast<std::size_t>(x) * 3u + 0u] = row[src + 0u];
                        dst_row[static_cast<std::size_t>(x) * 3u + 1u] = row[src + 1u];
                        dst_row[static_cast<std::size_t>(x) * 3u + 2u] = row[src + 2u];
                    } else {
                        dst_row[static_cast<std::size_t>(x) * 3u + 0u] = row[src + 2u];
                        dst_row[static_cast<std::size_t>(x) * 3u + 1u] = row[src + 1u];
                        dst_row[static_cast<std::size_t>(x) * 3u + 2u] = row[src + 0u];
                    }
                }
            }
        }
        if (has_palette) {
            // Pillow stores the SUN palette with rawmode "RGB;L" (plane
            // major); the facade palette is interleaved RGB, so
            // de-interleave the R/G/B planes here.
            const std::size_t entries = static_cast<std::size_t>(palette_length) / 3u;
            image->palette_rgb.assign(entries * 3u, std::uint8_t{0});
            const std::uint8_t* src = data.data() + 32u;
            for (std::size_t i = 0u; i < entries; ++i) {
                image->palette_rgb[i * 3u + 0u] = src[i];
                image->palette_rgb[i * 3u + 1u] = src[entries + i];
                image->palette_rgb[i * 3u + 2u] = src[entries * 2u + i];
            }
            image->palette_alpha.clear();
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_gbr_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 20u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t header_size = read_be32(data.data());
        if (header_size < 20u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t version = read_be32(data.data() + 4);
        if (version != 1u && version != 2u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::int32_t width = static_cast<std::int32_t>(read_be32(data.data() + 8));
        const std::int32_t height = static_cast<std::int32_t>(read_be32(data.data() + 12));
        const std::uint32_t color_depth = read_be32(data.data() + 16);
        if (width <= 0 || height <= 0 || (color_depth != 1u && color_depth != 4u)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (version == 2u) {
            if (header_size < 28u || data.size() < 28u || std::memcmp(data.data() + 20, "GIMP", 4u) != 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        if (header_size > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint64_t data_size = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * color_depth;
        if (data.size() - header_size < data_size) {
            return PILLOW_C_OPEN_NOT_ENOUGH;
        }
        const int channels = color_depth == 1u ? 1 : 4;
        const int mode = color_depth == 1u ? PILLOW_C_MODE_L : PILLOW_C_MODE_RGBA;
        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        std::memcpy(image->pixels.data(), data.data() + header_size, static_cast<std::size_t>(data_size));
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_fits_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // Pillow's header walk: 80-byte records; SIMPLE/XTENSION start a
        // unit, END jumps to the 2880 boundary, and the first record that
        // is not a unit start after END breaks the loop. The data offset
        // mirrors Pillow's tell()-80 arithmetic including the sub-80-byte
        // payload quirk (offset = record_start + record_len - 80).
        int bitpix = 0;
        int naxis = 0;
        int naxis1 = 0;
        int naxis2 = 0;
        bool header_in_progress = false;
        bool saw_end = false;
        bool broke = false;
        std::size_t pos = 0u;
        std::size_t data_offset = 0u;
        while (pos < data.size()) {
            const std::size_t record_len = std::min<std::size_t>(80u, data.size() - pos);
            const std::uint8_t* record = data.data() + pos;
            std::size_t key_len = 0u;
            while (key_len < record_len && key_len < 8u && record[key_len] != ' ') {
                ++key_len;
            }
            const bool is_simple = key_len == 6u && std::memcmp(record, "SIMPLE", 6u) == 0;
            const bool is_xtension = key_len == 8u && std::memcmp(record, "XTENSION", 8u) == 0;
            const bool is_end = key_len == 3u && std::memcmp(record, "END", 3u) == 0;
            if (is_simple || is_xtension) {
                header_in_progress = true;
                if (saw_end) {
                    // A later header unit starts; its records are parsed
                    // with the current (single-HDU) bounded support.
                }
            } else if (is_end) {
                pos += record_len;
                const std::size_t remainder = pos % 2880u;
                if (remainder != 0u) {
                    pos += 2880u - remainder;
                }
                saw_end = true;
                header_in_progress = false;
                continue;
            } else if (saw_end && !header_in_progress) {
                data_offset = pos + record_len - 80u;
                broke = true;
                break;
            }
            if (!saw_end) {
                if (pos == 0u && !is_simple) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::uint8_t* value = record + 8u;
                std::size_t value_len = record_len > 8u ? record_len - 8u : 0u;
                const std::uint8_t* slash = static_cast<const std::uint8_t*>(std::memchr(value, '/', value_len));
                if (slash) {
                    value_len = static_cast<std::size_t>(slash - value);
                }
                while (value_len > 0u && (value[value_len - 1u] == ' ' || value[value_len - 1u] == '\t')) {
                    --value_len;
                }
                if (value_len > 0u && value[0] == '=') {
                    ++value;
                    --value_len;
                    while (value_len > 0u && (value[0] == ' ' || value[0] == '\t')) {
                        ++value;
                        --value_len;
                    }
                }
                if (is_simple && !(value_len == 1u && value[0] == 'T')) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                if (key_len == 6u && std::memcmp(record, "BITPIX", 6u) == 0) {
                    bool negative = false;
                    std::size_t i = 0u;
                    if (i < value_len && value[i] == '-') {
                        negative = true;
                        ++i;
                    }
                    bitpix = 0;
                    for (; i < value_len && value[i] >= '0' && value[i] <= '9'; ++i) {
                        bitpix = bitpix * 10 + (value[i] - '0');
                    }
                    if (negative) {
                        bitpix = -bitpix;
                    }
                } else if (key_len == 5u && std::memcmp(record, "NAXIS", 5u) == 0) {
                    naxis = 0;
                    for (std::size_t i = 0u; i < value_len && value[i] >= '0' && value[i] <= '9'; ++i) {
                        naxis = naxis * 10 + (value[i] - '0');
                    }
                } else if (key_len == 6u && std::memcmp(record, "NAXIS1", 6u) == 0) {
                    naxis1 = 0;
                    for (std::size_t i = 0u; i < value_len && value[i] >= '0' && value[i] <= '9'; ++i) {
                        naxis1 = naxis1 * 10 + (value[i] - '0');
                    }
                } else if (key_len == 6u && std::memcmp(record, "NAXIS2", 6u) == 0) {
                    naxis2 = 0;
                    for (std::size_t i = 0u; i < value_len && value[i] >= '0' && value[i] <= '9'; ++i) {
                        naxis2 = naxis2 * 10 + (value[i] - '0');
                    }
                }
            }
            pos += record_len;
        }
        if (!broke) {
            // EOF inside the header walk (Pillow: "Truncated FITS file"),
            // which also covers NAXIS = 0 (the walk keeps reading past
            // the END until EOF because no decoder was selected).
            return PILLOW_C_OPEN_FITS_TRUNCATED;
        }
        if (!saw_end) {
            return PILLOW_C_OPEN_FITS_NO_DATA;
        }
        if (naxis == 0 || naxis1 <= 0 || naxis2 <= 0) {
            return PILLOW_C_OPEN_FITS_TRUNCATED;
        }
        int mode = 0;
        std::size_t sample_bytes = 0u;
        if (bitpix == 8) {
            mode = PILLOW_C_MODE_L;
            sample_bytes = 1u;
        } else if (bitpix == 16) {
            mode = PILLOW_C_MODE_I16;
            sample_bytes = 2u;
        } else if (bitpix == 32) {
            mode = PILLOW_C_MODE_I;
            sample_bytes = 4u;
        } else if (bitpix == -32 || bitpix == -64) {
            mode = PILLOW_C_MODE_F;
            sample_bytes = bitpix == -32 ? 4u : 8u;
        } else {
            return PILLOW_C_OPEN_FITS_NO_DATA;
        }
        const std::uint64_t row_bytes = static_cast<std::uint64_t>(naxis1) * sample_bytes;
        const std::uint64_t expected = row_bytes * static_cast<std::uint64_t>(naxis2);
        const std::size_t present = data.size() > data_offset ? data.size() - data_offset : 0u;
        if (present < expected) {
            return PILLOW_C_OPEN_TRUNCATED;
        }
        // Numeric FITS modes keep the file's big-endian bytes verbatim
        // (Pillow's raw decoder copies them); rows are bottom-up.
        std::size_t tight_stride = 0u;
        std::size_t tight_size = 0u;
        if (!checked_image_size(naxis1 * static_cast<int>(sample_bytes), naxis2, 1, &tight_stride, &tight_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            naxis1,
            naxis2,
            mode,
            1,
            tight_stride,
            std::vector<std::uint8_t>(tight_size)};
        for (int y = 0; y < naxis2; ++y) {
            const int src_y = naxis2 - 1 - y;
            std::memcpy(image->pixels.data() + static_cast<std::size_t>(y) * tight_stride,
                        data.data() + data_offset + static_cast<std::size_t>(src_y) * static_cast<std::size_t>(row_bytes),
                        static_cast<std::size_t>(row_bytes));
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_xpm_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 9u || std::memcmp(data.data(), "/* XPM */", 9u) != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // Skip to the header line: "W H NCOLORS CPP at the line start.
        std::size_t pos = 9u;
        int width = 0;
        int height = 0;
        int palette_length = 0;
        int bpp = 0;
        bool found = false;
        while (pos < data.size()) {
            const std::size_t line_start = pos;
            while (pos < data.size() && data[pos] != '\n') {
                ++pos;
            }
            const std::size_t line_length = pos - line_start;
            const std::uint8_t* line = data.data() + line_start;
            if (line_length >= 2u && line[0] == '"' && line[1] >= '0' && line[1] <= '9') {
                const auto parse_int = [&](std::size_t* cursor) {
                    int value = 0;
                    while (*cursor < line_length && line[*cursor] >= '0' && line[*cursor] <= '9') {
                        value = value * 10 + (line[*cursor] - '0');
                        ++(*cursor);
                    }
                    return value;
                };
                std::size_t cursor = 1u;
                width = parse_int(&cursor);
                if (cursor < line_length && line[cursor] == ' ') {
                    ++cursor;
                    height = parse_int(&cursor);
                }
                if (cursor < line_length && line[cursor] == ' ') {
                    ++cursor;
                    palette_length = parse_int(&cursor);
                }
                if (cursor < line_length && line[cursor] == ' ') {
                    ++cursor;
                    bpp = parse_int(&cursor);
                }
                found = true;
            }
            if (pos < data.size()) {
                ++pos;
            }
            if (found) {
                break;
            }
        }
        if (!found || width <= 0 || height <= 0 || palette_length <= 0 || bpp <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        // Palette lines: "key c #RRGGBB", (strip 2 trailing chars), or
        // "None" -> transparency key. P mode keeps the entries in order.
        std::vector<std::string> keys;
        std::vector<std::uint8_t> palette_rgb;
        for (int i = 0; i < palette_length; ++i) {
            const std::size_t line_start = pos;
            while (pos < data.size() && data[pos] != '\n') {
                ++pos;
            }
            const std::size_t line_length = pos - line_start;
            if (pos < data.size()) {
                ++pos;
            }
            const std::uint8_t* line = data.data() + line_start;
            if (line_length < static_cast<std::size_t>(1u + bpp + 2u)) {
                return PILLOW_C_OPEN_XPM_BAD;
            }
            const std::string key(reinterpret_cast<const char*>(line) + 1, static_cast<std::size_t>(bpp));
            // Value section: from bpp+1 to len-2, split on whitespace.
            const std::uint8_t* section = line + static_cast<std::size_t>(bpp) + 1u;
            const std::size_t section_length = line_length - static_cast<std::size_t>(bpp) - 1u - 2u;
            bool has_c = false;
            std::size_t cursor = 0u;
            std::string color;
            while (cursor < section_length) {
                while (cursor < section_length && (section[cursor] == ' ' || section[cursor] == '\t')) {
                    ++cursor;
                }
                const std::size_t word_start = cursor;
                while (cursor < section_length && section[cursor] != ' ' && section[cursor] != '\t') {
                    ++cursor;
                }
                if (cursor == word_start) {
                    break;
                }
                const std::string word(reinterpret_cast<const char*>(section) + word_start, cursor - word_start);
                if (word == "c") {
                    if (cursor >= section_length) {
                        return PILLOW_C_OPEN_XPM_BAD;
                    }
                    while (cursor < section_length && (section[cursor] == ' ' || section[cursor] == '\t')) {
                        ++cursor;
                    }
                    const std::size_t color_start = cursor;
                    while (cursor < section_length && section[cursor] != ' ' && section[cursor] != '\t') {
                        ++cursor;
                    }
                    color = std::string(reinterpret_cast<const char*>(section) + color_start, cursor - color_start);
                    has_c = true;
                    break;
                }
            }
            if (!has_c) {
                return PILLOW_C_OPEN_XPM_BAD;
            }
            if (color == "None") {
                keys.push_back(key);
                palette_rgb.insert(palette_rgb.end(), {0u, 0u, 0u});
            } else if (color.size() >= 2u && color[0] == '#') {
                unsigned int rgb = 0u;
                bool ok = true;
                for (std::size_t c = 1u; c < color.size(); ++c) {
                    const char ch = color[c];
                    unsigned int nibble = 0u;
                    if (ch >= '0' && ch <= '9') {
                        nibble = static_cast<unsigned int>(ch - '0');
                    } else if (ch >= 'a' && ch <= 'f') {
                        nibble = static_cast<unsigned int>(ch - 'a' + 10);
                    } else if (ch >= 'A' && ch <= 'F') {
                        nibble = static_cast<unsigned int>(ch - 'A' + 10);
                    } else {
                        ok = false;
                        break;
                    }
                    rgb = rgb * 16u + nibble;
                }
                if (!ok) {
                    return PILLOW_C_OPEN_XPM_BAD;
                }
                keys.push_back(key);
                palette_rgb.push_back(static_cast<std::uint8_t>((rgb >> 16) & 255u));
                palette_rgb.push_back(static_cast<std::uint8_t>((rgb >> 8) & 255u));
                palette_rgb.push_back(static_cast<std::uint8_t>(rgb & 255u));
            } else {
                return PILLOW_C_OPEN_XPM_BAD;
            }
        }

        const bool indexed = palette_length <= 256;
        const int channels = indexed ? 1 : 3;
        const int mode = indexed ? PILLOW_C_MODE_P : PILLOW_C_MODE_RGB;
        std::size_t image_stride = 0u;
        std::size_t image_size = 0u;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};

        // Pixel rows: skip one "/* pixels */" line (first only), then read
        // lines until the output is full; each line keeps the segments
        // between quotes; keys are bpp chars each. EOF with a short
        // output -> "not enough image data".
        bool skipped_marker = false;
        std::size_t dst = 0u;
        const std::size_t dest_length = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(channels);
        bool eof = false;
        while (dst < dest_length) {
            if (pos >= data.size()) {
                eof = true;
                break;
            }
            const std::size_t line_start = pos;
            while (pos < data.size() && data[pos] != '\n') {
                ++pos;
            }
            const std::size_t line_length = pos - line_start;
            if (pos < data.size()) {
                ++pos;
            }
            const std::uint8_t* line = data.data() + line_start;
            // rstrip == "/* pixels */"
            std::size_t stripped = line_length;
            while (stripped > 0u && (line[stripped - 1u] == ' ' || line[stripped - 1u] == '\t' || line[stripped - 1u] == '\r')) {
                --stripped;
            }
            if (!skipped_marker && stripped == 12u && std::memcmp(line, "/* pixels */", 12u) == 0) {
                skipped_marker = true;
                continue;
            }
            // Concatenate the segments between quotes.
            std::vector<std::uint8_t> joined;
            bool inside = false;
            for (std::size_t i = 0u; i < line_length; ++i) {
                if (line[i] == '"') {
                    inside = !inside;
                } else if (inside) {
                    joined.push_back(line[i]);
                }
            }
            for (std::size_t i = 0u; i + static_cast<std::size_t>(bpp) <= joined.size() && dst < dest_length; i += static_cast<std::size_t>(bpp)) {
                const std::string key(reinterpret_cast<const char*>(joined.data()) + i, static_cast<std::size_t>(bpp));
                if (indexed) {
                    std::size_t index = 0u;
                    bool found_key = false;
                    for (std::size_t k = 0u; k < keys.size(); ++k) {
                        if (keys[k] == key) {
                            index = k;
                            found_key = true;
                            break;
                        }
                    }
                    if (!found_key) {
                        delete image;
                        return PILLOW_C_OPEN_XPM_KEY;
                    }
                    image->pixels[dst++] = static_cast<std::uint8_t>(index);
                } else {
                    std::size_t index = 0u;
                    bool found_key = false;
                    for (std::size_t k = 0u; k < keys.size(); ++k) {
                        if (keys[k] == key) {
                            index = k;
                            found_key = true;
                            break;
                        }
                    }
                    if (!found_key) {
                        delete image;
                        return PILLOW_C_OPEN_XPM_RGB_KEY;
                    }
                    image->pixels[dst++] = palette_rgb[index * 3u + 0u];
                    image->pixels[dst++] = palette_rgb[index * 3u + 1u];
                    image->pixels[dst++] = palette_rgb[index * 3u + 2u];
                }
            }
        }
        if (eof && dst < dest_length) {
            delete image;
            return PILLOW_C_OPEN_NOT_ENOUGH;
        }
        if (indexed) {
            image->palette_rgb = palette_rgb;
            image->palette_alpha.clear();
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// ---------------------------------------------------------------------------
// BEHAV-OPEN-004: PSD opener (Pillow's PsdImageFile, base image only).
// ---------------------------------------------------------------------------

int open_psd_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 26u || std::memcmp(data.data(), "8BPS", 4u) != 0 || read_be16(data.data() + 4) != 1) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int psd_channels = read_be16(data.data() + 12);
        const std::int32_t height = static_cast<std::int32_t>(read_be32(data.data() + 14));
        const std::int32_t width = static_cast<std::int32_t>(read_be32(data.data() + 18));
        const int bits = read_be16(data.data() + 22);
        const int psd_mode = read_be16(data.data() + 24);

        int mode = 0;
        int channels = 0;
        if (psd_mode == 0 && bits == 1) {
            mode = PILLOW_C_MODE_1;
            channels = 1;
        } else if ((psd_mode == 0 || psd_mode == 1 || psd_mode == 7 || psd_mode == 8) && bits == 8) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
        } else if (psd_mode == 2 && bits == 8) {
            mode = PILLOW_C_MODE_P;
            channels = 1;
        } else if (psd_mode == 3 && bits == 8) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
        } else if (psd_mode == 4 && bits == 8) {
            mode = PILLOW_C_MODE_CMYK;
            channels = 4;
        } else if (psd_mode == 9 && bits == 8) {
            mode = PILLOW_C_MODE_LAB;
            channels = 3;
        } else {
            // Pillow's MODES KeyError becomes a SyntaxError inside
            // ImageFile.__init__ -> the identification error.
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (channels > psd_channels) {
            return PILLOW_C_OPEN_PSD_CHANNELS;
        }
        if (mode == PILLOW_C_MODE_RGB && psd_channels == 4) {
            mode = PILLOW_C_MODE_RGBA;
            channels = 4;
        }
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t pos = 26u;
        std::vector<std::uint8_t> palette_rgb;
        const auto read_be32_at = [&data](std::size_t at) -> std::uint32_t {
            return read_be32(data.data() + at);
        };
        // Color mode data.
        if (pos + 4u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::uint32_t color_size = read_be32_at(pos);
        pos += 4u;
        if (color_size) {
            if (pos + color_size > data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (mode == PILLOW_C_MODE_P && color_size == 768u) {
                // Plane-major RGB;L palette -> de-interleave.
                palette_rgb.assign(768u, std::uint8_t{0});
                for (int i = 0; i < 256; ++i) {
                    palette_rgb[static_cast<std::size_t>(i) * 3u + 0u] = data[pos + static_cast<std::size_t>(i)];
                    palette_rgb[static_cast<std::size_t>(i) * 3u + 1u] = data[pos + 256u + static_cast<std::size_t>(i)];
                    palette_rgb[static_cast<std::size_t>(i) * 3u + 2u] = data[pos + 512u + static_cast<std::size_t>(i)];
                }
            }
            pos += color_size;
        }
        // Image resources.
        if (pos + 4u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::uint32_t resource_size = read_be32_at(pos);
        pos += 4u;
        if (resource_size) {
            if (pos + resource_size > data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            pos += resource_size;
        }
        // Layer and mask information (skipped; layers stay a child).
        if (pos + 4u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::uint32_t layer_size = read_be32_at(pos);
        pos += 4u;
        if (layer_size) {
            if (pos + layer_size > data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            pos += layer_size;
        }
        // Image descriptor.
        if (pos + 2u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int compression = read_be16(data.data() + pos);
        pos += 2u;

        std::size_t image_stride = 0u;
        std::size_t image_size = 0u;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};

        const std::uint64_t plane_size = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        const std::uint64_t stored_plane = mode == PILLOW_C_MODE_1 ? (plane_size + 7u) / 8u : plane_size;
        std::vector<std::size_t> channel_offsets(static_cast<std::size_t>(channels), 0u);
        std::vector<std::size_t> channel_sizes(static_cast<std::size_t>(channels), 0u);
        if (compression == 0) {
            for (int c = 0; c < channels; ++c) {
                channel_offsets[static_cast<std::size_t>(c)] = pos;
                channel_sizes[static_cast<std::size_t>(c)] = static_cast<std::size_t>(stored_plane);
                pos += static_cast<std::size_t>(stored_plane);
            }
        } else if (compression == 1) {
            // PackBits: channels*height BE16 row byte counts, then the
            // per-channel row streams at the accumulated offsets.
            const std::size_t bytecounts = static_cast<std::size_t>(channels) * static_cast<std::size_t>(height) * 2u;
            if (pos + bytecounts > data.size()) {
                delete image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::size_t cursor = pos + bytecounts;
            for (int c = 0; c < channels; ++c) {
                std::size_t total = 0u;
                for (int y = 0; y < height; ++y) {
                    const std::size_t bc = static_cast<std::size_t>(c) * static_cast<std::size_t>(height) * 2u + static_cast<std::size_t>(y) * 2u;
                    total += read_be16(data.data() + pos + bc);
                }
                channel_offsets[static_cast<std::size_t>(c)] = cursor;
                channel_sizes[static_cast<std::size_t>(c)] = total;
                cursor += total;
            }
            if (cursor > data.size()) {
                delete image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
        } else {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }

        // Decode the planes into interleaved storage.
        for (int c = 0; c < channels; ++c) {
            const std::size_t channel_offset = channel_offsets[static_cast<std::size_t>(c)];
            const std::size_t channel_size = channel_sizes[static_cast<std::size_t>(c)];
            if (channel_offset > data.size() || channel_size > data.size() - channel_offset) {
                delete image;
                return PILLOW_C_OPEN_TRUNCATED;
            }
            if (compression == 0) {
                if (channel_size < stored_plane) {
                    delete image;
                    return PILLOW_C_OPEN_TRUNCATED;
                }
                for (int y = 0; y < height; ++y) {
                    const std::size_t packed_row = (static_cast<std::size_t>(width) + 7u) / 8u;
                    const std::uint8_t* src = data.data() + channel_offset + static_cast<std::size_t>(y) * (mode == PILLOW_C_MODE_1 ? packed_row : static_cast<std::size_t>(width));
                    if (mode == PILLOW_C_MODE_1) {
                        // Packed 1-bit rows (MSB first).
                        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                        for (int x = 0; x < width; ++x) {
                            const std::uint8_t byte = src[static_cast<std::size_t>(x) / 8u];
                            dst_row[x] = (byte & static_cast<std::uint8_t>(0x80u >> (x & 7))) ? 255 : 0;
                        }
                    } else {
                        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                        for (int x = 0; x < width; ++x) {
                            std::uint8_t value = src[x];
                            if (mode == PILLOW_C_MODE_CMYK) {
                                value = static_cast<std::uint8_t>(255 - value);
                            }
                            // LAB: the facade's storage keeps the signed
                            // a/b form and ToBytes XORs 0x80, so store the
                            // file bytes verbatim here.
                            dst_row[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] = value;
                        }
                    }
                }
            } else {
                // PackBits rows: header n -> 0..127: n+1 literals,
                // 129..255: 257-n repeats of the next byte, 128: no-op.
                std::size_t src = channel_offset;
                std::size_t produced = 0u;
                const std::size_t produce_limit = mode == PILLOW_C_MODE_1 ? static_cast<std::size_t>(stored_plane) : static_cast<std::size_t>(plane_size);
                bool broken = false;
                std::vector<std::uint8_t> packed;
                if (mode == PILLOW_C_MODE_1) {
                    packed.reserve(static_cast<std::size_t>(stored_plane));
                }
                while (produced < produce_limit) {
                    if (src >= channel_offset + channel_size) {
                        broken = true;
                        break;
                    }
                    const std::uint8_t n = data[src++];
                    if (n <= 127u) {
                        const std::size_t count = static_cast<std::size_t>(n) + 1u;
                        for (std::size_t i = 0u; i < count && produced < produce_limit; ++i) {
                            if (src >= channel_offset + channel_size) {
                                broken = true;
                                break;
                            }
                            const std::uint8_t value = data[src++];
                            if (mode == PILLOW_C_MODE_1) {
                                packed.push_back(value);
                            } else {
                                const int x = static_cast<int>(produced % static_cast<std::uint64_t>(width));
                                const int y = static_cast<int>(produced / static_cast<std::uint64_t>(width));
                                std::uint8_t* dst = image->pixels.data() + static_cast<std::size_t>(y) * image_stride + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c);
                                if (mode == PILLOW_C_MODE_CMYK) {
                                    *dst = static_cast<std::uint8_t>(255 - value);
                                } else {
                                    *dst = value;
                                }
                            }
                            ++produced;
                        }
                        if (broken) {
                            break;
                        }
                    } else if (n >= 129u) {
                        const std::size_t count = 257u - n;
                        if (src >= channel_offset + channel_size) {
                            broken = true;
                            break;
                        }
                        const std::uint8_t value = data[src++];
                        for (std::size_t i = 0u; i < count && produced < produce_limit; ++i) {
                            if (mode == PILLOW_C_MODE_1) {
                                packed.push_back(value);
                            } else {
                                const int x = static_cast<int>(produced % static_cast<std::uint64_t>(width));
                                const int y = static_cast<int>(produced / static_cast<std::uint64_t>(width));
                                std::uint8_t* dst = image->pixels.data() + static_cast<std::size_t>(y) * image_stride + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c);
                                if (mode == PILLOW_C_MODE_CMYK) {
                                    *dst = static_cast<std::uint8_t>(255 - value);
                                } else {
                                    *dst = value;
                                }
                            }
                            ++produced;
                        }
                    }
                    // 128 = no-op.
                }
                if (broken) {
                    delete image;
                    return PILLOW_C_OPEN_TRUNCATED;
                }
                if (mode == PILLOW_C_MODE_1) {
                    for (std::size_t i = 0u; i < packed.size(); ++i) {
                        const int x = static_cast<int>((i % static_cast<std::uint64_t>(width)) * 8u);
                        const int y = static_cast<int>(i / static_cast<std::uint64_t>(width));
                        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                        for (int bit = 0; bit < 8 && x + bit < width; ++bit) {
                            dst_row[x + bit] = (packed[i] & static_cast<std::uint8_t>(0x80u >> bit)) ? 255 : 0;
                        }
                    }
                }
            }
        }

        if (!palette_rgb.empty()) {
            image->palette_rgb = std::move(palette_rgb);
            image->palette_alpha.clear();
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// BEHAV-OPEN-005: FLI/FLC status codes local to the FLI open route. The
// facade maps each one to Pillow 11.3.0's exact error message. -51 lets
// the facade recover Pillow's per-file "bytes not processed" count through
// pillow_c_image_fli_truncation_count.
constexpr int PILLOW_C_OPEN_FLI_OVERRUN = -48;
constexpr int PILLOW_C_OPEN_FLI_UNKNOWN = -49;
constexpr int PILLOW_C_OPEN_FLI_BROKEN = -50;
constexpr int PILLOW_C_OPEN_FLI_TRUNCATED = -51;

namespace {
// Mirrors Pillow's ImagingFliDecode chunk accounting for frame 0, including
// the exact out-of-bounds checks and the COPY-chunk "not enough data"
// consumed-bytes path. pixels may be null when only the status/truncation
// count is needed (the count is determined before any pixel writes matter).
int decode_fli_frame(
    const std::vector<std::uint8_t>& data,
    int width,
    int height,
    std::vector<std::uint8_t>* pixels,
    std::int64_t* truncation_count)
{
    constexpr std::size_t frame_pos = 128u;
    if (data.size() < frame_pos + 4u) {
        if (truncation_count) {
            *truncation_count = static_cast<std::int64_t>(data.size()) - 128;
        }
        return PILLOW_C_OPEN_FLI_TRUNCATED;
    }
    const std::uint64_t framesize = read_le32(data.data() + frame_pos);
    const std::int64_t avail = static_cast<std::int64_t>(data.size()) - 128;
    if (avail < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint64_t avail_u = static_cast<std::uint64_t>(avail);
    if (avail_u + (avail_u & 1u) < framesize) {
        if (truncation_count) {
            *truncation_count = avail;
        }
        return PILLOW_C_OPEN_FLI_TRUNCATED;
    }
    if (data.size() < frame_pos + 8u) {
        return PILLOW_C_OPEN_FLI_OVERRUN;
    }
    if (read_le16(data.data() + frame_pos + 4u) != 0xF1FA) {
        return PILLOW_C_OPEN_FLI_UNKNOWN;
    }
    const std::uint64_t frame_end = frame_pos + framesize;
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    const int chunks = read_le16(data.data() + frame_pos + 6u);
    std::uint64_t ptr = frame_pos + 16u;
    std::uint64_t remaining = framesize - 16u;
    std::uint8_t* out = pixels ? pixels->data() : nullptr;
    for (int c = 0; c < chunks; ++c) {
        if (remaining < 10u) {
            return PILLOW_C_OPEN_FLI_OVERRUN;
        }
        std::uint64_t d = ptr + 6u;
        const int chunk_type = read_le16(data.data() + ptr + 4u);
        const auto data_oob = [&](std::uint64_t off) -> bool {
            return d + off > frame_end;
        };
        switch (chunk_type) {
        case 4:
        case 11:
        case 18:
            // COLOR/COLOR_256/PSTAMP chunks: ignored by the C decoder
            // (the palette was parsed during the header walk).
            break;
        case 13:
            // BLACK: clear the frame.
            if (pixels) {
                std::memset(out, 0, static_cast<std::size_t>(pixel_count));
            }
            break;
        case 7: {
            // SS2 word-delta chunk (FLC).
            int lines = read_le16(data.data() + d);
            d += 2u;
            int l = 0;
            int y = 0;
            while (l < lines && y < height) {
                std::uint8_t* row = out ? out + static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(width) : nullptr;
                if (data_oob(2u)) {
                    return PILLOW_C_OPEN_FLI_OVERRUN;
                }
                int packets = read_le16(data.data() + d);
                d += 2u;
                while (packets & 0x8000) {
                    if (packets & 0x4000) {
                        y += 65536 - packets;
                        if (y >= height) {
                            return PILLOW_C_OPEN_FLI_OVERRUN;
                        }
                        row = out ? out + static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(width) : nullptr;
                    } else {
                        if (row) {
                            row[width - 1] = static_cast<std::uint8_t>(packets);
                        }
                    }
                    if (data_oob(2u)) {
                        return PILLOW_C_OPEN_FLI_OVERRUN;
                    }
                    packets = read_le16(data.data() + d);
                    d += 2u;
                }
                int p = 0;
                int x = 0;
                for (p = 0; p < packets; ++p) {
                    if (data_oob(2u)) {
                        return PILLOW_C_OPEN_FLI_OVERRUN;
                    }
                    x += data[d];
                    if (data[d + 1u] >= 128u) {
                        if (data_oob(4u)) {
                            return PILLOW_C_OPEN_FLI_OVERRUN;
                        }
                        const int i = 256 - data[d + 1u];
                        if (x + i + i > width) {
                            break;
                        }
                        if (row) {
                            for (int j = 0; j < i; ++j) {
                                row[x++] = data[d + 2u];
                                row[x++] = data[d + 3u];
                            }
                        }
                        d += 4u;
                    } else {
                        const int i = 2 * static_cast<int>(data[d + 1u]);
                        if (x + i > width) {
                            break;
                        }
                        if (data_oob(static_cast<std::uint64_t>(2 + i))) {
                            return PILLOW_C_OPEN_FLI_OVERRUN;
                        }
                        if (row) {
                            std::memcpy(row + x, data.data() + d + 2u, static_cast<std::size_t>(i));
                        }
                        d += static_cast<std::uint64_t>(2 + i);
                        x += i;
                    }
                }
                if (p < packets) {
                    break;
                }
                ++l;
                ++y;
            }
            if (l < lines) {
                return PILLOW_C_OPEN_FLI_OVERRUN;
            }
            break;
        }
        case 12: {
            // LC byte-delta chunk.
            int y = read_le16(data.data() + d);
            const int ymax = y + read_le16(data.data() + d + 2u);
            d += 4u;
            for (; y < ymax && y < height; ++y) {
                std::uint8_t* row = out ? out + static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(width) : nullptr;
                if (data_oob(1u)) {
                    return PILLOW_C_OPEN_FLI_OVERRUN;
                }
                const int packets = data[d++];
                int p = 0;
                int x = 0;
                int i = 0;
                for (p = 0; p < packets; ++p, x += i) {
                    if (data_oob(2u)) {
                        return PILLOW_C_OPEN_FLI_OVERRUN;
                    }
                    x += data[d];
                    if (data[d + 1u] & 0x80u) {
                        i = 256 - data[d + 1u];
                        if (x + i > width) {
                            break;
                        }
                        if (data_oob(3u)) {
                            return PILLOW_C_OPEN_FLI_OVERRUN;
                        }
                        if (row) {
                            std::memset(row + x, data[d + 2u], static_cast<std::size_t>(i));
                        }
                        d += 3u;
                    } else {
                        i = data[d + 1u];
                        if (x + i > width) {
                            break;
                        }
                        if (data_oob(static_cast<std::uint64_t>(2 + i))) {
                            return PILLOW_C_OPEN_FLI_OVERRUN;
                        }
                        if (row) {
                            std::memcpy(row + x, data.data() + d + 2u, static_cast<std::size_t>(i));
                        }
                        d += static_cast<std::uint64_t>(i + 2);
                    }
                }
                if (p < packets) {
                    break;
                }
            }
            if (y < ymax) {
                return PILLOW_C_OPEN_FLI_OVERRUN;
            }
            break;
        }
        case 15: {
            // BRUN byte-run chunk.
            for (int y = 0; y < height; ++y) {
                std::uint8_t* row = out ? out + static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(width) : nullptr;
                d += 1u;
                int x = 0;
                int i = 0;
                for (x = 0; x < width; x += i) {
                    if (data_oob(2u)) {
                        return PILLOW_C_OPEN_FLI_OVERRUN;
                    }
                    if (data[d] & 0x80u) {
                        i = 256 - data[d];
                        if (x + i > width) {
                            break;
                        }
                        if (data_oob(static_cast<std::uint64_t>(i + 1))) {
                            return PILLOW_C_OPEN_FLI_OVERRUN;
                        }
                        if (row) {
                            std::memcpy(row + x, data.data() + d + 1u, static_cast<std::size_t>(i));
                        }
                        d += static_cast<std::uint64_t>(i + 1);
                    } else {
                        i = data[d];
                        if (x + i > width) {
                            break;
                        }
                        if (row) {
                            std::memset(row + x, data[d + 1u], static_cast<std::size_t>(i));
                        }
                        d += 2u;
                    }
                }
                if (x != width) {
                    return PILLOW_C_OPEN_FLI_OVERRUN;
                }
            }
            break;
        }
        case 16: {
            // COPY chunk.
            if (INT32_MAX < pixel_count) {
                return PILLOW_C_OPEN_FLI_OVERRUN;
            }
            if (d + pixel_count > frame_end) {
                if (truncation_count) {
                    *truncation_count = static_cast<std::int64_t>(framesize) - static_cast<std::int64_t>(ptr - frame_pos);
                }
                return PILLOW_C_OPEN_FLI_TRUNCATED;
            }
            if (pixels) {
                std::memcpy(out, data.data() + d, static_cast<std::size_t>(pixel_count));
            }
            break;
        }
        default:
            return PILLOW_C_OPEN_FLI_UNKNOWN;
        }
        const std::uint64_t advance = read_le32(data.data() + ptr);
        if (advance == 0u) {
            return PILLOW_C_OPEN_FLI_BROKEN;
        }
        if (advance > INT32_MAX || advance > remaining) {
            return PILLOW_C_OPEN_FLI_OVERRUN;
        }
        ptr += advance;
        remaining -= advance;
    }
    return PILLOW_C_OK;
}
} // namespace

int open_fli_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // FliImagePlugin._open: 128-byte header, magic at 4 and the
        // zero field at 20:22 gate identification.
        if (data.size() < 128u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int magic = read_le16(data.data() + 4u);
        if ((magic != 0xAF11 && magic != 0xAF12) || data[20] != 0u || data[21] != 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // seek(0) inside _open runs _seek_check against n_frames; a zero
        // count raises EOFError, which ImageFile.__init__ wraps into
        // SyntaxError, so identification fails.
        if (read_le16(data.data() + 6u) == 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = read_le16(data.data() + 8u);
        const int height = read_le16(data.data() + 10u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, 1, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        // Palette walk (mirrors FliImagePlugin._open): skip an optional
        // F100 prefix chunk, then scan the first frame's subchunks for a
        // COLOR (shift 0) or COLOR_256 (shift 2) chunk.
        std::vector<std::uint8_t> palette_rgb(768u);
        for (int i = 0; i < 256; ++i) {
            palette_rgb[static_cast<std::size_t>(i) * 3u + 0u] = static_cast<std::uint8_t>(i);
            palette_rgb[static_cast<std::size_t>(i) * 3u + 1u] = static_cast<std::uint8_t>(i);
            palette_rgb[static_cast<std::size_t>(i) * 3u + 2u] = static_cast<std::uint8_t>(i);
        }
        std::size_t walk = 128u;
        auto have = [&](std::size_t pos, std::size_t n) -> bool {
            return pos <= data.size() && n <= data.size() - pos;
        };
        if (!have(walk, 16u)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::uint16_t head_type = read_le16(data.data() + walk + 4u);
        if (head_type == 0xF100u) {
            walk = 128u + static_cast<std::size_t>(read_le32(data.data() + walk));
            if (!have(walk, 16u)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            head_type = read_le16(data.data() + walk + 4u);
        }
        if (head_type == 0xF1FAu) {
            const int nsub = read_le16(data.data() + walk + 6u);
            std::size_t cursor = walk + 16u;
            std::int64_t chunk_size = -1;
            for (int i = 0; i < nsub; ++i) {
                if (chunk_size >= 0) {
                    if (chunk_size < 6) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    cursor += static_cast<std::size_t>(chunk_size) - 6u;
                }
                if (!have(cursor, 6u)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const int chunk_type = read_le16(data.data() + cursor + 4u);
                if (chunk_type == 4 || chunk_type == 11) {
                    const int shift = chunk_type == 11 ? 2 : 0;
                    std::size_t pd = cursor + 6u;
                    if (!have(pd, 2u)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    const int entry_count = read_le16(data.data() + pd);
                    pd += 2u;
                    int palette_index = 0;
                    for (int e = 0; e < entry_count; ++e) {
                        if (!have(pd, 2u)) {
                            return PILLOW_C_INVALID_ARGUMENT;
                        }
                        palette_index += data[pd];
                        int n = data[pd + 1u];
                        if (n == 0) {
                            n = 256;
                        }
                        pd += 2u;
                        const std::size_t rgb_bytes = static_cast<std::size_t>(n) * 3u;
                        if (!have(pd, rgb_bytes)) {
                            return PILLOW_C_INVALID_ARGUMENT;
                        }
                        for (int k = 0; k < n; ++k) {
                            const int idx = palette_index + k;
                            if (idx < 0 || idx >= 256) {
                                return PILLOW_C_INVALID_ARGUMENT;
                            }
                            palette_rgb[static_cast<std::size_t>(idx) * 3u + 0u] = static_cast<std::uint8_t>(data[pd] << shift);
                            palette_rgb[static_cast<std::size_t>(idx) * 3u + 1u] = static_cast<std::uint8_t>(data[pd + 1u] << shift);
                            palette_rgb[static_cast<std::size_t>(idx) * 3u + 2u] = static_cast<std::uint8_t>(data[pd + 2u] << shift);
                            pd += 3u;
                        }
                    }
                    break;
                }
                chunk_size = read_le32(data.data() + cursor);
                if (chunk_size == 0) {
                    break;
                }
            }
        }

        std::vector<std::uint8_t> pixels(image_size, 0u);
        std::int64_t truncation_count = 0;
        const int decode_status = decode_fli_frame(data, width, height, &pixels, &truncation_count);
        if (decode_status != PILLOW_C_OK) {
            return decode_status;
        }

        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_P,
            1,
            image_stride,
            std::move(pixels)};
        image->palette_rgb = std::move(palette_rgb);
        image->palette_alpha.clear();
        image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int fli_truncation_count(const char* path, std::int64_t* out_count)
{
    if (!path || !out_count) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_count = 0;
    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 128u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = read_le16(data.data() + 8u);
        const int height = read_le16(data.data() + 10u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::int64_t count = 0;
        const int status = decode_fli_frame(data, width, height, nullptr, &count);
        if (status != PILLOW_C_OPEN_FLI_TRUNCATED) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_count = count;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// BEHAV-OPEN-006: MIC status codes local to the MIC open route. -52 is
// Pillow's olefile ValueError that escapes Image.open unwrapped when the
// file is exactly the 512-byte CFB header with a valid magic.
constexpr int PILLOW_C_OPEN_MIC_HEADER_ONLY = -52;

namespace {

constexpr std::uint32_t MIC_ENDOFCHAIN = 0xFFFFFFFEu;
constexpr std::uint32_t MIC_FREESECT = 0xFFFFFFFFu;

struct MicDirectoryEntry {
    int type = 0; // 1 storage, 2 stream, 5 root
    std::string name;
    std::string name_lower;
    std::uint32_t left = MIC_FREESECT;
    std::uint32_t right = MIC_FREESECT;
    std::uint32_t child = MIC_FREESECT;
    std::uint32_t start = MIC_FREESECT;
    std::uint64_t size = 0;
};

std::string mic_decode_utf16le(const std::uint8_t* p, std::size_t byte_len)
{
    std::string out;
    out.reserve(byte_len / 2u);
    for (std::size_t i = 0; i + 1u < byte_len; i += 2u) {
        const std::uint16_t u = static_cast<std::uint16_t>(p[i] | (static_cast<std::uint16_t>(p[i + 1u]) << 8));
        if (u == 0) {
            break;
        }
        if (u < 0x80u) {
            out.push_back(static_cast<char>(u));
        } else {
            out.push_back(static_cast<char>(0xC0u | (u >> 6)));
            out.push_back(static_cast<char>(0x80u | (u & 0x3Fu)));
        }
    }
    return out;
}

std::string mic_ascii_lower(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

std::uint64_t mic_read_le64(const std::uint8_t* p)
{
    return static_cast<std::uint64_t>(read_le32(p)) | (static_cast<std::uint64_t>(read_le32(p + 4u)) << 32);
}

bool mic_chain_fat(
    const std::vector<std::uint32_t>& fat,
    std::uint32_t start,
    std::size_t max_sectors,
    std::vector<std::uint32_t>* out_ids)
{
    std::uint32_t cur = start;
    std::size_t guard = 0;
    while (cur != MIC_ENDOFCHAIN && cur != MIC_FREESECT) {
        if (++guard > max_sectors) {
            return false;
        }
        if (static_cast<std::size_t>(cur) >= fat.size()) {
            return false;
        }
        out_ids->push_back(cur);
        cur = fat[static_cast<std::size_t>(cur)];
    }
    return true;
}

// the embedded Image stream decodes through the exported TIFF route
extern "C" int pillow_c_image_open_tiff(const char* path, PillowCImage** out_image);

int open_mic_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        constexpr std::size_t header_size = 512u;
        if (data.size() < 8u ||
            data[0] != 0xD0u || data[1] != 0xCFu || data[2] != 0x11u || data[3] != 0xE0u ||
            data[4] != 0xA1u || data[5] != 0xB1u || data[6] != 0x1Au || data[7] != 0xE1u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() == header_size) {
            return PILLOW_C_OPEN_MIC_HEADER_ONLY;
        }
        if (data.size() < header_size) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (read_le16(data.data() + 30u) != 9 || read_le16(data.data() + 32u) != 6) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        constexpr std::size_t sector_size = 512u;
        constexpr std::size_t mini_size = 64u;
        const std::uint32_t dir_start = read_le32(data.data() + 48u);
        const std::uint32_t cutoff = read_le32(data.data() + 56u);
        const std::uint32_t minifat_start = read_le32(data.data() + 60u);
        const std::uint32_t nminifat = read_le32(data.data() + 64u);
        const std::uint32_t difat_start = read_le32(data.data() + 68u);
        const std::uint32_t ndifat = read_le32(data.data() + 72u);

        const auto sector_at = [&](std::uint32_t id, const std::uint8_t** out) -> bool {
            const std::uint64_t off = header_size + static_cast<std::uint64_t>(id) * sector_size;
            if (off > data.size() || sector_size > data.size() - off) {
                return false;
            }
            *out = data.data() + off;
            return true;
        };

        std::vector<std::uint32_t> fat_ids;
        for (int i = 0; i < 109; ++i) {
            const std::uint32_t id = read_le32(data.data() + 76u + 4u * static_cast<std::size_t>(i));
            if (id == MIC_ENDOFCHAIN || id == MIC_FREESECT) {
                break;
            }
            fat_ids.push_back(id);
        }
        std::uint32_t difat_next = difat_start;
        std::size_t difat_guard = 0;
        while (ndifat > 0 && difat_next != MIC_ENDOFCHAIN && difat_next != MIC_FREESECT) {
            if (++difat_guard > 1024u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint8_t* sec = nullptr;
            if (!sector_at(difat_next, &sec)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t per_sector = sector_size / 4u - 1u;
            for (std::size_t i = 0; i < per_sector; ++i) {
                const std::uint32_t id = read_le32(sec + 4u * i);
                if (id == MIC_ENDOFCHAIN || id == MIC_FREESECT) {
                    break;
                }
                fat_ids.push_back(id);
            }
            difat_next = read_le32(sec + 4u * per_sector);
        }

        std::vector<std::uint32_t> fat;
        fat.reserve(fat_ids.size() * (sector_size / 4u));
        for (const std::uint32_t id : fat_ids) {
            const std::uint8_t* sec = nullptr;
            if (!sector_at(id, &sec)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            for (std::size_t i = 0; i < sector_size / 4u; ++i) {
                fat.push_back(read_le32(sec + 4u * i));
            }
        }

        std::vector<MicDirectoryEntry> entries;
        std::uint32_t ds = dir_start;
        std::size_t dir_guard = 0;
        while (ds != MIC_ENDOFCHAIN && ds != MIC_FREESECT) {
            if (++dir_guard > 4096u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::uint8_t* sec = nullptr;
            if (!sector_at(ds, &sec)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            for (std::size_t i = 0; i < sector_size / 128u; ++i) {
                const std::uint8_t* e = sec + i * 128u;
                const int type = e[66];
                if (type == 0) {
                    continue;
                }
                MicDirectoryEntry entry;
                entry.type = type;
                const std::size_t name_len = read_le16(e + 64u);
                entry.name = mic_decode_utf16le(e, name_len < 64u ? name_len : 64u);
                entry.name_lower = mic_ascii_lower(entry.name);
                entry.left = read_le32(e + 68u);
                entry.right = read_le32(e + 72u);
                entry.child = read_le32(e + 76u);
                entry.start = read_le32(e + 116u);
                entry.size = mic_read_le64(e + 120u);
                entries.push_back(entry);
            }
            if (static_cast<std::size_t>(ds) >= fat.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            ds = fat[static_cast<std::size_t>(ds)];
        }
        if (entries.empty() || entries[0].type != 5) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        struct MicPath {
            std::vector<std::string> components;
            std::uint32_t entry_id;
        };
        std::vector<MicPath> paths;
        // olefile raises only a non-fatal DEFECT_INCORRECT for out-of-range
        // tree ids, so the walk skips them instead of failing; a visited
        // guard mirrors olefile's duplicate-reference defect
        std::vector<std::uint8_t> visited(entries.size(), 0u);
        std::function<void(std::uint32_t, const std::vector<std::string>&)> walk_storage =
            [&](std::uint32_t node_id, const std::vector<std::string>& prefix) {
                if (node_id >= entries.size()) {
                    return;
                }
                std::vector<std::uint32_t> kids;
                std::function<void(std::uint32_t)> collect = [&](std::uint32_t id) {
                    if (id >= entries.size() || visited[static_cast<std::size_t>(id)] != 0u) {
                        return;
                    }
                    visited[static_cast<std::size_t>(id)] = 1u;
                    collect(entries[static_cast<std::size_t>(id)].left);
                    kids.push_back(id);
                    collect(entries[static_cast<std::size_t>(id)].right);
                };
                collect(entries[static_cast<std::size_t>(node_id)].child);
                std::stable_sort(kids.begin(), kids.end(),
                                 [&](std::uint32_t a, std::uint32_t b) {
                                     return entries[static_cast<std::size_t>(a)].name_lower <
                                            entries[static_cast<std::size_t>(b)].name_lower;
                                 });
                for (const std::uint32_t kid : kids) {
                    std::vector<std::string> full = prefix;
                    full.push_back(entries[static_cast<std::size_t>(kid)].name);
                    if (entries[static_cast<std::size_t>(kid)].type == 2) {
                        paths.push_back(MicPath{full, kid});
                    } else if (entries[static_cast<std::size_t>(kid)].type == 1) {
                        walk_storage(kid, full);
                    }
                }
            };
        walk_storage(0, {});

        std::vector<MicPath> images;
        for (const MicPath& p : paths) {
            if (p.components.size() >= 2u &&
                p.components[0].size() >= 4u &&
                p.components[0].compare(p.components[0].size() - 4u, 4u, ".ACI") == 0 &&
                p.components[1] == "Image") {
                images.push_back(p);
            }
        }
        if (images.empty()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const MicDirectoryEntry& stream_entry = entries[static_cast<std::size_t>(images[0].entry_id)];
        std::vector<std::uint8_t> stream;
        const std::uint64_t stream_size = stream_entry.size;
        if (stream_size > 512u * 1024u * 1024u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::size_t wanted = static_cast<std::size_t>(stream_size);
        if (stream_size < cutoff) {
            if (minifat_start == MIC_ENDOFCHAIN || minifat_start == MIC_FREESECT || nminifat == 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint32_t> minifat_ids;
            if (!mic_chain_fat(fat, minifat_start, 65536u, &minifat_ids) ||
                minifat_ids.size() < static_cast<std::size_t>(nminifat)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint32_t> minifat;
            for (std::size_t s = 0; s < static_cast<std::size_t>(nminifat) && s < minifat_ids.size(); ++s) {
                const std::uint8_t* sec = nullptr;
                if (!sector_at(minifat_ids[s], &sec)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                for (std::size_t i = 0; i < sector_size / 4u; ++i) {
                    minifat.push_back(read_le32(sec + 4u * i));
                }
            }
            std::vector<std::uint32_t> container_ids;
            if (!mic_chain_fat(fat, entries[0].start, 65536u, &container_ids)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            std::vector<std::uint8_t> container;
            for (const std::uint32_t id : container_ids) {
                const std::uint8_t* sec = nullptr;
                if (!sector_at(id, &sec)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                container.insert(container.end(), sec, sec + sector_size);
            }
            const std::uint64_t container_limit = entries[0].size < container.size() ? entries[0].size : container.size();
            std::uint32_t m = stream_entry.start;
            std::size_t guard = 0;
            while (m != MIC_ENDOFCHAIN && m != MIC_FREESECT && stream.size() < wanted) {
                if (++guard > 65536u || static_cast<std::size_t>(m) >= minifat.size()) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::uint64_t off = static_cast<std::uint64_t>(m) * mini_size;
                if (off >= container_limit) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::uint64_t available = container_limit - off;
                const std::uint64_t need = wanted - stream.size();
                const std::size_t take = static_cast<std::size_t>(available < need ? available : need);
                stream.insert(stream.end(), container.begin() + static_cast<std::ptrdiff_t>(off),
                              container.begin() + static_cast<std::ptrdiff_t>(off + take));
                m = minifat[static_cast<std::size_t>(m)];
            }
        } else {
            std::vector<std::uint32_t> ids;
            if (!mic_chain_fat(fat, stream_entry.start, 65536u, &ids)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            for (const std::uint32_t id : ids) {
                const std::uint8_t* sec = nullptr;
                if (!sector_at(id, &sec)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::size_t take = sector_size < wanted - stream.size() ? sector_size : wanted - stream.size();
                stream.insert(stream.end(), sec, sec + take);
                if (stream.size() >= wanted) {
                    break;
                }
            }
        }
        if (stream.size() < wanted) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        wchar_t temp_wide[MAX_PATH];
        const DWORD temp_length = GetTempPathW(MAX_PATH, temp_wide);
        if (temp_length == 0 || temp_length >= MAX_PATH) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        wchar_t name[64];
        std::swprintf(name, sizeof(name) / sizeof(name[0]), L"pillow-c-mic-%llu-%u.tif",
                      static_cast<unsigned long long>(GetTickCount64()),
                      static_cast<unsigned>(GetCurrentProcessId()));
        const std::wstring temp_path = std::wstring(temp_wide) + name;
        std::string temp_utf8;
        const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, temp_path.c_str(), static_cast<int>(temp_path.size()),
                                                    nullptr, 0, nullptr, nullptr);
        if (utf8_length > 0) {
            temp_utf8.resize(static_cast<std::size_t>(utf8_length));
            WideCharToMultiByte(CP_UTF8, 0, temp_path.c_str(), static_cast<int>(temp_path.size()),
                                temp_utf8.data(), utf8_length, nullptr, nullptr);
        }
        if (temp_utf8.empty() || !write_binary_file(temp_utf8.c_str(), stream)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int tiff_status = pillow_c_image_open_tiff(temp_utf8.c_str(), out_image);
        DeleteFileW(temp_path.c_str());
        if (tiff_status != PILLOW_C_OK) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

// BEHAV-SGI-001: SGI status codes local to the SGI open route. The
// facade maps each one to Pillow 11.3.0's exact error message; the
// generic status table does not carry these shapes.
constexpr int PILLOW_C_SGI_TRUNCATED = -6;
constexpr int PILLOW_C_SGI_16BIT_SHORT = -7;
constexpr int PILLOW_C_SGI_BAD_COMPRESSION = -8;

int save_sgi_image(const PillowCImage* image, const char* path, int bpc)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int channels = 0;
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        channels = 1;
    } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        channels = 3;
    } else if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        channels = 4;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (bpc != 1 && bpc != 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    try {
        std::vector<std::uint8_t> out;
        out.reserve(
            512u + static_cast<std::size_t>(image->width) *
                       static_cast<std::size_t>(image->height) *
                       static_cast<std::size_t>(channels) * static_cast<std::size_t>(bpc));
        append_be16(out, 474);
        out.push_back(0);  // rle: verbatim (Pillow's save never compresses)
        out.push_back(static_cast<std::uint8_t>(bpc));
        const int dimension = image->mode == PILLOW_C_MODE_L
            ? (image->height == 1 ? 1 : 2)
            : 3;
        append_be16(out, static_cast<std::uint16_t>(dimension));
        append_be16(out, static_cast<std::uint16_t>(image->width));
        append_be16(out, static_cast<std::uint16_t>(image->height));
        append_be16(out, static_cast<std::uint16_t>(channels));
        append_be32(out, 0);    // pinmin
        append_be32(out, 255);  // pinmax
        for (int i = 0; i < 4; ++i) {
            out.push_back(0);  // dummy
        }
        // Pillow writes the path basename minus its extension, ASCII
        // only (non-ASCII chars dropped), truncated to 79 bytes, then
        // a NUL byte.
        {
            const char* last_slash = std::max(strrchr(path, '\\'), strrchr(path, '/'));
            std::string candidate = last_slash ? last_slash + 1 : path;
            const std::size_t dot = candidate.rfind('.');
            if (dot != std::string::npos && dot > 0u) {
                candidate.resize(dot);
            }
            std::string base;
            for (unsigned char c : candidate) {
                if (c < 0x80u) {
                    base.push_back(static_cast<char>(c));
                }
            }
            if (base.size() > 79u) {
                base.resize(79u);
            }
            out.insert(out.end(), base.begin(), base.end());
            while (out.size() < 24u + 79u) {
                out.push_back(0);
            }
            out.push_back(0);  // NUL after the 79-byte name field
        }
        append_be32(out, 0);  // colormap
        for (int i = 0; i < 404; ++i) {
            out.push_back(0);  // dummy
        }

        // Band-major, bottom-up rows; 16-bit samples pack as v << 8.
        for (int ch = 0; ch < channels; ++ch) {
            for (int y = image->height - 1; y >= 0; --y) {
                const std::uint8_t* src_row =
                    image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < image->width; ++x) {
                    const std::uint8_t sample =
                        src_row[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                static_cast<std::size_t>(ch)];
                    if (bpc == 1) {
                        out.push_back(sample);
                    } else {
                        out.push_back(sample);
                        out.push_back(0);
                    }
                }
            }
        }

        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_sgi_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 512u || read_be16(data.data()) != 474u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int compression = data[2];
        const int bpc = data[3];
        const int dimension = read_be16(data.data() + 4u);
        const int xsize = read_be16(data.data() + 6u);
        const int ysize = read_be16(data.data() + 8u);
        const int zsize = read_be16(data.data() + 10u);

        // Pillow's MODES table keys (bpc, dimension, zsize).
        int mode = 0;
        int channels = 0;
        if ((bpc == 1 || bpc == 2) && (dimension == 1 || dimension == 2) && zsize == 1) {
            mode = PILLOW_C_MODE_L;
            channels = 1;
        } else if ((bpc == 1 || bpc == 2) && dimension == 3 && zsize == 3) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
        } else if ((bpc == 1 || bpc == 2) && dimension == 3 && zsize == 4) {
            mode = PILLOW_C_MODE_RGBA;
            channels = 4;
        } else {
            return PILLOW_C_MISMATCH;
        }
        if (xsize <= 0 || ysize <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(xsize, ysize, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            xsize,
            ysize,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};

        if (compression == 0) {
            const std::size_t pagesize = static_cast<std::size_t>(xsize) *
                                         static_cast<std::size_t>(ysize) *
                                         static_cast<std::size_t>(bpc);
            const std::size_t need = 512u + static_cast<std::size_t>(zsize) * pagesize;
            if (data.size() < need) {
                delete image;
                return bpc == 2 ? PILLOW_C_SGI_16BIT_SHORT : PILLOW_C_SGI_TRUNCATED;
            }
            const std::uint8_t* payload = data.data() + 512u;
            for (int ch = 0; ch < channels; ++ch) {
                const std::uint8_t* band = payload + static_cast<std::size_t>(ch) * pagesize;
                for (int y = 0; y < ysize; ++y) {
                    const std::uint8_t* src_row = band +
                        static_cast<std::size_t>(y) * static_cast<std::size_t>(xsize) *
                            static_cast<std::size_t>(bpc);
                    std::uint8_t* dst_row = image->pixels.data() +
                        static_cast<std::size_t>(ysize - 1 - y) * image_stride;
                    if (bpc == 1) {
                        for (int x = 0; x < xsize; ++x) {
                            dst_row[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                    static_cast<std::size_t>(ch)] = src_row[static_cast<std::size_t>(x)];
                        }
                    } else {
                        for (int x = 0; x < xsize; ++x) {
                            const std::uint16_t v = static_cast<std::uint16_t>(
                                (src_row[static_cast<std::size_t>(x) * 2u] << 8) |
                                src_row[static_cast<std::size_t>(x) * 2u + 1u]);
                            dst_row[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                    static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(v >> 8);
                        }
                    }
                }
            }
        } else if (compression == 1) {
            // Pillow's ImagingSgiRleDecode: channel-major per-row
            // start/length tables, run/copy chunks, bottom-up rows,
            // and the exact quirk semantics (a row whose final chunk
            // carries a nonzero specifier is discarded and decoding
            // stops; short rows keep the persistent buffer's stale
            // bytes; malformed tables/data raise the overrun error).
            const std::size_t bufsize = data.size() - 512u;
            const std::size_t tablen =
                static_cast<std::size_t>(zsize) * static_cast<std::size_t>(ysize);
            if (bufsize < 8u * tablen) {
                delete image;
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::uint8_t* ptr = data.data() + 512u;
            const std::uint8_t* end = data.data() + data.size() - 1u;
            std::vector<std::uint32_t> starttab(tablen, 0u);
            std::vector<std::uint32_t> lengthtab(tablen, 0u);
            for (std::size_t i = 0; i < tablen; ++i) {
                starttab[i] = read_be32(ptr + i * 4u);
                lengthtab[i] = read_be32(ptr + (tablen + i) * 4u);
            }
            std::vector<std::uint8_t> rowbuf(
                static_cast<std::size_t>(xsize) * static_cast<std::size_t>(channels) * 2u,
                std::uint8_t{0});
            bool stop = false;
            for (int rowno = 0; rowno < ysize && !stop; ++rowno) {
                for (int channo = 0; channo < channels && !stop; ++channo) {
                    const std::size_t tab_index =
                        static_cast<std::size_t>(rowno) + static_cast<std::size_t>(channo) * static_cast<std::size_t>(ysize);
                    const std::uint32_t rleoffset = starttab[tab_index];
                    const std::uint32_t rlelength = lengthtab[tab_index];
                    if (rleoffset < 512u) {
                        delete image;
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    const std::uint8_t* src = ptr + (rleoffset - 512u);
                    int status = 0;
                    std::uint32_t n = rlelength;
                    std::size_t x = 0;
                    for (; n > 0u; --n) {
                        const std::size_t x_start = x;
                        if (bpc == 1) {
                            if (src > end) {
                                status = -1;
                                break;
                            }
                            const std::uint8_t pixel = *src++;
                            if (n == 1u && pixel != 0u) {
                                stop = true;
                                break;
                            }
                            const std::uint8_t count = pixel & 0x7Fu;
                            if (count == 0u) {
                                break;
                            }
                            if (x + count > static_cast<std::size_t>(xsize)) {
                                status = -1;
                                break;
                            }
                            x += count;
                            const std::size_t dest_base =
                                x_start * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channo);
                            if ((pixel & 0x80u) != 0u) {
                                if (src + count > end) {
                                    status = -1;
                                    break;
                                }
                                for (std::uint8_t i = 0; i < count; ++i, ++src) {
                                    rowbuf[dest_base + static_cast<std::size_t>(i) * static_cast<std::size_t>(channels)] = *src;
                                }
                            } else {
                                if (src > end) {
                                    status = -1;
                                    break;
                                }
                                const std::uint8_t value = *src++;
                                for (std::uint8_t i = 0; i < count; ++i) {
                                    rowbuf[dest_base + static_cast<std::size_t>(i) * static_cast<std::size_t>(channels)] = value;
                                }
                            }
                        } else {
                            if (src + 1 > end) {
                                status = -1;
                                break;
                            }
                            const std::uint8_t pixel = src[1];
                            src += 2;
                            if (n == 1u && pixel != 0u) {
                                stop = true;
                                break;
                            }
                            const std::uint8_t count = pixel & 0x7Fu;
                            if (count == 0u) {
                                break;
                            }
                            if (x + count > static_cast<std::size_t>(xsize)) {
                                status = -1;
                                break;
                            }
                            x += count;
                            std::size_t dest =
                                (x_start * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channo)) * 2u;
                            if ((pixel & 0x80u) != 0u) {
                                if (src + 2u * count > end) {
                                    status = -1;
                                    break;
                                }
                                for (std::uint8_t i = 0; i < count; ++i) {
                                    rowbuf[dest + 0u] = src[0];
                                    rowbuf[dest + 1u] = src[1];
                                    src += 2;
                                    dest += static_cast<std::size_t>(channels) * 2u;
                                }
                            } else {
                                if (src + 2 > end) {
                                    status = -1;
                                    break;
                                }
                                for (std::uint8_t i = 0; i < count; ++i) {
                                    rowbuf[dest + 0u] = src[0];
                                    rowbuf[dest + 1u] = src[1];
                                    dest += static_cast<std::size_t>(channels) * 2u;
                                }
                                src += 2;
                            }
                        }
                    }
                    if (status == -1) {
                        delete image;
                        return PILLOW_C_INVALID_LENGTH;
                    }
                    if (stop) {
                        break;
                    }
                }
                if (stop) {
                    break;
                }
                std::uint8_t* dst_row = image->pixels.data() +
                    static_cast<std::size_t>(ysize - 1 - rowno) * image_stride;
                if (bpc == 1) {
                    for (int x = 0; x < xsize; ++x) {
                        for (int ch = 0; ch < channels; ++ch) {
                            dst_row[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                    static_cast<std::size_t>(ch)] =
                                rowbuf[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                       static_cast<std::size_t>(ch)];
                        }
                    }
                } else {
                    for (int x = 0; x < xsize; ++x) {
                        for (int ch = 0; ch < channels; ++ch) {
                            const std::size_t off =
                                (static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                 static_cast<std::size_t>(ch)) * 2u;
                            const std::uint16_t v = static_cast<std::uint16_t>(
                                (rowbuf[off] << 8) | rowbuf[off + 1u]);
                            dst_row[static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) +
                                    static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(v >> 8);
                        }
                    }
                }
            }
        } else {
            delete image;
            return PILLOW_C_SGI_BAD_COMPRESSION;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// BEHAV-DDS-001: DDS status codes local to the DDS open route; the
// facade maps them to Pillow 11.3.0's exact error messages.
constexpr int PILLOW_C_DDS_HDR_SIZE = -9;
constexpr int PILLOW_C_DDS_HDR_SHORT = -10;
constexpr int PILLOW_C_DDS_PFFLAGS = -11;
constexpr int PILLOW_C_DDS_FOURCC = -12;
constexpr int PILLOW_C_DDS_BITCOUNT = -13;
constexpr int PILLOW_C_DDS_DXGI = -14;
constexpr int PILLOW_C_DDS_TRUNC8 = -15;
constexpr int PILLOW_C_DDS_TRUNC_RAW = -16;
constexpr int PILLOW_C_DDS_ZERO_MASK = -17;
constexpr int PILLOW_C_DDS_TRUNC16 = -18;

namespace {
struct dds_rgba {
    std::uint8_t c[4];
};

std::uint16_t dds_encode_565(const std::uint8_t* c)
{
    const std::uint8_t r = c[0] >> (8 - 5);
    const std::uint8_t g = c[1] >> (8 - 6);
    const std::uint8_t b = c[2] >> (8 - 5);
    return static_cast<std::uint16_t>((r << (5 + 6)) | (g << 5) | b);
}

void dds_decode_565(std::uint16_t x, std::uint8_t* out)
{
    int r = (x & 0xf800) >> 8;
    r |= r >> 5;
    int g = (x & 0x7e0) >> 3;
    g |= g >> 6;
    int b = (x & 0x1f) << 3;
    b |= b >> 5;
    out[0] = static_cast<std::uint8_t>(r);
    out[1] = static_cast<std::uint8_t>(g);
    out[2] = static_cast<std::uint8_t>(b);
}

// Port of Pillow's encode_bc1_color (BcnEncode.c), including the
// integer-division weighting and the transparency endpoint swap.
void dds_encode_bc1_block(
    const PillowCImage* im,
    int block_x,
    int block_y,
    std::uint8_t* dst,
    bool separate_alpha)
{
    std::uint16_t color_min = 0;
    std::uint16_t color_max = 0;
    std::uint8_t color_min_rgb[3] = {0, 0, 0};
    std::uint8_t color_max_rgb[3] = {0, 0, 0};
    std::uint8_t block[16][4] = {{0}};
    int first = 1;
    int transparency = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::uint8_t* current = block[i + j * 4];
            const int x = block_x + i;
            const int y = block_y + j;
            if (x >= im->width || y >= im->height) {
                for (int k = 0; k < 3; k++) {
                    current[k] = 0;
                }
                continue;
            }
            const std::uint8_t* px =
                im->pixels.data() + static_cast<std::size_t>(y) * im->stride +
                static_cast<std::size_t>(x) * static_cast<std::size_t>(im->channels);
            for (int k = 0; k < 3; k++) {
                current[k] = px[im->channels == 1 ? 0 : k];
            }
            if (separate_alpha) {
                if (px[3] == 0) {
                    current[3] = 0;
                    transparency = 1;
                    continue;
                }
                current[3] = 1;
            }
            const std::uint16_t color = dds_encode_565(current);
            if (first || color < color_min) {
                color_min = color;
            }
            if (first || color > color_max) {
                color_max = color;
            }
            first = 0;
        }
    }

    if (transparency) {
        *dst++ = static_cast<std::uint8_t>(color_min);
        *dst++ = static_cast<std::uint8_t>(color_min >> 8);
    }
    *dst++ = static_cast<std::uint8_t>(color_max);
    *dst++ = static_cast<std::uint8_t>(color_max >> 8);
    if (!transparency) {
        *dst++ = static_cast<std::uint8_t>(color_min);
        *dst++ = static_cast<std::uint8_t>(color_min >> 8);
    }

    dds_decode_565(color_min, color_min_rgb);
    dds_decode_565(color_max, color_max_rgb);
    for (int i = 0; i < 4; i++) {
        std::uint8_t l = 0;
        for (int j = 3; j > -1; j--) {
            const std::uint8_t* current = block[i * 4 + j];
            if (transparency && !current[3]) {
                l |= 3 << (j * 2);
                continue;
            }
            float distance = 0;
            int total = 0;
            for (int k = 0; k < 3; k++) {
                const float denom =
                    static_cast<float>(std::abs(color_max_rgb[k] - color_min_rgb[k]));
                if (denom != 0) {
                    distance += std::abs(static_cast<int>(current[k]) - static_cast<int>(color_min_rgb[k])) /
                        denom;
                    total += 1;
                }
            }
            if (total == 0) {
                continue;
            }
            if (transparency) {
                distance *= 4 / total;
                if (distance < 1) {
                    // color_max
                } else if (distance < 3) {
                    l |= 2 << (j * 2);
                } else {
                    l |= 1 << (j * 2);
                }
            } else {
                distance *= 6 / total;
                if (distance < 1) {
                    l |= 1 << (j * 2);
                } else if (distance < 3) {
                    l |= 3 << (j * 2);
                } else if (distance < 5) {
                    l |= 2 << (j * 2);
                } else {
                    // color_max
                }
            }
        }
        *dst++ = l;
    }
}

// Port of Pillow's encode_bc2_block (BcnEncode.c).
void dds_encode_bc2_block(const PillowCImage* im, int block_x, int block_y, std::uint8_t* dst)
{
    std::uint8_t block[16] = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const int x = block_x + i;
            const int y = block_y + j;
            if (x >= im->width || y >= im->height) {
                block[i + j * 4] = 0;
                continue;
            }
            block[i + j * 4] = im->pixels[static_cast<std::size_t>(y) * im->stride +
                                         static_cast<std::size_t>(x) * static_cast<std::size_t>(im->channels) + 3u];
        }
    }
    for (int i = 0; i < 4; i++) {
        std::uint16_t l = 0;
        for (int j = 3; j > -1; j--) {
            l |= static_cast<std::uint16_t>(block[i * 4 + j]) << (j * 4);
        }
        *dst++ = static_cast<std::uint8_t>(l);
        *dst++ = static_cast<std::uint8_t>(l >> 8);
    }
}

// Port of Pillow's encode_bc3_alpha (BcnEncode.c).
void dds_encode_bc3_alpha(
    const PillowCImage* im,
    int block_x,
    int block_y,
    std::uint8_t* dst,
    int channel)
{
    std::uint8_t alpha_min = 0;
    std::uint8_t alpha_max = 0;
    std::uint8_t block[16] = {0};
    int first = 1;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const int x = block_x + i;
            const int y = block_y + j;
            if (x >= im->width || y >= im->height) {
                block[i + j * 4] = 0;
                continue;
            }
            const std::uint8_t current =
                im->pixels[static_cast<std::size_t>(y) * im->stride +
                           static_cast<std::size_t>(x) * static_cast<std::size_t>(im->channels) +
                           static_cast<std::size_t>(channel)];
            block[i + j * 4] = current;
            if (first || current < alpha_min) {
                alpha_min = current;
            }
            if (first || current > alpha_max) {
                alpha_max = current;
            }
            first = 0;
        }
    }
    *dst++ = alpha_min;
    *dst++ = alpha_max;
    const float denom = static_cast<float>(std::abs(static_cast<int>(alpha_max) - static_cast<int>(alpha_min)));
    for (int i = 0; i < 2; i++) {
        std::uint32_t l = 0;
        for (int j = 7; j > -1; j--) {
            const std::uint8_t current = block[i * 8 + j];
            if (!current) {
                l |= 6u << (j * 3);
                continue;
            }
            if (current == 255) {
                l |= 7u << (j * 3);
                continue;
            }
            const float distance =
                denom == 0 ? 0 : std::abs(static_cast<int>(current) - static_cast<int>(alpha_min)) / denom * 10;
            if (distance < 3) {
                l |= 2u << (j * 3);
            } else if (distance < 5) {
                l |= 3u << (j * 3);
            } else if (distance < 7) {
                l |= 4u << (j * 3);
            } else {
                l |= 5u << (j * 3);
            }
        }
        *dst++ = static_cast<std::uint8_t>(l);
        *dst++ = static_cast<std::uint8_t>(l >> 8);
        *dst++ = static_cast<std::uint8_t>(l >> 16);
    }
}

// ---- BCN decode (port of Pillow's BcnDecode.c) ----

void dds_decode_bc1_color(dds_rgba* dst, const std::uint8_t* src, int separate_alpha)
{
    const std::uint16_t c0 = static_cast<std::uint16_t>(src[0] | (src[1] << 8));
    const std::uint16_t c1 = static_cast<std::uint16_t>(src[2] | (src[3] << 8));
    dds_rgba p[4];
    p[0] = dds_rgba{{0, 0, 0, 0}};
    p[1] = dds_rgba{{0, 0, 0, 0}};
    p[2] = dds_rgba{{0, 0, 0, 0}};
    p[3] = dds_rgba{{0, 0, 0, 0}};
    dds_decode_565(c0, p[0].c);
    p[0].c[3] = 0xff;
    const std::uint16_t r0 = p[0].c[0];
    const std::uint16_t g0 = p[0].c[1];
    const std::uint16_t b0 = p[0].c[2];
    dds_decode_565(c1, p[1].c);
    p[1].c[3] = 0xff;
    const std::uint16_t r1 = p[1].c[0];
    const std::uint16_t g1 = p[1].c[1];
    const std::uint16_t b1 = p[1].c[2];
    if (c0 > c1 || separate_alpha) {
        p[2].c[0] = static_cast<std::uint8_t>((2 * r0 + 1 * r1) / 3);
        p[2].c[1] = static_cast<std::uint8_t>((2 * g0 + 1 * g1) / 3);
        p[2].c[2] = static_cast<std::uint8_t>((2 * b0 + 1 * b1) / 3);
        p[2].c[3] = 0xff;
        p[3].c[0] = static_cast<std::uint8_t>((1 * r0 + 2 * r1) / 3);
        p[3].c[1] = static_cast<std::uint8_t>((1 * g0 + 2 * g1) / 3);
        p[3].c[2] = static_cast<std::uint8_t>((1 * b0 + 2 * b1) / 3);
        p[3].c[3] = 0xff;
    } else {
        p[2].c[0] = static_cast<std::uint8_t>((r0 + r1) / 2);
        p[2].c[1] = static_cast<std::uint8_t>((g0 + g1) / 2);
        p[2].c[2] = static_cast<std::uint8_t>((b0 + b1) / 2);
        p[2].c[3] = 0xff;
        p[3].c[0] = 0;
        p[3].c[1] = 0;
        p[3].c[2] = 0;
        p[3].c[3] = 0;
    }
    for (int n = 0; n < 4; n++) {
        for (int o = 0; o < 4; o++) {
            const int cw = 3 & (src[4 + n] >> (2 * o));
            dst[n * 4 + o] = p[cw];
        }
    }
}

void dds_decode_bc3_alpha(std::uint8_t* dst, const std::uint8_t* src, int stride, int o, int sign)
{
    std::uint16_t a0;
    std::uint16_t a1;
    std::uint8_t a[8];
    std::uint32_t lut1;
    std::uint32_t lut2;
    if (sign == 1) {
        a0 = static_cast<std::uint16_t>(static_cast<int8_t>(src[0]) + 128);
        a1 = static_cast<std::uint16_t>(static_cast<int8_t>(src[1]) + 128);
    } else {
        a0 = src[0];
        a1 = src[1];
    }
    lut1 = src[2] | (src[3] << 8) | (src[4] << 16);
    lut2 = src[5] | (src[6] << 8) | (src[7] << 16);
    a[0] = static_cast<std::uint8_t>(a0);
    a[1] = static_cast<std::uint8_t>(a1);
    if (a0 > a1) {
        a[2] = static_cast<std::uint8_t>((6 * a0 + 1 * a1) / 7);
        a[3] = static_cast<std::uint8_t>((5 * a0 + 2 * a1) / 7);
        a[4] = static_cast<std::uint8_t>((4 * a0 + 3 * a1) / 7);
        a[5] = static_cast<std::uint8_t>((3 * a0 + 4 * a1) / 7);
        a[6] = static_cast<std::uint8_t>((2 * a0 + 5 * a1) / 7);
        a[7] = static_cast<std::uint8_t>((1 * a0 + 6 * a1) / 7);
    } else {
        a[2] = static_cast<std::uint8_t>((4 * a0 + 1 * a1) / 5);
        a[3] = static_cast<std::uint8_t>((3 * a0 + 2 * a1) / 5);
        a[4] = static_cast<std::uint8_t>((2 * a0 + 3 * a1) / 5);
        a[5] = static_cast<std::uint8_t>((1 * a0 + 4 * a1) / 5);
        a[6] = 0;
        a[7] = 0xff;
    }
    for (int n = 0; n < 8; n++) {
        const int aw = 7 & (lut1 >> (3 * n));
        dst[stride * n + o] = a[aw];
    }
    for (int n = 0; n < 8; n++) {
        const int aw = 7 & (lut2 >> (3 * n));
        dst[stride * (8 + n) + o] = a[aw];
    }
}

void dds_decode_bc2_block(dds_rgba* col, const std::uint8_t* src)
{
    dds_decode_bc1_color(col, src + 8, 1);
    for (int n = 0; n < 16; n++) {
        const int bit_i = n * 4;
        const int by_i = bit_i >> 3;
        int av = 0xf & (src[by_i] >> (bit_i & 7));
        av = (av << 4) | av;
        col[n].c[3] = static_cast<std::uint8_t>(av);
    }
}

void dds_decode_bc3_block(dds_rgba* col, const std::uint8_t* src)
{
    dds_decode_bc1_color(col, src + 8, 1);
    dds_decode_bc3_alpha(
        reinterpret_cast<std::uint8_t*>(col), src, static_cast<int>(sizeof(col[0])), 3, 0);
}

void dds_decode_bc4_block(std::uint8_t* col, const std::uint8_t* src)
{
    dds_decode_bc3_alpha(col, src, 1, 0, 0);
}

void dds_decode_bc5_block(dds_rgba* col, const std::uint8_t* src, int sign)
{
    dds_decode_bc3_alpha(
        reinterpret_cast<std::uint8_t*>(col), src, static_cast<int>(sizeof(col[0])), 0, sign);
    dds_decode_bc3_alpha(
        reinterpret_cast<std::uint8_t*>(col), src + 8, static_cast<int>(sizeof(col[0])), 1, sign);
}

// ---- BC6/BC7 decode (port of Pillow's BcnDecode.c) ----

std::uint8_t dds_get_bit(const std::uint8_t* src, int bit, std::size_t size)
{
    const int by = bit >> 3;
    bit &= 7;
    if (static_cast<std::size_t>(by) >= size) {
        return 0;
    }
    return static_cast<std::uint8_t>((src[by] >> bit) & 1);
}

std::uint8_t dds_get_bits(const std::uint8_t* src, int bit, int count, std::size_t size)
{
    if (!count) {
        return 0;
    }
    const int by = bit >> 3;
    bit &= 7;
    if (static_cast<std::size_t>(by) >= size) {
        return 0;
    }
    if (bit + count <= 8) {
        return static_cast<std::uint8_t>((src[by] >> bit) & ((1 << count) - 1));
    }
    std::uint16_t x = src[by];
    if (static_cast<std::size_t>(by + 1) < size) {
        x |= static_cast<std::uint16_t>(src[by + 1]) << 8;
    }
    return static_cast<std::uint8_t>((x >> bit) & ((1 << count) - 1));
}

struct dds_bc7_mode_info {
    char ns;
    char pb;
    char rb;
    char isb;
    char cb;
    char ab;
    char epb;
    char spb;
    char ib;
    char ib2;
};

const dds_bc7_mode_info dds_bc7_modes[] = {
    {3, 4, 0, 0, 4, 0, 1, 0, 3, 0},
    {2, 6, 0, 0, 6, 0, 0, 1, 3, 0},
    {3, 6, 0, 0, 5, 0, 0, 0, 2, 0},
    {2, 6, 0, 0, 7, 0, 1, 0, 2, 0},
    {1, 0, 2, 1, 5, 6, 0, 0, 2, 3},
    {1, 0, 2, 0, 7, 8, 0, 0, 2, 2},
    {1, 0, 0, 0, 7, 7, 1, 0, 4, 0},
    {2, 6, 0, 0, 5, 5, 1, 0, 2, 0}};

const std::uint16_t dds_bc7_si2[] = {
    0xcccc, 0x8888, 0xeeee, 0xecc8, 0xc880, 0xfeec, 0xfec8, 0xec80, 0xc800, 0xffec,
    0xfe80, 0xe800, 0xffe8, 0xff00, 0xfff0, 0xf000, 0xf710, 0x008e, 0x7100, 0x08ce,
    0x008c, 0x7310, 0x3100, 0x8cce, 0x088c, 0x3110, 0x6666, 0x366c, 0x17e8, 0x0ff0,
    0x718e, 0x399c, 0xaaaa, 0xf0f0, 0x5a5a, 0x33cc, 0x3c3c, 0x55aa, 0x9696, 0xa55a,
    0x73ce, 0x13c8, 0x324c, 0x3bdc, 0x6996, 0xc33c, 0x9966, 0x0660, 0x0272, 0x04e4,
    0x4e40, 0x2720, 0xc936, 0x936c, 0x39c6, 0x639c, 0x9336, 0x9cc6, 0x817e, 0xe718,
    0xccf0, 0x0fcc, 0x7744, 0xee22};

const std::uint32_t dds_bc7_si3[] = {
    0xaa685050, 0x6a5a5040, 0x5a5a4200, 0x5450a0a8, 0xa5a50000, 0xa0a05050, 0x5555a0a0,
    0x5a5a5050, 0xaa550000, 0xaa555500, 0xaaaa5500, 0x90909090, 0x94949494, 0xa4a4a4a4,
    0xa9a59450, 0x2a0a4250, 0xa5945040, 0x0a425054, 0xa5a5a500, 0x55a0a0a0, 0xa8a85454,
    0x6a6a4040, 0xa4a45000, 0x1a1a0500, 0x0050a4a4, 0xaaa59090, 0x14696914, 0x69691400,
    0xa08585a0, 0xaa821414, 0x50a4a450, 0x6a5a0200, 0xa9a58000, 0x5090a0a8, 0xa8a09050,
    0x24242424, 0x00aa5500, 0x24924924, 0x24499224, 0x50a50a50, 0x500aa550, 0xaaaa4444,
    0x66660000, 0xa5a0a5a0, 0x50a050a0, 0x69286928, 0x44aaaa44, 0x66666600, 0xaa444444,
    0x54a854a8, 0x95809580, 0x96969600, 0xa85454a8, 0x80959580, 0xaa141414, 0x96960000,
    0xaaaa1414, 0xa05050a0, 0xa0a5a5a0, 0x96000000, 0x40804080, 0xa9a8a9a8, 0xaaaaaa44,
    0x2a4a5254};

const char dds_bc7_ai0[] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                            15, 15, 15, 15, 2,  8,  2,  2,  8,  8,  15, 2,  8,
                            2,  2,  8,  8,  2,  2,  15, 15, 6,  8,  2,  8,  15,
                            15, 2,  8,  2,  2,  2,  15, 15, 6,  6,  2,  6,  8,
                            15, 15, 2,  2,  15, 15, 15, 15, 15, 2,  2,  15};

const char dds_bc7_ai1[] = {3,  3,  15, 15, 8,  3,  15, 15, 8,  8,  6,  6,  6,
                            5,  3,  3,  3,  3,  8,  15, 3,  3,  6,  10, 5,  8,
                            8,  6,  8,  5,  15, 15, 8,  15, 3,  5,  6,  10, 8,
                            15, 15, 3,  15, 5,  15, 15, 15, 15, 3,  15, 5,  5,
                            5,  8,  5,  10, 5,  10, 8,  13, 15, 12, 3,  3};

const char dds_bc7_ai2[] = {15, 8,  8,  3,  15, 15, 3,  8,  15, 15, 15, 15, 15,
                            15, 15, 8,  15, 8,  15, 3,  15, 8,  15, 8,  3,  15,
                            6,  10, 15, 15, 10, 8,  15, 3,  15, 10, 10, 8,  9,
                            10, 6,  15, 8,  15, 3,  6,  6,  8,  15, 3,  15, 15,
                            15, 15, 15, 15, 15, 15, 15, 15, 3,  15, 15, 8};

const char dds_bc7_weights2[] = {0, 21, 43, 64};
const char dds_bc7_weights3[] = {0, 9, 18, 27, 37, 46, 55, 64};
const char dds_bc7_weights4[] = {
    0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

const char* dds_bc7_get_weights(int n)
{
    if (n == 2) {
        return dds_bc7_weights2;
    }
    if (n == 3) {
        return dds_bc7_weights3;
    }
    return dds_bc7_weights4;
}

int dds_bc7_get_subset(int ns, int partition, int n)
{
    if (ns == 2) {
        return 1 & (dds_bc7_si2[partition] >> n);
    }
    if (ns == 3) {
        return 3 & (dds_bc7_si3[partition] >> (2 * n));
    }
    return 0;
}

std::uint8_t dds_expand_quantized(std::uint8_t v, int bits)
{
    v = static_cast<std::uint8_t>(v << (8 - bits));
    return static_cast<std::uint8_t>(v | (v >> bits));
}

void dds_bc7_lerp(dds_rgba* dst, const dds_rgba* e, int s0, int s1)
{
    const int t0 = 64 - s0;
    const int t1 = 64 - s1;
    dst->c[0] = static_cast<std::uint8_t>((t0 * e[0].c[0] + s0 * e[1].c[0] + 32) >> 6);
    dst->c[1] = static_cast<std::uint8_t>((t0 * e[0].c[1] + s0 * e[1].c[1] + 32) >> 6);
    dst->c[2] = static_cast<std::uint8_t>((t0 * e[0].c[2] + s0 * e[1].c[2] + 32) >> 6);
    dst->c[3] = static_cast<std::uint8_t>((t1 * e[0].c[3] + s1 * e[1].c[3] + 32) >> 6);
}

void dds_decode_bc7_block(dds_rgba* col, const std::uint8_t* src, std::size_t size)
{
    dds_rgba endpoints[6];
    int bit = 0;
    int cibit;
    int aibit;
    const int mode0 = src[0];
    if (!mode0) {
        for (int i = 0; i < 16; i++) {
            col[i].c[0] = col[i].c[1] = col[i].c[2] = 0;
            col[i].c[3] = 255;
        }
        return;
    }
    while (!(mode0 & (1 << bit))) {
        bit++;
    }
    const int mode = bit;
    bit = mode + 1;  // the C post-increment leaves the fields after the mode bits
    const dds_bc7_mode_info* info = &dds_bc7_modes[mode];
    const int cb = info->cb;
    const int ab = info->ab;
    const char* cw = dds_bc7_get_weights(info->ib);
    const char* aw = dds_bc7_get_weights((ab && info->ib2) ? info->ib2 : info->ib);
    const int pb = info->pb;
    const int rb = info->rb;
    const int isb = info->isb;
    int partition = 0;
    int rotation = 0;
    int index_sel = 0;
    if (pb) {
        partition = dds_get_bits(src, bit, pb, size);
        bit += pb;
    }
    if (rb) {
        rotation = dds_get_bits(src, bit, rb, size);
        bit += rb;
    }
    if (isb) {
        index_sel = dds_get_bits(src, bit, isb, size);
        bit += isb;
    }
    const int numep = info->ns << 1;
    for (int i = 0; i < numep; i++) {
        endpoints[i].c[0] = dds_get_bits(src, bit, cb, size);
        bit += cb;
    }
    for (int i = 0; i < numep; i++) {
        endpoints[i].c[1] = dds_get_bits(src, bit, cb, size);
        bit += cb;
    }
    for (int i = 0; i < numep; i++) {
        endpoints[i].c[2] = dds_get_bits(src, bit, cb, size);
        bit += cb;
    }
    for (int i = 0; i < numep; i++) {
        endpoints[i].c[3] = ab ? dds_get_bits(src, bit, ab, size) : 255;
        if (ab) {
            bit += ab;
        }
    }
    if (info->epb) {
        for (int i = 0; i < numep; i++) {
            const std::uint8_t val = dds_get_bit(src, bit, size);
            bit += 1;
            endpoints[i].c[0] = static_cast<std::uint8_t>((endpoints[i].c[0] << 1) | val);
            endpoints[i].c[1] = static_cast<std::uint8_t>((endpoints[i].c[1] << 1) | val);
            endpoints[i].c[2] = static_cast<std::uint8_t>((endpoints[i].c[2] << 1) | val);
            if (ab) {
                endpoints[i].c[3] = static_cast<std::uint8_t>((endpoints[i].c[3] << 1) | val);
            }
        }
    }
    if (info->spb) {
        for (int i = 0; i < numep; i += 2) {
            const std::uint8_t val = dds_get_bit(src, bit, size);
            bit += 1;
            for (int j = 0; j < 2; j++) {
                endpoints[i + j].c[0] = static_cast<std::uint8_t>((endpoints[i + j].c[0] << 1) | val);
                endpoints[i + j].c[1] = static_cast<std::uint8_t>((endpoints[i + j].c[1] << 1) | val);
                endpoints[i + j].c[2] = static_cast<std::uint8_t>((endpoints[i + j].c[2] << 1) | val);
                if (ab) {
                    endpoints[i + j].c[3] = static_cast<std::uint8_t>((endpoints[i + j].c[3] << 1) | val);
                }
            }
        }
    }
    const int ecb = cb + (info->epb ? 1 : 0) + (info->spb ? 1 : 0);
    const int eab = ab + (info->epb ? 1 : 0) + (info->spb ? 1 : 0);
    for (int i = 0; i < numep; i++) {
        endpoints[i].c[0] = dds_expand_quantized(endpoints[i].c[0], ecb);
        endpoints[i].c[1] = dds_expand_quantized(endpoints[i].c[1], ecb);
        endpoints[i].c[2] = dds_expand_quantized(endpoints[i].c[2], ecb);
        if (ab) {
            endpoints[i].c[3] = dds_expand_quantized(endpoints[i].c[3], eab);
        }
    }
    cibit = bit;
    aibit = cibit + 16 * info->ib - info->ns;
    for (int i = 0; i < 16; i++) {
        const int s = dds_bc7_get_subset(info->ns, partition, i) << 1;
        int ib = info->ib;
        if (i == 0) {
            ib--;
        } else if (info->ns == 2) {
            if (i == dds_bc7_ai0[partition]) {
                ib--;
            }
        } else if (info->ns == 3) {
            if (i == dds_bc7_ai1[partition]) {
                ib--;
            } else if (i == dds_bc7_ai2[partition]) {
                ib--;
            }
        }
        const int i0 = dds_get_bits(src, cibit, ib, size);
        cibit += ib;
        if (ab && info->ib2) {
            int ib2 = info->ib2;
            if (ib2 && i == 0) {
                ib2--;
            }
            const int i1 = dds_get_bits(src, aibit, ib2, size);
            aibit += ib2;
            if (index_sel) {
                dds_bc7_lerp(&col[i], &endpoints[s], aw[i1], cw[i0]);
            } else {
                dds_bc7_lerp(&col[i], &endpoints[s], cw[i0], aw[i1]);
            }
        } else {
            dds_bc7_lerp(&col[i], &endpoints[s], cw[i0], cw[i0]);
        }
        if (rotation == 1) {
            std::swap(col[i].c[0], col[i].c[3]);
        } else if (rotation == 2) {
            std::swap(col[i].c[1], col[i].c[3]);
        } else if (rotation == 3) {
            std::swap(col[i].c[2], col[i].c[3]);
        }
    }
}
void dds_bcn_put_block(
    PillowCImage* image,
    int block_x,
    int block_y,
    const dds_rgba* col,
    int channels)
{
    for (int j = 0; j < 4; j++) {
        const int y = block_y + j;
        if (y >= image->height) {
            continue;
        }
        std::uint8_t* dst_row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int i = 0; i < 4; i++) {
            const int x = block_x + i;
            if (x >= image->width) {
                continue;
            }
            const dds_rgba& px = col[j * 4 + i];
            std::uint8_t* dst =
                dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
            for (int c = 0; c < channels; c++) {
                dst[c] = px.c[c];
            }
        }
    }
}
}  // namespace

int save_dds_image(const PillowCImage* image, const char* path, const char* pixel_format)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int channels = 0;
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        channels = 1;
    } else if (image->mode == PILLOW_C_MODE_LA && image->channels == 2) {
        channels = 2;
    } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        channels = 3;
    } else if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        channels = 4;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    const char* fmt = pixel_format ? pixel_format : "";
    int n = 0;
    std::uint32_t fourcc = 0;
    bool dx10 = false;
    std::uint32_t dxgi = 0;
    if (fmt[0] == 0) {
        // raw
    } else if (std::strcmp(fmt, "DXT1") == 0) {
        fourcc = 0x31545844;
        n = 1;
    } else if (std::strcmp(fmt, "DXT3") == 0) {
        fourcc = 0x33545844;
        n = 2;
    } else if (std::strcmp(fmt, "DXT5") == 0) {
        fourcc = 0x35545844;
        n = 3;
    } else if (std::strcmp(fmt, "BC2") == 0) {
        fourcc = 0x30315844;
        dx10 = true;
        dxgi = 73;
        n = 2;
    } else if (std::strcmp(fmt, "BC3") == 0) {
        fourcc = 0x30315844;
        dx10 = true;
        dxgi = 76;
        n = 3;
    } else if (std::strcmp(fmt, "BC5") == 0) {
        fourcc = 0x30315844;
        dx10 = true;
        dxgi = 82;
        n = 5;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        const std::uint32_t bitcount = static_cast<std::uint32_t>(channels) * 8u;
        std::uint32_t flags = 0x1u | 0x2u | 0x4u | 0x1000u;
        std::uint32_t pitch = 0;
        std::uint32_t pfflags = 0;
        std::uint32_t masks[4] = {0, 0, 0, 0};
        const bool alpha = image->mode == PILLOW_C_MODE_RGBA || image->mode == PILLOW_C_MODE_LA;
        if (fmt[0] == 0) {
            flags |= 0x8u;
            pitch = (static_cast<std::uint32_t>(image->width) * bitcount + 7u) / 8u;
            if (image->mode == PILLOW_C_MODE_L || image->mode == PILLOW_C_MODE_LA) {
                pfflags = 0x20000u;
                masks[0] = masks[1] = masks[2] = alpha ? 0xFFu : 0xFF000000u;
            } else {
                pfflags = 0x40u;
                masks[0] = 0x00FF0000u;
                masks[1] = 0x0000FF00u;
                masks[2] = 0x000000FFu;
            }
            if (alpha) {
                pfflags |= 0x1u;
            }
            masks[3] = alpha ? 0xFF000000u : 0u;
        } else {
            flags |= 0x80000u;
            pitch = (static_cast<std::uint32_t>(image->width) + 3u) * 4u;
            pfflags = 0x4u;
        }

        std::vector<std::uint8_t> out;
        const std::size_t payload_bytes = fmt[0] == 0
            ? static_cast<std::size_t>(pitch) * static_cast<std::size_t>(image->height)
            : static_cast<std::size_t>((image->width + 3) / 4) *
                static_cast<std::size_t>((image->height + 3) / 4) *
                (n == 1 ? 8u : 16u);
        out.reserve(148u + payload_bytes);
        append_le32(out, 0x20534444u);
        append_le32(out, 124u);
        append_le32(out, flags);
        append_le32(out, static_cast<std::uint32_t>(image->height));
        append_le32(out, static_cast<std::uint32_t>(image->width));
        append_le32(out, pitch);
        append_le32(out, 0u);
        append_le32(out, 0u);
        for (int i = 0; i < 11; ++i) {
            append_le32(out, 0u);
        }
        append_le32(out, 32u);
        append_le32(out, pfflags);
        append_le32(out, fourcc);
        append_le32(out, bitcount);
        for (int i = 0; i < 4; ++i) {
            append_le32(out, masks[i]);
        }
        append_le32(out, 0x1000u);
        for (int i = 0; i < 4; ++i) {
            append_le32(out, 0u);
        }
        if (dx10) {
            append_le32(out, dxgi);
            append_le32(out, 3u);
            append_le32(out, 0u);
            append_le32(out, 0u);
            append_le32(out, 1u);
        }

        if (fmt[0] == 0) {
            for (int y = 0; y < image->height; ++y) {
                const std::uint8_t* src_row =
                    image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < image->width; ++x) {
                    const std::uint8_t* px =
                        src_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
                    if (channels == 1) {
                        out.push_back(px[0]);
                    } else if (channels == 2) {
                        out.push_back(px[0]);
                        out.push_back(px[1]);
                    } else if (channels == 3) {
                        out.push_back(px[2]);
                        out.push_back(px[1]);
                        out.push_back(px[0]);
                    } else {
                        // Pillow merges (a, r, g, b) bands and the raw
                        // encoder reverses them: B, G, R, A per pixel.
                        out.push_back(px[2]);
                        out.push_back(px[1]);
                        out.push_back(px[0]);
                        out.push_back(px[3]);
                    }
                }
            }
        } else {
            const bool has_alpha = channels == 4 || channels == 2;
            for (int by = 0; by < image->height; by += 4) {
                for (int bx = 0; bx < image->width; bx += 4) {
                    if (n == 5) {
                        std::uint8_t block[16];
                        dds_encode_bc3_alpha(image, bx, by, block, 0);
                        dds_encode_bc3_alpha(image, bx, by, block + 8, 1);
                        out.insert(out.end(), block, block + 16);
                    } else {
                        if (n == 2 || n == 3) {
                            if (has_alpha) {
                                std::uint8_t block[8];
                                if (n == 2) {
                                    dds_encode_bc2_block(image, bx, by, block);
                                } else {
                                    dds_encode_bc3_alpha(image, bx, by, block, 3);
                                }
                                out.insert(out.end(), block, block + 8);
                            } else {
                                for (int i = 0; i < 8; i++) {
                                    out.push_back(0xff);
                                }
                            }
                        }
                        std::uint8_t block[8];
                        dds_encode_bc1_block(image, bx, by, block, n == 1 && has_alpha);
                        out.insert(out.end(), block, block + 8);
                    }
                }
            }
        }

        if (!write_binary_file(path, out)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_dds_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 8u || std::memcmp(data.data(), "DDS ", 4) != 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t header_size = read_le32(data.data() + 4u);
        if (header_size != 124u) {
            return PILLOW_C_DDS_HDR_SIZE;
        }
        if (data.size() < 128u) {
            return PILLOW_C_DDS_HDR_SHORT;
        }
        const int height = static_cast<int>(read_le32(data.data() + 12u));
        const int width = static_cast<int>(read_le32(data.data() + 16u));
        const std::uint32_t pfflags = read_le32(data.data() + 80u);
        const std::uint32_t fourcc = read_le32(data.data() + 84u);
        const std::uint32_t bitcount = read_le32(data.data() + 88u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        int mode = 0;
        int channels = 0;
        int n = 0;
        int sign = 0;
        std::size_t data_offset = 128u;
        int kind = 0;  // 0 raw, 1 masks, 2 p8, 3 bcn
        std::uint32_t masks[4] = {0, 0, 0, 0};
        if ((pfflags & 0x40u) != 0u) {
            kind = 1;
            const int mask_count = (pfflags & 0x1u) != 0u ? 4 : 3;
            channels = mask_count;
            mode = mask_count == 4 ? PILLOW_C_MODE_RGBA : PILLOW_C_MODE_RGB;
            for (int i = 0; i < mask_count; ++i) {
                masks[i] = read_le32(data.data() + 92u + static_cast<std::size_t>(i) * 4u);
            }
        } else if ((pfflags & 0x20000u) != 0u) {
            kind = 0;
            if (bitcount == 8u) {
                mode = PILLOW_C_MODE_L;
                channels = 1;
            } else if (bitcount == 16u && (pfflags & 0x1u) != 0u) {
                mode = PILLOW_C_MODE_LA;
                channels = 2;
            } else {
                return PILLOW_C_DDS_BITCOUNT;
            }
        } else if ((pfflags & 0x20u) != 0u) {
            kind = 2;
            mode = PILLOW_C_MODE_P;
            channels = 1;
        } else if ((pfflags & 0x4u) != 0u) {
            kind = 3;
            if (fourcc == 0x31545844u) {
                mode = PILLOW_C_MODE_RGBA;
                channels = 4;
                n = 1;
            } else if (fourcc == 0x33545844u) {
                mode = PILLOW_C_MODE_RGBA;
                channels = 4;
                n = 2;
            } else if (fourcc == 0x35545844u) {
                mode = PILLOW_C_MODE_RGBA;
                channels = 4;
                n = 3;
            } else if (fourcc == 0x55344342u || fourcc == 0x31495441u) {
                mode = PILLOW_C_MODE_L;
                channels = 1;
                n = 4;
            } else if (fourcc == 0x53354342u) {
                mode = PILLOW_C_MODE_RGB;
                channels = 3;
                n = 5;
                sign = 1;
            } else if (fourcc == 0x55354342u || fourcc == 0x32495441u) {
                mode = PILLOW_C_MODE_RGB;
                channels = 3;
                n = 5;
            } else if (fourcc == 0x30315844u) {
                data_offset = 148u;
                if (data.size() < 148u) {
                    return PILLOW_C_DDS_TRUNC16;
                }
                const std::uint32_t dxgi = read_le32(data.data() + 128u);
                if (dxgi == 70u || dxgi == 71u) {
                    mode = PILLOW_C_MODE_RGBA;
                    channels = 4;
                    n = 1;
                } else if (dxgi == 73u || dxgi == 74u) {
                    mode = PILLOW_C_MODE_RGBA;
                    channels = 4;
                    n = 2;
                } else if (dxgi == 76u || dxgi == 77u) {
                    mode = PILLOW_C_MODE_RGBA;
                    channels = 4;
                    n = 3;
                } else if (dxgi == 79u || dxgi == 80u) {
                    mode = PILLOW_C_MODE_L;
                    channels = 1;
                    n = 4;
                } else if (dxgi == 82u || dxgi == 83u) {
                    mode = PILLOW_C_MODE_RGB;
                    channels = 3;
                    n = 5;
                } else if (dxgi == 84u) {
                    mode = PILLOW_C_MODE_RGB;
                    channels = 3;
                    n = 5;
                    sign = 1;
                } else if (dxgi == 97u || dxgi == 98u || dxgi == 99u) {
                    mode = PILLOW_C_MODE_RGBA;
                    channels = 4;
                    n = 7;
                } else if (dxgi == 27u || dxgi == 28u || dxgi == 29u) {
                    kind = 0;
                    mode = PILLOW_C_MODE_RGBA;
                    channels = 4;
                } else {
                    return PILLOW_C_DDS_DXGI;
                }
            } else {
                return PILLOW_C_DDS_FOURCC;
            }
        } else {
            return PILLOW_C_DDS_PFFLAGS;
        }

        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};

        if (kind == 1) {
            // dds_rgb mask decode: bitcount/8 bytes per pixel (LE), mask
            // offset/total scaling with Python's float division then int().
            for (int i = 0; i < channels; ++i) {
                std::uint64_t m = masks[i];
                int offset = 0;
                while (m != 0u && ((m >> (offset + 1)) << (offset + 1)) == m) {
                    ++offset;
                }
                const std::uint64_t total = m >> offset;
                if (total == 0u) {
                    delete image;
                    return PILLOW_C_DDS_ZERO_MASK;
                }
            }
            const std::size_t bytecount = bitcount / 8u;
            std::size_t pos = data_offset;
            const std::size_t pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            std::vector<std::uint8_t> scaled;
            scaled.reserve(pixel_count * static_cast<std::size_t>(channels));
            while (scaled.size() < pixel_count * static_cast<std::size_t>(channels)) {
                std::uint64_t value = 0;
                for (std::size_t b = 0; b < bytecount; ++b) {
                    if (pos < data.size()) {
                        value |= static_cast<std::uint64_t>(data[pos]) << (8u * b);
                        ++pos;
                    }
                }
                for (int i = 0; i < channels; ++i) {
                    std::uint64_t m = masks[i];
                    int offset = 0;
                    while (m != 0u && ((m >> (offset + 1)) << (offset + 1)) == m) {
                        ++offset;
                    }
                    const std::uint64_t total = m >> offset;
                    const std::uint64_t masked = value & m;
                    const double scaled_value =
                        (static_cast<double>(masked >> offset) / static_cast<double>(total)) * 255.0;
                    scaled.push_back(static_cast<std::uint8_t>(static_cast<int>(scaled_value)));
                }
            }
            std::memcpy(image->pixels.data(), scaled.data(), scaled.size());
        } else if (kind == 2) {
            // P8: 1024-byte RGBA palette then 8-bit indices.
            const std::size_t palette_need = data_offset + 1024u;
            image->palette_rgb.assign(256u * 3u, std::uint8_t{0});
            image->palette_alpha.assign(256u, std::uint8_t{0});
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_RGBA;
            for (int i = 0; i < 256; ++i) {
                const std::size_t off = data_offset + static_cast<std::size_t>(i) * 4u;
                if (off + 3u < data.size()) {
                    image->palette_rgb[static_cast<std::size_t>(i) * 3u + 0u] = data[off];
                    image->palette_rgb[static_cast<std::size_t>(i) * 3u + 1u] = data[off + 1u];
                    image->palette_rgb[static_cast<std::size_t>(i) * 3u + 2u] = data[off + 2u];
                    image->palette_alpha[static_cast<std::size_t>(i)] = data[off + 3u];
                }
            }
            const std::size_t payload_offset = palette_need;
            const std::size_t need = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            if (data.size() < payload_offset + need) {
                delete image;
                return PILLOW_C_DDS_TRUNC_RAW;
            }
            std::memcpy(image->pixels.data(), data.data() + payload_offset, need);
        } else if (kind == 3) {
            const std::size_t blocksize = (n == 1 || n == 4) ? 8u : 16u;
            const std::size_t block_xs = (static_cast<std::size_t>(width) + 3u) / 4u;
            const std::size_t block_ys = (static_cast<std::size_t>(height) + 3u) / 4u;
            const std::size_t need = block_xs * block_ys * blocksize;
            if (data.size() < data_offset + need) {
                delete image;
                return blocksize == 8u ? PILLOW_C_DDS_TRUNC8 : PILLOW_C_DDS_TRUNC16;
            }
            const std::uint8_t* ptr = data.data() + data_offset;
            const std::size_t payload_size = data.size() - data_offset;
            int block_index = 0;
            for (int by = 0; by < height; by += 4) {
                for (int bx = 0; bx < width; bx += 4) {
                    const std::size_t off = static_cast<std::size_t>(block_index) * blocksize;
                    dds_rgba col[16];
                    std::memset(col, n == 5 && sign ? 128 : 0, sizeof(col));
                    switch (n) {
                        case 1:
                            dds_decode_bc1_color(col, ptr + off, 0);
                            break;
                        case 2:
                            dds_decode_bc2_block(col, ptr + off);
                            break;
                        case 3:
                            dds_decode_bc3_block(col, ptr + off);
                            break;
                        case 4:
                            for (int i = 0; i < 16; ++i) {
                                col[i].c[0] = 0;
                                col[i].c[1] = 0;
                                col[i].c[2] = 0;
                                col[i].c[3] = 0;
                            }
                            {
                                std::uint8_t lum[16];
                                dds_decode_bc4_block(lum, ptr + off);
                                for (int i = 0; i < 16; ++i) {
                                    col[i].c[0] = lum[i];
                                    col[i].c[1] = 0;
                                    col[i].c[2] = 0;
                                }
                            }
                            break;
                        case 5:
                            dds_decode_bc5_block(col, ptr + off, sign);
                            break;
                        case 7:
                            dds_decode_bc7_block(col, ptr + off, payload_size - off);
                            break;
                        default:
                            delete image;
                            return PILLOW_C_INVALID_ARGUMENT;
                    }
                    // BC4 col carries the luminance in c[0].
                    if (n != 4) {
                        dds_bcn_put_block(image, bx, by, col, channels);
                    } else {
                        for (int j = 0; j < 4; ++j) {
                            const int y = by + j;
                            if (y >= height) {
                                continue;
                            }
                            std::uint8_t* dst_row =
                                image->pixels.data() + static_cast<std::size_t>(y) * image_stride;
                            for (int i = 0; i < 4; ++i) {
                                const int x = bx + i;
                                if (x >= width) {
                                    continue;
                                }
                                dst_row[x] = col[j * 4 + i].c[0];
                            }
                        }
                    }
                    ++block_index;
                }
            }
        } else {
            // raw L/LA/RGBA tile, tight top-down rows.
            const std::size_t need = static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height) * static_cast<std::size_t>(channels);
            if (data.size() < data_offset + need) {
                delete image;
                return PILLOW_C_DDS_TRUNC_RAW;
            }
            std::memcpy(image->pixels.data(), data.data() + data_offset, need);
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

struct IcoDirectoryEntryInfo {
    std::uint8_t width_byte = 0;
    std::uint8_t height_byte = 0;
    std::uint8_t color_count = 0;
    std::uint8_t reserved = 0;
    std::uint16_t planes = 0;
    std::uint16_t bit_count = 0;
    std::uint32_t bytes_in_resource = 0;
    std::uint32_t image_offset = 0;
    int width = 0;
    int height = 0;
    int color_depth = 0;
    std::uint32_t square = 0;
    std::size_t original_index = 0;
};

int ico_directory_color_depth(std::uint8_t color_count, std::uint16_t bit_count)
{
    if (bit_count != 0) {
        return static_cast<int>(bit_count);
    }
    if (color_count != 0) {
        int depth = 0;
        int colors = static_cast<int>(color_count) - 1;
        while (colors > 0) {
            ++depth;
            colors >>= 1;
        }
        if (depth != 0) {
            return depth;
        }
    }
    return 256;
}

bool parse_icon_directory_entries(
    const std::vector<std::uint8_t>& data,
    std::uint16_t expected_type,
    std::vector<IcoDirectoryEntryInfo>* out_entries)
{
    if (!out_entries || data.size() < 6u ||
        read_le16(data.data()) != 0u ||
        read_le16(data.data() + 2u) != expected_type) {
        return false;
    }
    const std::uint16_t count = read_le16(data.data() + 4u);
    if (count == 0u ||
        static_cast<std::size_t>(count) > (std::numeric_limits<std::size_t>::max() - 6u) / 16u) {
        return false;
    }
    const std::size_t directory_size = 6u + static_cast<std::size_t>(count) * 16u;
    if (directory_size > data.size()) {
        return false;
    }

    std::vector<IcoDirectoryEntryInfo> entries;
    entries.reserve(count);
    for (std::size_t index = 0; index < static_cast<std::size_t>(count); ++index) {
        const std::size_t offset = 6u + index * 16u;
        IcoDirectoryEntryInfo entry{};
        entry.width_byte = data[offset];
        entry.height_byte = data[offset + 1u];
        entry.color_count = data[offset + 2u];
        entry.reserved = data[offset + 3u];
        entry.planes = read_le16(data.data() + offset + 4u);
        entry.bit_count = read_le16(data.data() + offset + 6u);
        entry.bytes_in_resource = read_le32(data.data() + offset + 8u);
        entry.image_offset = read_le32(data.data() + offset + 12u);
        entry.width = entry.width_byte == 0u ? 256 : static_cast<int>(entry.width_byte);
        entry.height = entry.height_byte == 0u ? 256 : static_cast<int>(entry.height_byte);
        entry.color_depth = ico_directory_color_depth(entry.color_count, entry.bit_count);
        entry.square = static_cast<std::uint32_t>(entry.width * entry.height);
        entry.original_index = index;

        const std::size_t image_offset = static_cast<std::size_t>(entry.image_offset);
        const std::size_t image_size = static_cast<std::size_t>(entry.bytes_in_resource);
        if (image_size == 0u || image_offset > data.size() || image_size > data.size() - image_offset) {
            return false;
        }
        entries.push_back(entry);
    }

    *out_entries = std::move(entries);
    return true;
}

bool parse_ico_directory_entries(
    const std::vector<std::uint8_t>& data,
    std::vector<IcoDirectoryEntryInfo>* out_entries)
{
    return parse_icon_directory_entries(data, 1u, out_entries);
}

bool parse_cur_directory_entries(
    const std::vector<std::uint8_t>& data,
    std::vector<IcoDirectoryEntryInfo>* out_entries)
{
    return parse_icon_directory_entries(data, 2u, out_entries);
}

bool choose_pillow_ico_directory_entry(
    const std::vector<IcoDirectoryEntryInfo>& entries,
    int requested_width,
    int requested_height,
    bool require_requested_size,
    IcoDirectoryEntryInfo* out_entry)
{
    if (!out_entry || entries.empty()) {
        return false;
    }

    std::vector<IcoDirectoryEntryInfo> sorted = entries;
    std::stable_sort(sorted.begin(), sorted.end(), [](const IcoDirectoryEntryInfo& left, const IcoDirectoryEntryInfo& right) {
        return left.color_depth < right.color_depth;
    });
    std::stable_sort(sorted.begin(), sorted.end(), [](const IcoDirectoryEntryInfo& left, const IcoDirectoryEntryInfo& right) {
        return left.square > right.square;
    });

    for (const IcoDirectoryEntryInfo& entry : sorted) {
        if (!require_requested_size ||
            (entry.width == requested_width && entry.height == requested_height)) {
            *out_entry = entry;
            return true;
        }
    }
    return false;
}

bool build_single_entry_ico_bytes(
    const std::vector<std::uint8_t>& source,
    const IcoDirectoryEntryInfo& entry,
    std::vector<std::uint8_t>* out)
{
    if (!out) {
        return false;
    }
    const std::size_t image_offset = static_cast<std::size_t>(entry.image_offset);
    const std::size_t image_size = static_cast<std::size_t>(entry.bytes_in_resource);
    if (image_size == 0u || image_offset > source.size() || image_size > source.size() - image_offset ||
        image_size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() - 22u)) {
        return false;
    }

    std::vector<std::uint8_t> ico;
    ico.reserve(22u + image_size);
    append_le16(ico, 0u);
    append_le16(ico, 1u);
    append_le16(ico, 1u);
    ico.push_back(entry.width_byte);
    ico.push_back(entry.height_byte);
    ico.push_back(entry.color_count);
    ico.push_back(entry.reserved);
    append_le16(ico, entry.planes);
    append_le16(ico, entry.bit_count);
    append_le32(ico, entry.bytes_in_resource);
    append_le32(ico, 22u);
    ico.insert(ico.end(), source.begin() + image_offset, source.begin() + image_offset + image_size);
    *out = std::move(ico);
    return true;
}

bool ico_entry_payload_is_png(const std::vector<std::uint8_t>& source, const IcoDirectoryEntryInfo& entry)
{
    static constexpr std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    const std::size_t image_offset = static_cast<std::size_t>(entry.image_offset);
    const std::size_t image_size = static_cast<std::size_t>(entry.bytes_in_resource);
    return image_offset <= source.size() &&
           image_size >= sizeof(signature) &&
           image_size <= source.size() - image_offset &&
           std::memcmp(source.data() + image_offset, signature, sizeof(signature)) == 0;
}

bool read_ico_entry_dib_bit_count(
    const std::vector<std::uint8_t>& source,
    const IcoDirectoryEntryInfo& entry,
    std::uint16_t* out_bit_count)
{
    if (!out_bit_count) {
        return false;
    }
    *out_bit_count = 0;
    const std::size_t image_offset = static_cast<std::size_t>(entry.image_offset);
    const std::size_t image_size = static_cast<std::size_t>(entry.bytes_in_resource);
    if (image_offset > source.size() || image_size < 16u || image_size > source.size() - image_offset) {
        return false;
    }
    const std::uint8_t* dib = source.data() + image_offset;
    const std::uint32_t header_size = read_le32(dib);
    if (header_size < 16u || header_size > image_size) {
        return false;
    }
    *out_bit_count = read_le16(dib + 14u);
    return *out_bit_count != 0u;
}

bool read_ico_entry_dib_metadata(
    const std::vector<std::uint8_t>& source,
    const IcoDirectoryEntryInfo& entry,
    bool* out_has_dpi,
    double* out_dpi_x,
    double* out_dpi_y,
    int* out_compression)
{
    if (!out_has_dpi || !out_dpi_x || !out_dpi_y || !out_compression) {
        return false;
    }
    *out_has_dpi = false;
    *out_dpi_x = 0.0;
    *out_dpi_y = 0.0;
    *out_compression = -1;

    const std::size_t image_offset = static_cast<std::size_t>(entry.image_offset);
    const std::size_t image_size = static_cast<std::size_t>(entry.bytes_in_resource);
    if (image_offset > source.size() || image_size < 40u || image_size > source.size() - image_offset) {
        return false;
    }

    const std::uint8_t* dib = source.data() + image_offset;
    const std::uint32_t header_size = read_le32(dib);
    if (header_size < 40u || header_size > image_size) {
        return false;
    }

    const std::uint32_t compression = read_le32(dib + 16u);
    if (compression > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    *out_compression = static_cast<int>(compression);

    const std::int32_t x_pels_per_meter = static_cast<std::int32_t>(read_le32(dib + 24u));
    const std::int32_t y_pels_per_meter = static_cast<std::int32_t>(read_le32(dib + 28u));
    if (x_pels_per_meter > 0 && y_pels_per_meter > 0) {
        *out_has_dpi = true;
        *out_dpi_x = static_cast<double>(x_pels_per_meter) * 0.0254;
        *out_dpi_y = static_cast<double>(y_pels_per_meter) * 0.0254;
    }
    return true;
}

int ico_payload_format(
    const char* path,
    int requested_width,
    int requested_height,
    bool require_requested_size,
    char* out_format,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!path || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;
    if (require_requested_size && (requested_width <= 0 || requested_height <= 0)) {
        return PILLOW_C_INVALID_ARGUMENT;

    }

    try {
        std::vector<std::uint8_t> ico_file;
        if (!read_binary_file(path, &ico_file)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<IcoDirectoryEntryInfo> entries;
        if (!parse_ico_directory_entries(ico_file, &entries)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        IcoDirectoryEntryInfo selected_entry{};
        if (!choose_pillow_ico_directory_entry(
                entries,
                requested_width,
                requested_height,
                require_requested_size,
                &selected_entry)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const char* format = "";
        if (ico_entry_payload_is_png(ico_file, selected_entry)) {
            format = "PNG";
        }
        const std::size_t required = std::strlen(format) == 0u ? 0u : std::strlen(format) + 1u;
        *out_required = required;
        if (required == 0u || !out_format) {
            return PILLOW_C_OK;
        }
        if (out_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out_format, format, required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int ico_payload_dib_metadata(
    const char* path,
    int requested_width,
    int requested_height,
    bool require_requested_size,
    int* out_has_dib,
    int* out_has_dpi,
    double* out_dpi_x,
    double* out_dpi_y,
    int* out_compression)
{
    if (!path || !out_has_dib || !out_has_dpi || !out_dpi_x || !out_dpi_y || !out_compression) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_dib = 0;
    *out_has_dpi = 0;
    *out_dpi_x = 0.0;
    *out_dpi_y = 0.0;
    *out_compression = -1;
    if (require_requested_size && (requested_width <= 0 || requested_height <= 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> ico_file;
        if (!read_binary_file(path, &ico_file)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<IcoDirectoryEntryInfo> entries;
        if (!parse_ico_directory_entries(ico_file, &entries)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        IcoDirectoryEntryInfo selected_entry{};
        if (!choose_pillow_ico_directory_entry(
                entries,
                requested_width,
                requested_height,
                require_requested_size,
                &selected_entry)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        if (ico_entry_payload_is_png(ico_file, selected_entry)) {
            return PILLOW_C_OK;
        }

        bool has_dpi = false;
        double dpi_x = 0.0;
        double dpi_y = 0.0;
        int compression = -1;
        if (!read_ico_entry_dib_metadata(ico_file, selected_entry, &has_dpi, &dpi_x, &dpi_y, &compression)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_has_dib = 1;
        *out_has_dpi = has_dpi ? 1 : 0;
        *out_dpi_x = dpi_x;
        *out_dpi_y = dpi_y;
        *out_compression = compression;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int decode_single_entry_ico_bytes(std::vector<std::uint8_t>& selected_ico, PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (selected_ico.empty() || selected_ico.size() > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        ComInitScope com;
        if (!com.usable()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ComPtr<IWICImagingFactory> factory;
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }

        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = stream->InitializeFromMemory(
            selected_ico.data(),
            static_cast<DWORD>(selected_ico.size()));
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = factory->CreateDecoderFromStream(
            stream.get(),
            nullptr,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID container = {};
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatIco)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || frame_count == 0) {
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
        if (FAILED(hr) ||
            width_u > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height_u > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int width = static_cast<int>(width_u);
        const int height = static_cast<int>(height_u);
        if (width <= 0 || height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 4, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, GUID_WICPixelFormat32bppRGBA)) {
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
                GUID_WICPixelFormat32bppRGBA,
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
            PILLOW_C_MODE_RGBA,
            4,
            stride,
            std::vector<std::uint8_t>(size)};

        hr = source->CopyPixels(
            nullptr,
            static_cast<UINT>(stride),
            static_cast<UINT>(image->pixels.size()),
            image->pixels.data());
        if (FAILED(hr)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_ico_image_size(const char* path, int requested_width, int requested_height, bool require_requested_size, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (require_requested_size && (requested_width <= 0 || requested_height <= 0)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> ico_file;
        if (!read_binary_file(path, &ico_file)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<IcoDirectoryEntryInfo> entries;
        if (!parse_ico_directory_entries(ico_file, &entries)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        IcoDirectoryEntryInfo selected_entry{};
        if (!choose_pillow_ico_directory_entry(
                entries,
                requested_width,
                requested_height,
                require_requested_size,
                &selected_entry)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<std::uint8_t> selected_ico;
        if (!build_single_entry_ico_bytes(ico_file, selected_entry, &selected_ico)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return decode_single_entry_ico_bytes(selected_ico, out_image);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_ico_image(const char* path, PillowCImage** out_image)
{
    return open_ico_image_size(path, 0, 0, false, out_image);
}

int open_cur_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> cur_file;
        if (!read_binary_file(path, &cur_file)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::vector<IcoDirectoryEntryInfo> entries;
        if (!parse_cur_directory_entries(cur_file, &entries)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        IcoDirectoryEntryInfo selected_entry{};
        if (!choose_pillow_ico_directory_entry(entries, 0, 0, false, &selected_entry)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (ico_entry_payload_is_png(cur_file, selected_entry)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::uint16_t bit_count = 0;
        if (!read_ico_entry_dib_bit_count(cur_file, selected_entry, &bit_count)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        IcoDirectoryEntryInfo ico_entry = selected_entry;
        ico_entry.planes = 0;
        ico_entry.bit_count = bit_count;
        std::vector<std::uint8_t> selected_ico;
        if (!build_single_entry_ico_bytes(cur_file, ico_entry, &selected_ico)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        PillowCImage* image = nullptr;
        int status = decode_single_entry_ico_bytes(selected_ico, &image);
        if (status != PILLOW_C_OK) {
            return status;
        }
        bool has_dpi = false;
        double dpi_x = 0.0;
        double dpi_y = 0.0;
        int compression = -1;
        if (!read_ico_entry_dib_metadata(cur_file, selected_entry, &has_dpi, &dpi_x, &dpi_y, &compression)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        image->has_dpi = has_dpi;
        image->dpi_x = dpi_x;
        image->dpi_y = dpi_y;
        image->has_dib_compression = true;
        image->dib_compression = compression;
        image->has_hotspot = true;
        image->hotspot_x = static_cast<int>(selected_entry.planes);
        image->hotspot_y = static_cast<int>(selected_entry.bit_count);
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int ico_image_sizes(const char* path, int* out_sizes, std::size_t out_pair_count, std::size_t* out_required)
{
    if (!path || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;

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
        int status = create_wic_factory(&factory);
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
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatIco)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::pair<int, int>> sizes;
        sizes.reserve(frame_count);
        for (UINT index = 0; index < frame_count; ++index) {
            ComPtr<IWICBitmapFrameDecode> frame;
            hr = decoder->GetFrame(index, frame.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            UINT width = 0;
            UINT height = 0;
            hr = frame->GetSize(&width, &height);
            if (FAILED(hr) ||
                width > static_cast<UINT>(std::numeric_limits<int>::max()) ||
                height > static_cast<UINT>(std::numeric_limits<int>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            sizes.emplace_back(static_cast<int>(width), static_cast<int>(height));
        }

        std::sort(sizes.begin(), sizes.end());
        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
        *out_required = sizes.size();
        if (!out_sizes) {
            return PILLOW_C_OK;
        }
        if (out_pair_count < sizes.size()) {
            return PILLOW_C_INVALID_LENGTH;
        }
        for (std::size_t index = 0; index < sizes.size(); ++index) {
            out_sizes[index * 2u] = sizes[index].first;
            out_sizes[index * 2u + 1u] = sizes[index].second;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

struct IcoRequestedSize {
    int width;
    int height;
};

struct IcoResource {
    int width;
    int height;
    int bit_count;
    int color_count;
    std::vector<std::uint8_t> payload;
};

int ico_dib_mode_bit_count(const PillowCImage* image, int* out_bit_count)
{
    if (!image || !out_bit_count) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->mode == PILLOW_C_MODE_1) {
        *out_bit_count = 1;
    } else if (image->mode == PILLOW_C_MODE_L || image->mode == PILLOW_C_MODE_P) {
        *out_bit_count = 8;
    } else if (image->mode == PILLOW_C_MODE_RGB) {
        *out_bit_count = 24;
    } else if (image->mode == PILLOW_C_MODE_RGBA) {
        *out_bit_count = 32;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

int encode_ico_dib_image(const PillowCImage* image, std::vector<std::uint8_t>* out, int* out_bit_count, int* out_color_count)
{
    if (!image || !out || !out_bit_count || !out_color_count) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 || image->width > 256 || image->height > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int bits_per_pixel = 0;
    int dib_color_count = 0;
    int directory_color_count = 0;
    std::size_t palette_entries = 0;
    if (image->mode == PILLOW_C_MODE_1) {
        bits_per_pixel = 1;
        dib_color_count = 2;
        directory_color_count = 2;
        palette_entries = 2;
    } else if (image->mode == PILLOW_C_MODE_L) {
        bits_per_pixel = 8;
        dib_color_count = 256;
        directory_color_count = 0;
        palette_entries = 256;
    } else if (image->mode == PILLOW_C_MODE_P) {
        if (image->palette_rgb.size() > 256u * 3u || image->palette_rgb.size() % 3u != 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        bits_per_pixel = 8;
        dib_color_count = static_cast<int>(image->palette_rgb.size() / 3u);
        directory_color_count = 0;
        palette_entries = static_cast<std::size_t>(dib_color_count);
    } else if (image->mode == PILLOW_C_MODE_RGB) {
        bits_per_pixel = 24;
        dib_color_count = 0;
        directory_color_count = 0;
    } else if (image->mode == PILLOW_C_MODE_RGBA) {
        bits_per_pixel = 32;
        dib_color_count = 0;
        directory_color_count = 0;
    } else {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t xor_stride = 0;
    int status = bmp_row_stride(image->width, bits_per_pixel, &xor_stride);
    if (status != PILLOW_C_OK) {
        return status;

    }
    const bool has_and_mask = bits_per_pixel != 32;
    std::size_t and_stride = 0;
    if (has_and_mask) {
        and_stride = (static_cast<std::size_t>(image->width) + 7u) / 8u;
    }

    const std::uint64_t xor_size_u64 =
        static_cast<std::uint64_t>(xor_stride) * static_cast<std::uint64_t>(image->height);
    const std::uint64_t and_size_u64 =
        static_cast<std::uint64_t>(and_stride) * static_cast<std::uint64_t>(image->height);
    const std::uint64_t palette_size_u64 = static_cast<std::uint64_t>(palette_entries) * 4u;
    const std::uint64_t total_size_u64 = 40u + palette_size_u64 + xor_size_u64 + and_size_u64;
    if (xor_size_u64 > std::numeric_limits<std::uint32_t>::max() ||
        total_size_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> dib;
        dib.reserve(static_cast<std::size_t>(total_size_u64));
        append_le32(dib, 40);
        append_le32(dib, static_cast<std::uint32_t>(image->width));
        append_le32(dib, static_cast<std::uint32_t>(image->height * 2));
        append_le16(dib, 1);
        append_le16(dib, static_cast<std::uint16_t>(bits_per_pixel));
        append_le32(dib, 0);
        append_le32(dib, static_cast<std::uint32_t>(xor_size_u64));
        append_le32(dib, 3780);
        append_le32(dib, 3780);
        append_le32(dib, static_cast<std::uint32_t>(dib_color_count));
        append_le32(dib, static_cast<std::uint32_t>(dib_color_count));

        if (image->mode == PILLOW_C_MODE_1) {
            dib.push_back(0);
            dib.push_back(0);
            dib.push_back(0);
            dib.push_back(0);
            dib.push_back(255);
            dib.push_back(255);
            dib.push_back(255);
            dib.push_back(0);
        } else if (image->mode == PILLOW_C_MODE_L) {
            for (int value = 0; value < 256; ++value) {
                const auto byte = static_cast<std::uint8_t>(value);
                dib.push_back(byte);
                dib.push_back(byte);
                dib.push_back(byte);
                dib.push_back(0);
            }
        } else if (image->mode == PILLOW_C_MODE_P) {
            for (std::size_t index = 0; index < palette_entries; ++index) {
                const std::size_t src = index * 3u;
                dib.push_back(image->palette_rgb[src + 2u]);
                dib.push_back(image->palette_rgb[src + 1u]);
                dib.push_back(image->palette_rgb[src + 0u]);
                dib.push_back(0);
            }
        }

        std::vector<std::uint8_t> row(xor_stride, std::uint8_t{0});
        for (int y = image->height - 1; y >= 0; --y) {
            std::fill(row.begin(), row.end(), std::uint8_t{0});
            const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            if (image->mode == PILLOW_C_MODE_1) {
                for (int x = 0; x < image->width; ++x) {
                    if (src_row[x] != 0) {
                        row[static_cast<std::size_t>(x) / 8u] |=
                            static_cast<std::uint8_t>(0x80u >> (static_cast<unsigned>(x) & 7u));
                    }
                }
            } else if (image->mode == PILLOW_C_MODE_L || image->mode == PILLOW_C_MODE_P) {
                std::memcpy(row.data(), src_row, static_cast<std::size_t>(image->width));
            } else if (image->mode == PILLOW_C_MODE_RGB) {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * 3u + 0u] = src_row[static_cast<std::size_t>(x) * 3u + 2u];
                    row[static_cast<std::size_t>(x) * 3u + 1u] = src_row[static_cast<std::size_t>(x) * 3u + 1u];
                    row[static_cast<std::size_t>(x) * 3u + 2u] = src_row[static_cast<std::size_t>(x) * 3u + 0u];
                }
            } else {
                for (int x = 0; x < image->width; ++x) {
                    row[static_cast<std::size_t>(x) * 4u + 0u] = src_row[static_cast<std::size_t>(x) * 4u + 2u];
                    row[static_cast<std::size_t>(x) * 4u + 1u] = src_row[static_cast<std::size_t>(x) * 4u + 1u];
                    row[static_cast<std::size_t>(x) * 4u + 2u] = src_row[static_cast<std::size_t>(x) * 4u + 0u];
                    row[static_cast<std::size_t>(x) * 4u + 3u] = src_row[static_cast<std::size_t>(x) * 4u + 3u];
                }
            }
            dib.insert(dib.end(), row.begin(), row.end());
        }

        if (has_and_mask) {
            dib.resize(dib.size() + static_cast<std::size_t>(and_size_u64), std::uint8_t{0});
        }

        *out_bit_count = bits_per_pixel;
        *out_color_count = directory_color_count;
        *out = std::move(dib);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_images_with_sizes(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const IcoRequestedSize* requested_sizes,
    std::size_t requested_count,
    bool bitmap_format_bmp)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (requested_count > 0 && !requested_sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    const PillowCImage* image = images[0];
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (std::size_t index = 1; index < image_count; ++index) {
        if (!images[index]) {
            return PILLOW_C_NULL_POINTER;
        }
    }

    if (requested_count == 0) {
        std::vector<std::uint8_t> ico;
        ico.reserve(6);
        append_le16(ico, 0);
        append_le16(ico, 1);
        append_le16(ico, 0);
        return write_binary_file(path, ico) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    }

    int status = PILLOW_C_OK;
    if (!bitmap_format_bmp) {
        for (std::size_t index = 0; index < image_count; ++index) {
            int color_type = 0;
            int payload_channels = 0;
            status = pillow_c_png_custom_mode_spec(images[index], &color_type, &payload_channels);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }
    }
    for (std::size_t index = 0; index < image_count; ++index) {
        const int refresh_status = pillow_c_refresh_const_buffer_view_image(images[index]);
        if (refresh_status != PILLOW_C_OK) {
            return refresh_status;
        }
    }

    try {
        std::vector<IcoRequestedSize> sizes;
        sizes.reserve(requested_count);
        for (std::size_t index = 0; index < requested_count; ++index) {
            const IcoRequestedSize requested = requested_sizes[index];
            if (requested.width <= 0 || requested.height <= 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            sizes.push_back(requested);
        }
        std::sort(sizes.begin(), sizes.end(), [](const IcoRequestedSize& left, const IcoRequestedSize& right) {
            if (left.width != right.width) {
                return left.width < right.width;
            }
            return left.height < right.height;
        });
        sizes.erase(
            std::unique(sizes.begin(), sizes.end(), [](const IcoRequestedSize& left, const IcoRequestedSize& right) {
                return left.width == right.width && left.height == right.height;
            }),
            sizes.end());

        std::vector<IcoResource> resources;
        resources.reserve(sizes.size());

        auto append_resource = [&](const PillowCImage* frame, int* out_bit_count) -> int {
            IcoResource resource{};
            resource.width = frame->width;
            resource.height = frame->height;
            resource.bit_count = 32;
            resource.color_count = 0;
            if (bitmap_format_bmp) {
                status = encode_ico_dib_image(frame, &resource.payload, &resource.bit_count, &resource.color_count);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            } else {
                status = pillow_c_png_encode_custom_image(frame, false, 0.0, 0.0, false, 0, false, 0, 0, 0, &resource.payload);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            }
            if (resource.width > 256 || resource.height > 256 ||
                resource.payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (out_bit_count) {
                *out_bit_count = resource.bit_count;
            }
            resources.push_back(std::move(resource));
            return PILLOW_C_OK;
        };

        for (const IcoRequestedSize& requested : sizes) {
            if (requested.width > image->width || requested.height > image->height ||
                requested.width > 256 || requested.height > 256) {
                continue;
            }

            bool used_exact_frame = false;
            std::vector<int> used_bit_counts;
            for (std::size_t image_index = 0; image_index < image_count; ++image_index) {
                const PillowCImage* provided = images[image_index];
                if (provided->width != requested.width || provided->height != requested.height) {
                    continue;
                }
                if (bitmap_format_bmp && used_exact_frame) {
                    int candidate_bit_count = 0;
                    status = ico_dib_mode_bit_count(provided, &candidate_bit_count);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    if (std::find(used_bit_counts.begin(), used_bit_counts.end(), candidate_bit_count) != used_bit_counts.end()) {
                        continue;
                    }
                }
                int appended_bit_count = 0;
                status = append_resource(provided, bitmap_format_bmp ? &appended_bit_count : nullptr);
                if (status != PILLOW_C_OK) {
                    return status;
                }
                used_exact_frame = true;
                if (bitmap_format_bmp) {
                    used_bit_counts.push_back(appended_bit_count);
                } else {
                    break;
                }
            }
            if (used_exact_frame) {
                continue;
            }

            const PillowCImage* resize_source = images[image_count - 1u];
            const PillowCImage* frame = resize_source;
            PillowCImage resized{};
            if (requested.width != resize_source->width || requested.height != resize_source->height) {
                int out_width = 0;
                int out_height = 0;
                status = pillow_c_proportional_resize_size(resize_source, requested.width, requested.height, false, &out_width, &out_height);
                if (status != PILLOW_C_OK) {
                    return status;
                }
                // Pillow's thumbnail() never upscales: the fallback frame is
                // capped at the last provided image's own dimensions.
                if (out_width > resize_source->width) {
                    out_width = resize_source->width;
                }
                if (out_height > resize_source->height) {
                    out_height = resize_source->height;
                }
                if (out_width != resize_source->width || out_height != resize_source->height) {
                    std::size_t stride = 0;
                    std::size_t size = 0;
                    if (!checked_image_size(out_width, out_height, resize_source->channels, &stride, &size)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    resized = PillowCImage{
                        out_width,
                        out_height,
                        resize_source->mode,
                        resize_source->channels,
                        stride,
                        std::vector<std::uint8_t>(size)};
                    status = pillow_c_resize_image_into(resize_source, out_width, out_height, PILLOW_C_LEGACY_RESAMPLE_LANCZOS, &resized);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    pillow_c_copy_palette_if_same_mode(resize_source, &resized);
                    frame = &resized;
                }
            }

            status = append_resource(frame, nullptr);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }

        if (resources.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::uint64_t total_size = 6u + static_cast<std::uint64_t>(resources.size()) * 16u;
        for (const IcoResource& resource : resources) {
            total_size += static_cast<std::uint64_t>(resource.payload.size());
            if (total_size > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }

        std::vector<std::uint8_t> ico;
        ico.reserve(static_cast<std::size_t>(total_size));
        append_le16(ico, 0);
        append_le16(ico, 1);
        append_le16(ico, static_cast<std::uint16_t>(resources.size()));

        std::uint32_t offset = static_cast<std::uint32_t>(6u + resources.size() * 16u);
        for (const IcoResource& resource : resources) {
            ico.push_back(static_cast<std::uint8_t>(resource.width == 256 ? 0 : resource.width));
            ico.push_back(static_cast<std::uint8_t>(resource.height == 256 ? 0 : resource.height));
            ico.push_back(static_cast<std::uint8_t>(resource.color_count >= 256 ? 0 : resource.color_count));
            ico.push_back(0);
            append_le16(ico, 0);
            append_le16(ico, static_cast<std::uint16_t>(resource.bit_count));
            append_le32(ico, static_cast<std::uint32_t>(resource.payload.size()));
            append_le32(ico, offset);
            offset += static_cast<std::uint32_t>(resource.payload.size());
        }
        for (const IcoResource& resource : resources) {
            ico.insert(ico.end(), resource.payload.begin(), resource.payload.end());
        }

        if (!write_binary_file(path, ico)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_image_with_sizes(
    const PillowCImage* image,
    const char* path,
    const IcoRequestedSize* requested_sizes,
    std::size_t requested_count,
    bool bitmap_format_bmp)
{
    const PillowCImage* images[] = {image};
    return save_ico_images_with_sizes(images, 1, path, requested_sizes, requested_count, bitmap_format_bmp);
}

int save_ico_image(const PillowCImage* image, const char* path)
{
    static constexpr IcoRequestedSize default_sizes[] = {
        {16, 16},
        {24, 24},
        {32, 32},
        {48, 48},
        {64, 64},
        {128, 128},
        {256, 256},
    };
    return save_ico_image_with_sizes(
        image,
        path,
        default_sizes,
        sizeof(default_sizes) / sizeof(default_sizes[0]),
        false);
}

int save_ico_image_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count)
{
    if (size_count > 0 && !sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    try {
        std::vector<IcoRequestedSize> requested;
        requested.reserve(size_count);
        for (std::size_t index = 0; index < size_count; ++index) {
            requested.push_back(IcoRequestedSize{
                sizes[index * 2],
                sizes[index * 2 + 1],
            });
        }
        return save_ico_image_with_sizes(
            image,
            path,
            requested.empty() ? nullptr : requested.data(),
            requested.size(),
            false);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_image_format_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count,
    int has_sizes,
    const char* bitmap_format)
{
    static constexpr IcoRequestedSize default_sizes[] = {
        {16, 16},
        {24, 24},
        {32, 32},
        {48, 48},
        {64, 64},
        {128, 128},
        {256, 256},
    };
    if (has_sizes && size_count > 0 && !sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool bitmap_format_bmp = bitmap_format && std::strcmp(bitmap_format, "bmp") == 0;
    try {
        std::vector<IcoRequestedSize> requested;
        if (has_sizes) {
            requested.reserve(size_count);
            for (std::size_t index = 0; index < size_count; ++index) {
                requested.push_back(IcoRequestedSize{
                    sizes[index * 2],
                    sizes[index * 2 + 1],
                });
            }
        }
        const IcoRequestedSize* requested_data = nullptr;
        std::size_t requested_count = 0;
        if (has_sizes) {
            requested_data = requested.empty() ? nullptr : requested.data();
            requested_count = requested.size();
        } else {
            requested_data = default_sizes;
            requested_count = sizeof(default_sizes) / sizeof(default_sizes[0]);
        }
        return save_ico_image_with_sizes(
            image,
            path,
            requested_data,
            requested_count,
            bitmap_format_bmp);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_ico_images_format_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* sizes,
    std::size_t size_count,
    int has_sizes,
    const char* bitmap_format)
{
    static constexpr IcoRequestedSize default_sizes[] = {
        {16, 16},
        {24, 24},
        {32, 32},
        {48, 48},
        {64, 64},
        {128, 128},
        {256, 256},
    };
    if (has_sizes && size_count > 0 && !sizes) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool bitmap_format_bmp = bitmap_format && std::strcmp(bitmap_format, "bmp") == 0;
    try {
        std::vector<IcoRequestedSize> requested;
        if (has_sizes) {
            requested.reserve(size_count);
            for (std::size_t index = 0; index < size_count; ++index) {
                requested.push_back(IcoRequestedSize{
                    sizes[index * 2],
                    sizes[index * 2 + 1],
                });
            }
        }
        const IcoRequestedSize* requested_data = nullptr;
        std::size_t requested_count = 0;
        if (has_sizes) {
            requested_data = requested.empty() ? nullptr : requested.data();
            requested_count = requested.size();
        } else {
            requested_data = default_sizes;
            requested_count = sizeof(default_sizes) / sizeof(default_sizes[0]);
        }
        return save_ico_images_with_sizes(
            images,
            image_count,
            path,
            requested_data,
            requested_count,
            bitmap_format_bmp);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_cur_image_with_hotspot(
    const PillowCImage* image,
    const char* path,
    bool has_hotspot,
    int hotspot_x,
    int hotspot_y)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 ||
        image->width > 256 || image->height > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_hotspot &&
        (hotspot_x < 0 || hotspot_x > 65535 || hotspot_y < 0 || hotspot_y > 65535)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int status = pillow_c_refresh_const_buffer_view_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }

    try {
        // Pillow 11.3.0's CUR reader only accepts DIB payloads, so the
        // cursor body uses the shared ICO DIB encoder.
        std::vector<std::uint8_t> payload;
        int bit_count = 0;
        int color_count = 0;
        status = encode_ico_dib_image(image, &payload, &bit_count, &color_count);
        if (status != PILLOW_C_OK) {
            return status;
        }
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> cur;
        cur.reserve(22u + payload.size());
        append_le16(cur, 0);
        append_le16(cur, 2);
        append_le16(cur, 1);
        cur.push_back(static_cast<std::uint8_t>(image->width));
        cur.push_back(static_cast<std::uint8_t>(image->height));
        cur.push_back(static_cast<std::uint8_t>(color_count >= 256 ? 0 : color_count));
        cur.push_back(0);
        // CUR entries carry the hotspot in the planes/bit_count fields.
        append_le16(cur, static_cast<std::uint16_t>(has_hotspot ? hotspot_x : 0));
        append_le16(cur, static_cast<std::uint16_t>(has_hotspot ? hotspot_y : 0));
        append_le32(cur, static_cast<std::uint32_t>(payload.size()));
        append_le32(cur, 22u);
        cur.insert(cur.end(), payload.begin(), payload.end());
        return write_binary_file(path, cur) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// ---------------------------------------------------------------------------
// BEHAV-ICNS-001: Apple Icon Image format over the native PNG seams.
//
// Pillow 11.3.0's IcnsImagePlugin writes the 8-byte "icns" header, a TOC
// chunk, and eight PNG-backed icon entries (ic07..ic14), and its open picks
// the lexicographically largest (width, height, scale) resource. Container
// parse failures surface as Pillow's generic identification error because
// Image.open wraps the plugin's SyntaxError, so this layer reports
// PILLOW_C_INVALID_ARGUMENT for those; payload-level failures keep distinct
// local statuses (-20..-26).
// ---------------------------------------------------------------------------

constexpr int PILLOW_C_ICNS_JP2K = -20;
constexpr int PILLOW_C_ICNS_SUBIMAGE = -21;
constexpr int PILLOW_C_ICNS_IT32_SIG = -22;
constexpr int PILLOW_C_ICNS_RLE = -23;
constexpr int PILLOW_C_ICNS_PNG_MODE = -24;
constexpr int PILLOW_C_ICNS_PNG_BAD = -25;
constexpr int PILLOW_C_ICNS_SAVE_MODE = -26;

constexpr int ICNS_READER_PNG_JP2 = 0;
constexpr int ICNS_READER_RGB32 = 1;
constexpr int ICNS_READER_RGB32T = 2;
constexpr int ICNS_READER_MASK = 3;

struct IcnsSlotReader {
    const char* type;
    int kind;
};

struct IcnsSlot {
    int width;
    int height;
    int scale;
    const IcnsSlotReader* readers;
    std::size_t reader_count;
};

const IcnsSlotReader ICNS_SLOT_IC10[] = {{"ic10", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IC09[] = {{"ic09", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IC14[] = {{"ic14", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IC08[] = {{"ic08", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IC13[] = {{"ic13", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IC07[] = {
    {"ic07", ICNS_READER_PNG_JP2},
    {"it32", ICNS_READER_RGB32T},
    {"t8mk", ICNS_READER_MASK},
};
const IcnsSlotReader ICNS_SLOT_ICP6[] = {{"icp6", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IC12[] = {{"ic12", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_IH32[] = {
    {"ih32", ICNS_READER_RGB32},
    {"h8mk", ICNS_READER_MASK},
};
const IcnsSlotReader ICNS_SLOT_ICP5[] = {
    {"icp5", ICNS_READER_PNG_JP2},
    {"il32", ICNS_READER_RGB32},
    {"l8mk", ICNS_READER_MASK},
};
const IcnsSlotReader ICNS_SLOT_IC11[] = {{"ic11", ICNS_READER_PNG_JP2}};
const IcnsSlotReader ICNS_SLOT_ICP4[] = {
    {"icp4", ICNS_READER_PNG_JP2},
    {"is32", ICNS_READER_RGB32},
    {"s8mk", ICNS_READER_MASK},
};

const IcnsSlot ICNS_SLOTS[] = {
    {512, 512, 2, ICNS_SLOT_IC10, 1},
    {512, 512, 1, ICNS_SLOT_IC09, 1},
    {256, 256, 2, ICNS_SLOT_IC14, 1},
    {256, 256, 1, ICNS_SLOT_IC08, 1},
    {128, 128, 2, ICNS_SLOT_IC13, 1},
    {128, 128, 1, ICNS_SLOT_IC07, 3},
    {64, 64, 1, ICNS_SLOT_ICP6, 1},
    {32, 32, 2, ICNS_SLOT_IC12, 1},
    {48, 48, 1, ICNS_SLOT_IH32, 2},
    {32, 32, 1, ICNS_SLOT_ICP5, 3},
    {16, 16, 2, ICNS_SLOT_IC11, 1},
    {16, 16, 1, ICNS_SLOT_ICP4, 3},
};

struct IcnsChunk {
    std::string type;
    std::vector<std::uint8_t> payload;
};

const IcnsChunk* find_icns_chunk(const std::vector<IcnsChunk>& chunks, const char type[4])
{
    for (const IcnsChunk& chunk : chunks) {
        if (chunk.type.size() == 4u && std::memcmp(chunk.type.data(), type, 4u) == 0) {
            return &chunk;
        }
    }
    return nullptr;
}

int parse_icns_chunks(const std::vector<std::uint8_t>& data, std::vector<IcnsChunk>* out_chunks)
{
    if (!out_chunks) {
        return PILLOW_C_NULL_POINTER;
    }
    out_chunks->clear();
    if (data.size() < 8u || std::memcmp(data.data(), "icns", 4u) != 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint32_t file_length = read_be32(data.data() + 4u);
    std::size_t index = 8u;
    while (index < file_length) {
        if (index + 8u > data.size()) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint32_t block_size = read_be32(data.data() + index + 4u);
        if (block_size <= 0u || block_size < 8u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        IcnsChunk chunk;
        chunk.type.assign(
            reinterpret_cast<const char*>(data.data() + index),
            reinterpret_cast<const char*>(data.data() + index + 4u));
        const std::size_t payload_size = static_cast<std::size_t>(block_size) - 8u;
        const std::size_t available = data.size() >= index + 8u ? data.size() - index - 8u : 0u;
        chunk.payload.assign(
            data.begin() + static_cast<std::ptrdiff_t>(index + 8u),
            data.begin() + static_cast<std::ptrdiff_t>(index + 8u + std::min(payload_size, available)));
        out_chunks->push_back(std::move(chunk));
        index += 8u + payload_size;
    }
    return PILLOW_C_OK;
}

bool icns_slot_present(const std::vector<IcnsChunk>& chunks, const IcnsSlot& slot)
{
    for (std::size_t reader_index = 0; reader_index < slot.reader_count; ++reader_index) {
        if (find_icns_chunk(chunks, slot.readers[reader_index].type)) {
            return true;
        }
    }
    return false;
}

int icns_sizes_for_chunks(
    const std::vector<IcnsChunk>& chunks,
    std::vector<int>* out_sizes,
    bool* out_found,
    const IcnsSlot** out_best_slot)
{
    if (!out_sizes || !out_found || !out_best_slot) {
        return PILLOW_C_NULL_POINTER;
    }
    out_sizes->clear();
    *out_found = false;
    *out_best_slot = nullptr;
    int best_width = -1;
    int best_height = -1;
    int best_scale = -1;
    for (const IcnsSlot& slot : ICNS_SLOTS) {
        if (!icns_slot_present(chunks, slot)) {
            continue;
        }
        out_sizes->push_back(slot.width);
        out_sizes->push_back(slot.height);
        out_sizes->push_back(slot.scale);
        *out_found = true;
        if (slot.width > best_width ||
            (slot.width == best_width && slot.height > best_height) ||
            (slot.width == best_width && slot.height == best_height && slot.scale > best_scale)) {
            best_width = slot.width;
            best_height = slot.height;
            best_scale = slot.scale;
            *out_best_slot = &slot;
        }
    }
    return PILLOW_C_OK;
}

bool icns_payload_is_jpeg2000(const std::vector<std::uint8_t>& payload)
{
    static constexpr std::uint8_t jp2_a[4] = {0xff, 0x4f, 0xff, 0x51};
    static constexpr std::uint8_t jp2_b[4] = {0x0d, 0x0a, 0x87, 0x0a};
    static constexpr std::uint8_t jp2_c[12] = {0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a};
    if (payload.size() >= 4u &&
        (std::memcmp(payload.data(), jp2_a, 4u) == 0 || std::memcmp(payload.data(), jp2_b, 4u) == 0)) {
        return true;
    }
    return payload.size() >= 12u && std::memcmp(payload.data(), jp2_c, 12u) == 0;
}

bool icns_payload_is_png(const std::vector<std::uint8_t>& payload)
{
    static constexpr std::uint8_t png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    return payload.size() >= 8u && std::memcmp(payload.data(), png_signature, 8u) == 0;
}

bool icns_png_payload_mode_supported(const std::vector<std::uint8_t>& payload)
{
    if (payload.size() < 33u) {
        return false;
    }
    const int bit_depth = payload[24];
    const int color_type = payload[25];
    if ((color_type == 0 || color_type == 2 || color_type == 4 || color_type == 6) && bit_depth == 8) {
        return true;
    }
    if (color_type == 3 && bit_depth <= 8) {
        return true;
    }
    return false;
}

int decode_icns_rgb32_payload(
    const std::uint8_t* data,
    std::size_t size,
    int width,
    int height,
    bool expect_32t_header,
    PillowCImage** out_image,
    int* out_rle_leftover)
{
    if (!data || !out_image || !out_rle_leftover) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    *out_rle_leftover = -1;
    if (expect_32t_header) {
        if (size < 4u || data[0] != 0u || data[1] != 0u || data[2] != 0u || data[3] != 0u) {
            return PILLOW_C_ICNS_IT32_SIG;
        }
        data += 4;
        size -= 4u;
    }
    const std::int64_t sizesq = static_cast<std::int64_t>(width) * static_cast<std::int64_t>(height);
    if (sizesq <= 0 || sizesq > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t pixel_count = static_cast<std::size_t>(sizesq);

    std::size_t stride = 0;
    std::size_t storage_size = 0;
    if (!checked_image_size(width, height, 3, &stride, &storage_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_RGB,
            3,
            stride,
            std::vector<std::uint8_t>(storage_size)};
        if (size == pixel_count * 3u) {
            std::memcpy(image->pixels.data(), data, size);
            *out_image = image;
            return PILLOW_C_OK;
        }

        // RLE variant: per-band run/copy chunks with the second-byte flags.
        std::vector<std::uint8_t> bands[3];
        std::size_t pos = 0u;
        for (int band_index = 0; band_index < 3; ++band_index) {
            std::vector<std::uint8_t>& band = bands[band_index];
            std::int64_t bytes_left = sizesq;
            while (bytes_left > 0) {
                if (pos >= size) {
                    break;
                }
                const std::uint8_t byte_value = data[pos++];
                if (byte_value & 0x80u) {
                    const std::int64_t block_size = static_cast<std::int64_t>(byte_value) - 125;
                    const std::uint8_t fill = pos < size ? data[pos++] : 0;
                    for (std::int64_t repeat = 0; repeat < block_size; ++repeat) {
                        band.push_back(fill);
                    }
                    bytes_left -= block_size;
                } else {
                    const std::int64_t block_size = static_cast<std::int64_t>(byte_value) + 1;
                    const std::size_t copy_count = static_cast<std::size_t>(std::min<std::int64_t>(block_size, static_cast<std::int64_t>(size - pos)));
                    band.insert(band.end(), data + pos, data + pos + copy_count);
                    pos += copy_count;
                    bytes_left -= block_size;
                }
                if (bytes_left <= 0) {
                    break;
                }
            }
            if (bytes_left != 0) {
                *out_rle_leftover = static_cast<int>(bytes_left);
                delete image;
                return PILLOW_C_ICNS_RLE;
            }
        }

        std::size_t pixel_index = 0u;
        for (std::size_t index = 0; index < pixel_count && index < bands[0].size(); ++index) {
            image->pixels[index * 3u + 0u] = bands[0][index];
            image->pixels[index * 3u + 1u] = bands[1][index];
            image->pixels[index * 3u + 2u] = bands[2][index];
            ++pixel_index;
        }
        if (pixel_index != pixel_count) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_icns_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<IcnsChunk> chunks;
    int status = parse_icns_chunks(data, &chunks);
    if (status != PILLOW_C_OK) {
        return status;
    }

    std::vector<int> sizes;
    bool found = false;
    const IcnsSlot* best_slot = nullptr;
    status = icns_sizes_for_chunks(chunks, &sizes, &found, &best_slot);
    if (status != PILLOW_C_OK || !found || !best_slot) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    // Decode the best slot's channels: RGBA wins, else RGB plus optional A.
    PillowCImage* rgba_image = nullptr;
    PillowCImage* rgb_image = nullptr;
    PillowCImage* mask_image = nullptr;
    try {
        for (std::size_t reader_index = 0; reader_index < best_slot->reader_count; ++reader_index) {
            const IcnsSlotReader& reader = best_slot->readers[reader_index];
            const IcnsChunk* chunk = find_icns_chunk(chunks, reader.type);
            if (!chunk) {
                continue;
            }
            if (reader.kind == ICNS_READER_PNG_JP2) {
                if (icns_payload_is_png(chunk->payload)) {
                    if (!icns_png_payload_mode_supported(chunk->payload)) {
                        return PILLOW_C_ICNS_PNG_MODE;
                    }
                    status = pillow_c_png_decode_memory(
                        chunk->payload.data(),
                        chunk->payload.size(),
                        &rgba_image);
                    if (status != PILLOW_C_OK) {
                        return PILLOW_C_ICNS_PNG_BAD;
                    }
                } else if (icns_payload_is_jpeg2000(chunk->payload)) {
                    return PILLOW_C_ICNS_JP2K;
                } else {
                    return PILLOW_C_ICNS_SUBIMAGE;
                }
            } else if (reader.kind == ICNS_READER_RGB32 || reader.kind == ICNS_READER_RGB32T) {
                int rle_leftover = -1;
                status = decode_icns_rgb32_payload(
                    chunk->payload.data(),
                    chunk->payload.size(),
                    best_slot->width,
                    best_slot->height,
                    reader.kind == ICNS_READER_RGB32T,
                    &rgb_image,
                    &rle_leftover);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            } else if (reader.kind == ICNS_READER_MASK) {
                const std::int64_t sizesq = static_cast<std::int64_t>(best_slot->width) * static_cast<std::int64_t>(best_slot->height);
                if (sizesq <= 0 || chunk->payload.size() != static_cast<std::size_t>(sizesq)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                std::size_t stride = 0;
                std::size_t storage_size = 0;
                if (!checked_image_size(best_slot->width, best_slot->height, 1, &stride, &storage_size)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                auto* mask = new PillowCImage{
                    best_slot->width,
                    best_slot->height,
                    PILLOW_C_MODE_L,
                    1,
                    stride,
                    std::vector<std::uint8_t>(chunk->payload.begin(), chunk->payload.end())};
                mask_image = mask;
            }
        }

        if (rgba_image) {
            delete rgb_image;
            delete mask_image;
            *out_image = rgba_image;
            return PILLOW_C_OK;
        }
        if (!rgb_image) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (mask_image) {
            // RGB + 1-bit/mask alpha -> RGBA in Pillow's getimage().
            std::size_t stride = 0;
            std::size_t storage_size = 0;
            if (!checked_image_size(best_slot->width, best_slot->height, 4, &stride, &storage_size)) {
                delete rgb_image;
                delete mask_image;
                return PILLOW_C_INVALID_ARGUMENT;
            }
            auto* composed = new PillowCImage{
                best_slot->width,
                best_slot->height,
                PILLOW_C_MODE_RGBA,
                4,
                stride,
                std::vector<std::uint8_t>(storage_size)};
            const std::size_t pixel_count = static_cast<std::size_t>(best_slot->width) * static_cast<std::size_t>(best_slot->height);
            for (std::size_t index = 0; index < pixel_count; ++index) {
                composed->pixels[index * 4u + 0u] = rgb_image->pixels[index * 3u + 0u];
                composed->pixels[index * 4u + 1u] = rgb_image->pixels[index * 3u + 1u];
                composed->pixels[index * 4u + 2u] = rgb_image->pixels[index * 3u + 2u];
                composed->pixels[index * 4u + 3u] = mask_image->pixels[index];
            }
            delete rgb_image;
            delete mask_image;
            *out_image = composed;
            return PILLOW_C_OK;
        }
        *out_image = rgb_image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        delete rgba_image;
        delete rgb_image;
        delete mask_image;
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int icns_sizes_for_path(const char* path, int* out_sizes, int capacity, int* out_count)
{
    if (!path || !out_sizes || !out_count) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_count = 0;
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<IcnsChunk> chunks;
    int status = parse_icns_chunks(data, &chunks);
    if (status != PILLOW_C_OK) {
        return status;
    }
    std::vector<int> sizes;
    bool found = false;
    const IcnsSlot* best_slot = nullptr;
    status = icns_sizes_for_chunks(chunks, &sizes, &found, &best_slot);
    if (status != PILLOW_C_OK || !found) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (capacity < 0 || sizes.size() > static_cast<std::size_t>(capacity)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        out_sizes[index] = sizes[index];
    }
    *out_count = static_cast<int>(sizes.size());
    return PILLOW_C_OK;
}

int save_icns_images(const PillowCImage* const* images, std::size_t image_count, const char* path)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count == 0 || !images[0]) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const PillowCImage* source = images[0];
    if (source->width <= 0 || source->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    for (std::size_t index = 1; index < image_count; ++index) {
        if (!images[index]) {
            return PILLOW_C_NULL_POINTER;
        }
    }
    int color_type = 0;
    int payload_channels = 0;
    int status = pillow_c_png_custom_mode_spec(source, &color_type, &payload_channels);
    if (status != PILLOW_C_OK) {
        return PILLOW_C_ICNS_SAVE_MODE;
    }
    for (std::size_t index = 0; index < image_count; ++index) {
        status = pillow_c_refresh_const_buffer_view_image(images[index]);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    static constexpr struct {
        const char* type;
        int size;
    } ICNS_SAVE_SIZES[] = {
        {"ic07", 128},
        {"ic08", 256},
        {"ic09", 512},
        {"ic10", 1024},
        {"ic11", 32},
        {"ic12", 64},
        {"ic13", 256},
        {"ic14", 512},
    };

    try {
        const int resample =
            source->mode == PILLOW_C_MODE_P || source->mode == PILLOW_C_MODE_1
                ? PILLOW_C_LEGACY_RESAMPLE_NEAREST
                : PILLOW_C_LEGACY_RESAMPLE_BICUBIC;

        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> entries;
        entries.reserve(8);
        for (const auto& save_size : ICNS_SAVE_SIZES) {
            const PillowCImage* provided = nullptr;
            for (std::size_t index = 0; index < image_count; ++index) {
                if (images[index]->width == save_size.size) {
                    provided = images[index];
                    break;
                }
            }
            const PillowCImage* frame = provided ? provided : source;
            PillowCImage resized{};
            if (!provided && (frame->width != save_size.size || frame->height != save_size.size)) {
                std::size_t stride = 0;
                std::size_t storage_size = 0;
                if (!checked_image_size(save_size.size, save_size.size, frame->channels, &stride, &storage_size)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                resized = PillowCImage{
                    save_size.size,
                    save_size.size,
                    frame->mode,
                    frame->channels,
                    stride,
                    std::vector<std::uint8_t>(storage_size)};
                status = pillow_c_resize_image_into(frame, save_size.size, save_size.size, resample, &resized);
                if (status != PILLOW_C_OK) {
                    return status;
                }
                pillow_c_copy_palette_if_same_mode(frame, &resized);
                frame = &resized;
            }
            std::vector<std::uint8_t> payload;
            status = pillow_c_png_encode_custom_image(frame, false, 0.0, 0.0, false, 0, false, 0, 0, 0, &payload);
            if (status != PILLOW_C_OK) {
                return PILLOW_C_ICNS_SAVE_MODE;
            }
            entries.emplace_back(save_size.type, std::move(payload));
        }

        std::uint64_t file_length = 8u + (8u + 8u * entries.size());
        for (const auto& entry : entries) {
            file_length += 8u + static_cast<std::uint64_t>(entry.second.size());
            if (file_length > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) ||
                entry.second.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 8u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }

        std::vector<std::uint8_t> icns;
        icns.reserve(static_cast<std::size_t>(file_length));
        icns.insert(icns.end(), {'i', 'c', 'n', 's'});
        append_be32(icns, static_cast<std::uint32_t>(file_length));
        const std::uint32_t toc_length = static_cast<std::uint32_t>(8u + entries.size() * 8u);
        icns.insert(icns.end(), {'T', 'O', 'C', ' '});
        append_be32(icns, toc_length);
        for (const auto& entry : entries) {
            icns.insert(icns.end(), entry.first.begin(), entry.first.end());
            append_be32(icns, static_cast<std::uint32_t>(8u + entry.second.size()));
        }
        for (const auto& entry : entries) {
            icns.insert(icns.end(), entry.first.begin(), entry.first.end());
            append_be32(icns, static_cast<std::uint32_t>(8u + entry.second.size()));
            icns.insert(icns.end(), entry.second.begin(), entry.second.end());
        }
        return write_binary_file(path, icns) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// ---------------------------------------------------------------------------
// BEHAV-EPS-001: Encapsulated PostScript save (Pillow's pure-Python writer).
//
// Pillow 11.3.0's EpsImagePlugin._save writes the DSC 3.0 header, an
// %ImageData descriptor, the PostScript preamble, and the pixel bytes hex
// encoded with 39 bytes (78 hex chars) per line; L/RGB/CMYK are supported
// and every other mode raises "image mode is not supported" (local status
// -27 for the facade). The RGB "skip junk bytes" hack in EpsEncode.c only
// applies to 4-byte RGBX-stored cores, which this runtime never produces
// (tight 3-byte RGB storage), so it never triggers - verified against the
// Pillow 11.3.0 oracle.
// ---------------------------------------------------------------------------

constexpr int PILLOW_C_EPS_MODE = -27;

int save_eps_image(const PillowCImage* image, const char* path)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    const char* operator_name = nullptr;
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
        operator_name = "image";
    } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
        operator_name = "false 3 colorimage";
    } else if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
        operator_name = "false 4 colorimage";
    } else {
        return PILLOW_C_EPS_MODE;
    }

    try {
        static constexpr char hex_digits[] = "0123456789abcdef";
        std::vector<std::uint8_t> eps;
        auto append_text = [&eps](const std::string& text) {
            eps.insert(eps.end(), text.begin(), text.end());
        };
        auto append_int = [&eps](std::int64_t value) {
            const std::string text = std::to_string(value);
            eps.insert(eps.end(), text.begin(), text.end());
        };

        append_text("%!PS-Adobe-3.0 EPSF-3.0\n");
        append_text("%%Creator: PIL 0.1 EpsEncode\n");
        append_text("%%BoundingBox: 0 0 ");
        append_int(image->width);
        eps.push_back(' ');
        append_int(image->height);
        eps.push_back('\n');
        append_text("%%Pages: 1\n");
        append_text("%%EndComments\n");
        append_text("%%Page: 1 1\n");
        append_text("%ImageData: ");
        append_int(image->width);
        eps.push_back(' ');
        append_int(image->height);
        append_text(" 8 ");
        append_int(image->channels);
        append_text(" 0 1 1 \"");
        append_text(operator_name);
        append_text("\"\n");
        append_text("gsave\n");
        append_text("10 dict begin\n");
        append_text("/buf ");
        append_int(static_cast<std::int64_t>(image->width) * image->channels);
        append_text(" string def\n");
        append_int(image->width);
        eps.push_back(' ');
        append_int(image->height);
        append_text(" scale\n");
        append_int(image->width);
        eps.push_back(' ');
        append_int(image->height);
        append_text(" 8\n[");
        append_int(image->width);
        append_text(" 0 0 -");
        append_int(image->height);
        append_text(" 0 ");
        append_int(image->height);
        append_text("]\n");
        append_text("{ currentfile buf readhexstring pop } bind\n");
        append_text(operator_name);
        eps.push_back('\n');

        const std::uint8_t* pixels = image->pixels.data();
        const std::size_t total_bytes = static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) * static_cast<std::size_t>(image->channels);
        std::size_t line_bytes = 0;
        for (std::size_t index = 0; index < total_bytes; ++index) {
            if (line_bytes == 39u) {
                eps.push_back('\n');
                line_bytes = 0;
            }
            const std::uint8_t value = pixels[index];
            eps.push_back(static_cast<std::uint8_t>(hex_digits[(value >> 4) & 15]));
            eps.push_back(static_cast<std::uint8_t>(hex_digits[value & 15]));
            ++line_bytes;
        }
        append_text("\n%%EndBinary\n");
        append_text("grestore end\n");
        return write_binary_file(path, eps) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}


} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_open_bmp(
    const char* path,
    PillowCImage** out_image)
{
    return open_bmp_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_bmp(
    const PillowCImage* image,
    const char* path)
{
    return save_bmp_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_msp(
    const char* path,
    PillowCImage** out_image)
{
    return open_msp_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_msp(
    const PillowCImage* image,
    const char* path)
{
    return save_msp_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_blp(
    const char* path,
    PillowCImage** out_image)
{
    return open_blp_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_blp(
    const PillowCImage* image,
    const char* path,
    int blp1)
{
    return save_blp_image(image, path, blp1 != 0);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_pcx(
    const char* path,
    PillowCImage** out_image)
{
    return open_pcx_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_pixar(
    const char* path,
    PillowCImage** out_image)
{
    return open_pixar_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_xvthumb(
    const char* path,
    PillowCImage** out_image)
{
    return open_xvthumb_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_dcx(
    const char* path,
    PillowCImage** out_image)
{
    return open_dcx_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_ftex(
    const char* path,
    PillowCImage** out_image)
{
    return open_ftex_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_sun(
    const char* path,
    PillowCImage** out_image)
{
    return open_sun_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_gbr(
    const char* path,
    PillowCImage** out_image)
{
    return open_gbr_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_fits(
    const char* path,
    PillowCImage** out_image)
{
    return open_fits_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_xpm(
    const char* path,
    PillowCImage** out_image)
{
    return open_xpm_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_psd(
    const char* path,
    PillowCImage** out_image)
{
    return open_psd_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_fli(
    const char* path,
    PillowCImage** out_image)
{
    return open_fli_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_fli_truncation_count(
    const char* path,
    std::int64_t* out_count)
{
    return fli_truncation_count(path, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_mic(
    const char* path,
    PillowCImage** out_image)
{
    return open_mic_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_pcx(
    const PillowCImage* image,
    const char* path)
{
    return save_pcx_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_sgi(
    const char* path,
    PillowCImage** out_image)
{
    return open_sgi_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_sgi(
    const PillowCImage* image,
    const char* path,
    int bpc)
{
    return save_sgi_image(image, path, bpc);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_dds(
    const char* path,
    PillowCImage** out_image)
{
    return open_dds_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_dds(
    const PillowCImage* image,
    const char* path,
    const char* pixel_format)
{
    return save_dds_image(image, path, pixel_format);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_icns(
    const char* path,
    PillowCImage** out_image)
{
    return open_icns_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_icns(
    const PillowCImage* image,
    const char* path)
{
    return save_icns_images(&image, 1u, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_icns_frames(
    const PillowCImage* const* images,
    int image_count,
    const char* path)
{
    if (image_count <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_icns_images(images, static_cast<std::size_t>(image_count), path);
}

extern "C" __declspec(dllexport) int pillow_c_image_icns_sizes(
    const char* path,
    int* out_sizes,
    int capacity,
    int* out_count)
{
    return icns_sizes_for_path(path, out_sizes, capacity, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_eps(
    const PillowCImage* image,
    const char* path)
{
    return save_eps_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_ppm(
    const char* path,
    PillowCImage** out_image)
{
    return open_ppm_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ppm(
    const PillowCImage* image,
    const char* path)
{
    return save_ppm_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_qoi(
    const char* path,
    PillowCImage** out_image)
{
    return open_qoi_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_qoi(
    const PillowCImage* image,
    const char* path)
{
    return save_qoi_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_qoi_options(
    const PillowCImage* image,
    const char* path,
    int colorspace)
{
    return save_qoi_image_with_colorspace(image, path, colorspace);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_tga(
    const char* path,
    PillowCImage** out_image)
{
    return open_tga_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_tga_open_info(
    const char* path,
    int* out_has_rle,
    int* out_orientation)
{
    if (!path || !out_has_rle || !out_orientation) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_rle = 0;
    *out_orientation = -1;
    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (data.size() < 18u) {
            return PILLOW_C_INVALID_LENGTH;
        }
        // API-OPENINFO-001: Pillow's TGA info -- compression is "tga_rle"
        // when the image type carries the RLE bit; orientation is 1 for the
        // 0x20/0x30 descriptor flags and -1 for 0/0x10 (anything else is
        // Pillow's SyntaxError).
        const std::uint8_t image_type = data[2];
        const std::uint8_t flags = data[17] & 0x30u;
        if (flags == 0x20u || flags == 0x30u) {
            *out_orientation = 1;
        } else if (flags == 0u || flags == 0x10u) {
            *out_orientation = -1;
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_has_rle = (image_type & 8u) != 0u ? 1 : 0;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tga(
    const PillowCImage* image,
    const char* path)
{
    return save_tga_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tga_options(
    const PillowCImage* image,
    const char* path,
    int rle)
{
    return save_tga_image_with_options(image, path, rle != 0);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_tga_full_options(
    const PillowCImage* image,
    const char* path,
    int rle,
    const std::uint8_t* id_section,
    std::size_t id_size,
    int orientation)
{
    return save_tga_image_with_full_options(
        image, path, rle != 0, id_section, id_size, orientation);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_xbm(
    const char* path,
    PillowCImage** out_image)
{
    return open_xbm_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_xbm(
    const PillowCImage* image,
    const char* path)
{
    return save_xbm_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_xbm_options(
    const PillowCImage* image,
    const char* path,
    int has_hotspot,
    int hotspot_x,
    int hotspot_y)
{
    return save_xbm_image_with_options(image, path, has_hotspot != 0, hotspot_x, hotspot_y);
}

extern "C" __declspec(dllexport) int pillow_c_image_tobitmap(
    const PillowCImage* image,
    const char* name,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    return tobitmap_image(image, name, out, out_size, out_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_ico(
    const char* path,
    PillowCImage** out_image)
{
    return open_ico_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_cur(
    const char* path,
    PillowCImage** out_image)
{
    return open_cur_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_ico_size(
    const char* path,
    int width,
    int height,
    PillowCImage** out_image)
{
    return open_ico_image_size(path, width, height, true, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_ico_sizes(
    const char* path,
    int* out_sizes,
    std::size_t out_pair_count,
    std::size_t* out_required)
{
    return ico_image_sizes(path, out_sizes, out_pair_count, out_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_ico_payload_format(
    const char* path,
    int width,
    int height,
    int require_requested_size,
    char* out_format,
    std::size_t out_size,
    std::size_t* out_required)
{
    return ico_payload_format(path, width, height, require_requested_size != 0, out_format, out_size, out_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_ico_payload_dib_metadata(
    const char* path,
    int width,
    int height,
    int require_requested_size,
    int* out_has_dib,
    int* out_has_dpi,
    double* out_dpi_x,
    double* out_dpi_y,
    int* out_compression)
{
    return ico_payload_dib_metadata(
        path,
        width,
        height,
        require_requested_size != 0,
        out_has_dib,
        out_has_dpi,
        out_dpi_x,
        out_dpi_y,
        out_compression);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico(
    const PillowCImage* image,
    const char* path)
{
    return save_ico_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count)
{
    return save_ico_image_options(image, path, sizes, size_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico_format_options(
    const PillowCImage* image,
    const char* path,
    const int* sizes,
    std::size_t size_count,
    int has_sizes,
    const char* bitmap_format)
{
    return save_ico_image_format_options(image, path, sizes, size_count, has_sizes, bitmap_format);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_ico_frames_format_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* sizes,
    std::size_t size_count,
    int has_sizes,
    const char* bitmap_format)
{
    return save_ico_images_format_options(images, image_count, path, sizes, size_count, has_sizes, bitmap_format);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_cur_options(
    const PillowCImage* image,
    const char* path,
    int has_hotspot,
    int hotspot_x,
    int hotspot_y)
{
    return save_cur_image_with_hotspot(image, path, has_hotspot != 0, hotspot_x, hotspot_y);
}


