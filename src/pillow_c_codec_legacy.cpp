#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <cstdio>
#include <cstdlib>
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
constexpr int PILLOW_C_LEGACY_RESAMPLE_LANCZOS = 1;

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

int save_qoi_image(const PillowCImage* image, const char* path)
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
        out.push_back(1u);

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

int save_tga_image_with_options(const PillowCImage* image, const char* path, bool rle)
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
        out.reserve(18u + palette_entries * 3u + pixel_count * static_cast<std::size_t>(file_pixel_bytes) + 26u);

        out.push_back(0);
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
        out.push_back(static_cast<std::uint8_t>(image->mode == PILLOW_C_MODE_RGBA ? 8 : 0));
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
        for (int y = image->height - 1; y >= 0; --y) {
            const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            for (int x = 0; x < image->width; ++x) {
                const std::uint8_t* src = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels);
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

int open_pcx_image(const char* path, PillowCImage** out_image)
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

extern "C" __declspec(dllexport) int pillow_c_image_open_tga(
    const char* path,
    PillowCImage** out_image)
{
    return open_tga_image(path, out_image);
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


