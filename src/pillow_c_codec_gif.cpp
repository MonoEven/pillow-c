#include "pillow_c_internal.h"
#include "pillow_c_wic_internal.h"

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

namespace {
struct GifMetadata {
    int duration_ms = -1;
    int loop = -1;
    int disposal = 0;
    int background = -1;
    int transparency = -1;
    std::vector<std::uint8_t> comment;
    bool is_87a = false;
    bool has_extension = false;
    std::vector<std::uint8_t> extension_label;
    std::size_t extension_offset = 0;
};

bool skip_gif_sub_blocks(const std::vector<std::uint8_t>& data, std::size_t* pos)
{
    if (!pos) {
        return false;
    }
    while (*pos < data.size()) {
        const std::size_t block_size = data[(*pos)++];
        if (block_size == 0u) {
            return true;
        }
        if (block_size > data.size() - *pos) {
            return false;
        }
        *pos += block_size;
    }
    return false;
}

bool read_gif_sub_blocks(
    const std::vector<std::uint8_t>& data,
    std::size_t* pos,
    std::vector<std::uint8_t>* out);

bool gif_app_extension_is_looping(const std::uint8_t* data, std::size_t size)
{
    return size == 11u &&
           (std::memcmp(data, "NETSCAPE2.0", 11u) == 0 ||
            std::memcmp(data, "ANIMEXTS1.0", 11u) == 0);
}

bool read_gif_metadata(const char* path, int frame_index, GifMetadata* out)
{
    if (!path || frame_index < 0 || !out) {
        return false;
    }
    *out = GifMetadata{};

    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data) || data.size() < 13u) {
        return false;
    }
    if (!(std::memcmp(data.data(), "GIF87a", 6u) == 0 ||
          std::memcmp(data.data(), "GIF89a", 6u) == 0)) {
        return false;
    }
    out->is_87a = std::memcmp(data.data(), "GIF87a", 6u) == 0;

    const std::uint8_t logical_packed = data[10];
    out->background = data[11];

    std::size_t pos = 13u;
    if ((logical_packed & 0x80u) != 0u) {
        const std::size_t global_color_count = std::size_t{1} << ((logical_packed & 0x07u) + 1u);
        const std::size_t global_color_table_size = global_color_count * 3u;
        if (global_color_table_size > data.size() - pos) {
            return false;
        }
        pos += global_color_table_size;
    }

    int pending_duration_cs = -1;
    int pending_disposal = 0;
    int pending_transparency = -1;
    int loop_count = -1;
    int current_frame = 0;
    std::vector<std::uint8_t> current_comment;

    while (pos < data.size()) {
        const std::uint8_t introducer = data[pos++];
        if (introducer == 0x3bu) {
            return false;
        }
        if (introducer == 0x21u) {
            if (pos >= data.size()) {
                return false;
            }
            const std::uint8_t label = data[pos++];
            if (label == 0xf9u) {
                if (pos >= data.size()) {
                    return false;
                }
                const std::size_t block_size = data[pos++];
                if (block_size < 4u || block_size > data.size() - pos) {
                    return false;
                }
                const std::uint8_t packed = data[pos];
                pending_disposal = (packed >> 2) & 0x07;
                pending_duration_cs = static_cast<int>(read_le16(data.data() + pos + 1u));
                pending_transparency = (packed & 0x01u) != 0u ? static_cast<int>(data[pos + 3u]) : -1;
                pos += block_size;
                if (pos >= data.size() || data[pos] != 0u) {
                    return false;
                }
                ++pos;
            } else if (label == 0xfeu) {
                std::vector<std::uint8_t> comment;
                if (!read_gif_sub_blocks(data, &pos, &comment)) {
                    return false;
                }
                current_comment = std::move(comment);
            } else if (label == 0xffu) {
                if (pos >= data.size()) {
                    return false;
                }
                const std::size_t app_size = data[pos++];
                if (app_size > data.size() - pos) {
                    return false;
                }
                const std::uint8_t* app = data.data() + pos;
                const bool is_looping_app = gif_app_extension_is_looping(app, app_size);
                pos += app_size;
                // Pillow's info["extension"] = (label_bytes, file_offset
                // right after the label) -- only for the first frame.
                if (current_frame == 0 && !out->has_extension) {
                    out->has_extension = true;
                    out->extension_label.assign(app, app + app_size);
                    out->extension_offset = pos;
                }
                bool terminated = false;
                while (pos < data.size()) {
                    const std::size_t block_size = data[pos++];
                    if (block_size == 0u) {
                        terminated = true;
                        break;
                    }
                    if (block_size > data.size() - pos) {
                        return false;
                    }
                    if (is_looping_app && block_size >= 3u && data[pos] == 1u) {
                        loop_count = static_cast<int>(read_le16(data.data() + pos + 1u));
                    }
                    pos += block_size;
                }
                if (!terminated) {
                    return false;
                }
            } else if (!skip_gif_sub_blocks(data, &pos)) {
                return false;
            }
            continue;
        }
        if (introducer != 0x2cu) {
            return false;
        }

        if (9u > data.size() - pos) {
            return false;
        }
        const std::uint8_t image_packed = data[pos + 8u];
        pos += 9u;
        if ((image_packed & 0x80u) != 0u) {
            const std::size_t local_color_count = std::size_t{1} << ((image_packed & 0x07u) + 1u);
            const std::size_t local_color_table_size = local_color_count * 3u;
            if (local_color_table_size > data.size() - pos) {
                return false;
            }
            pos += local_color_table_size;
        }
        if (pos >= data.size()) {
            return false;
        }
        ++pos; // LZW minimum code size.
        if (!skip_gif_sub_blocks(data, &pos)) {
            return false;
        }

        if (current_frame == frame_index) {
            out->duration_ms = pending_duration_cs >= 0 ? pending_duration_cs * 10 : -1;
            out->loop = loop_count;
            out->disposal = pending_disposal;
            out->transparency = pending_transparency;
            out->comment = current_comment;
            return true;
        }
        ++current_frame;
        pending_duration_cs = -1;
        pending_disposal = 0;
        pending_transparency = -1;
    }

    return false;
}

bool read_gif_color_table(
    const std::vector<std::uint8_t>& data,
    std::size_t* pos,
    std::size_t entry_count,
    std::vector<std::uint8_t>* out)
{
    if (!pos || !out || entry_count == 0u || entry_count > 256u) {
        return false;
    }
    const std::size_t byte_count = entry_count * 3u;
    if (byte_count > data.size() - *pos) {
        return false;
    }
    out->assign(data.begin() + static_cast<std::ptrdiff_t>(*pos),
                data.begin() + static_cast<std::ptrdiff_t>(*pos + byte_count));
    *pos += byte_count;
    return true;
}

bool read_gif_sub_blocks(
    const std::vector<std::uint8_t>& data,
    std::size_t* pos,
    std::vector<std::uint8_t>* out)
{
    if (!pos || !out) {
        return false;
    }
    out->clear();
    while (*pos < data.size()) {
        const std::size_t block_size = data[(*pos)++];
        if (block_size == 0u) {
            return true;
        }
        if (block_size > data.size() - *pos) {
            return false;
        }
        out->insert(out->end(),
                    data.begin() + static_cast<std::ptrdiff_t>(*pos),
                    data.begin() + static_cast<std::ptrdiff_t>(*pos + block_size));
        *pos += block_size;
    }
    return false;
}

struct GifBitReader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t bit_pos = 0;

    bool read(int bit_count, int* out_code)
    {
        if (!out_code || bit_count <= 0 || bit_count > 12) {
            return false;
        }
        const std::size_t total_bits = bytes.size() * 8u;
        if (static_cast<std::size_t>(bit_count) > total_bits - bit_pos) {
            return false;
        }
        std::uint32_t code = 0;
        for (int bit = 0; bit < bit_count; ++bit) {
            const std::uint8_t value = bytes[bit_pos / 8u];
            if ((value & (std::uint8_t{1} << (bit_pos % 8u))) != 0u) {
                code |= std::uint32_t{1} << bit;
            }
            ++bit_pos;
        }
        *out_code = static_cast<int>(code);
        return true;
    }
};

bool gif_lzw_decode_indices(
    const std::vector<std::uint8_t>& compressed,
    int min_code_size,
    std::size_t expected_pixels,
    std::vector<std::uint8_t>* out)
{
    if (!out || min_code_size < 2 || min_code_size > 8 || expected_pixels == 0u) {
        return false;
    }
    out->clear();
    out->reserve(expected_pixels);

    const int clear_code = 1 << min_code_size;
    const int end_code = clear_code + 1;
    int next_code = end_code + 1;
    int code_size = min_code_size + 1;
    std::vector<std::vector<std::uint8_t>> dictionary(4096);

    auto reset_dictionary = [&]() {
        for (auto& entry : dictionary) {
            entry.clear();
        }
        for (int code = 0; code < clear_code; ++code) {
            dictionary[code] = {static_cast<std::uint8_t>(code)};
        }
        next_code = end_code + 1;
        code_size = min_code_size + 1;
    };

    reset_dictionary();
    GifBitReader reader{compressed};
    std::vector<std::uint8_t> previous;
    bool have_previous = false;

    while (out->size() < expected_pixels) {
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
        if (code >= 0 && code < next_code && !dictionary[code].empty()) {
            entry = dictionary[code];
        } else if (code == next_code && have_previous && !previous.empty()) {
            entry = previous;
            entry.push_back(previous.front());
        } else {
            return false;
        }

        const std::size_t remaining = expected_pixels - out->size();
        if (entry.size() > remaining) {
            out->insert(out->end(), entry.begin(), entry.begin() + static_cast<std::ptrdiff_t>(remaining));
            break;
        }
        out->insert(out->end(), entry.begin(), entry.end());
        if (have_previous && next_code < 4096 && !previous.empty() && !entry.empty()) {
            std::vector<std::uint8_t> next_entry = previous;
            next_entry.push_back(entry.front());
            dictionary[next_code++] = std::move(next_entry);
            if (next_code == (1 << code_size) && code_size < 12) {
                ++code_size;
            }
        }
        previous = std::move(entry);
        have_previous = true;
    }

    if (out->size() < expected_pixels) {
        return false;
    }
    if (out->size() > expected_pixels) {
        out->resize(expected_pixels);
    }
    return true;
}

bool gif_deinterlace_indices(
    const std::vector<std::uint8_t>& decoded,
    int width,
    int height,
    std::vector<std::uint8_t>* out)
{
    if (!out || width <= 0 || height <= 0 ||
        decoded.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return false;
    }
    out->assign(decoded.size(), 0);
    static constexpr int pass_starts[4] = {0, 4, 2, 1};
    static constexpr int pass_steps[4] = {8, 8, 4, 2};
    std::size_t src_row = 0;
    for (int pass = 0; pass < 4; ++pass) {
        for (int y = pass_starts[pass]; y < height; y += pass_steps[pass]) {
            if (src_row >= static_cast<std::size_t>(height)) {
                return false;
            }
            std::copy_n(decoded.data() + src_row * static_cast<std::size_t>(width),
                        width,
                        out->data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width));
            ++src_row;
        }
    }
    return src_row == static_cast<std::size_t>(height);
}

void gif_palette_color(
    const std::vector<std::uint8_t>& palette,
    int index,
    std::uint8_t* r,
    std::uint8_t* g,
    std::uint8_t* b)
{
    const std::size_t offset = static_cast<std::size_t>(index) * 3u;
    if (index < 0 || offset + 2u >= palette.size()) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }
    *r = palette[offset];
    *g = palette[offset + 1u];
    *b = palette[offset + 2u];
}

void gif_fill_rgb_rect(
    std::vector<std::uint8_t>* canvas,
    int logical_width,
    int logical_height,
    int left,
    int top,
    int width,
    int height,
    const std::uint8_t* color)
{
    if (!canvas || !color || logical_width <= 0 || logical_height <= 0 || width <= 0 || height <= 0) {
        return;
    }
    const int x0 = std::max(0, left);
    const int y0 = std::max(0, top);
    const int x1 = std::min(logical_width, left + width);
    const int y1 = std::min(logical_height, top + height);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(logical_width) +
                                        static_cast<std::size_t>(x)) * 3u;
            (*canvas)[offset] = color[0];
            (*canvas)[offset + 1u] = color[1];
            (*canvas)[offset + 2u] = color[2];
        }
    }
}

void gif_draw_indexed_frame_rgb(
    std::vector<std::uint8_t>* canvas,
    int logical_width,
    int logical_height,
    int left,
    int top,
    int width,
    int height,
    const std::vector<std::uint8_t>& indices,
    const std::vector<std::uint8_t>& palette,
    int transparency)
{
    if (!canvas || logical_width <= 0 || logical_height <= 0 || width <= 0 || height <= 0) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        const int dst_y = top + y;
        if (dst_y < 0 || dst_y >= logical_height) {
            continue;
        }
        for (int x = 0; x < width; ++x) {
            const int dst_x = left + x;
            if (dst_x < 0 || dst_x >= logical_width) {
                continue;
            }
            const std::size_t src_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                           static_cast<std::size_t>(x);
            if (src_offset >= indices.size()) {
                return;
            }
            const int index = indices[src_offset];
            if (index == transparency) {
                continue;
            }
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            gif_palette_color(palette, index, &r, &g, &b);
            const std::size_t dst_offset = (static_cast<std::size_t>(dst_y) * static_cast<std::size_t>(logical_width) +
                                            static_cast<std::size_t>(dst_x)) * 3u;
            (*canvas)[dst_offset] = r;
            (*canvas)[dst_offset + 1u] = g;
            (*canvas)[dst_offset + 2u] = b;
        }
    }
}

void gif_fill_rgba_rect(
    std::vector<std::uint8_t>* canvas,
    int logical_width,
    int logical_height,
    int left,
    int top,
    int width,
    int height,
    const std::uint8_t* color,
    std::uint8_t alpha)
{
    if (!canvas || !color || logical_width <= 0 || logical_height <= 0 || width <= 0 || height <= 0) {
        return;
    }
    const int x0 = std::max(0, left);
    const int y0 = std::max(0, top);
    const int x1 = std::min(logical_width, left + width);
    const int y1 = std::min(logical_height, top + height);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(logical_width) +
                                        static_cast<std::size_t>(x)) * 4u;
            (*canvas)[offset] = color[0];
            (*canvas)[offset + 1u] = color[1];
            (*canvas)[offset + 2u] = color[2];
            (*canvas)[offset + 3u] = alpha;
        }
    }
}

void gif_draw_indexed_frame_rgba(
    std::vector<std::uint8_t>* canvas,
    int logical_width,
    int logical_height,
    int left,
    int top,
    int width,
    int height,
    const std::vector<std::uint8_t>& indices,
    const std::vector<std::uint8_t>& palette,
    int transparency)
{
    if (!canvas || logical_width <= 0 || logical_height <= 0 || width <= 0 || height <= 0) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        const int dst_y = top + y;
        if (dst_y < 0 || dst_y >= logical_height) {
            continue;
        }
        for (int x = 0; x < width; ++x) {
            const int dst_x = left + x;
            if (dst_x < 0 || dst_x >= logical_width) {
                continue;
            }
            const std::size_t src_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                           static_cast<std::size_t>(x);
            if (src_offset >= indices.size()) {
                return;
            }
            const int index = indices[src_offset];
            if (index == transparency) {
                continue;
            }
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            gif_palette_color(palette, index, &r, &g, &b);
            const std::size_t dst_offset = (static_cast<std::size_t>(dst_y) * static_cast<std::size_t>(logical_width) +
                                            static_cast<std::size_t>(dst_x)) * 4u;
            (*canvas)[dst_offset] = r;
            (*canvas)[dst_offset + 1u] = g;
            (*canvas)[dst_offset + 2u] = b;
            (*canvas)[dst_offset + 3u] = 255u;
        }
    }
}

int open_gif_composited_frame_image(const char* path, int frame_index, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (frame_index <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data) || data.size() < 13u ||
            !(std::memcmp(data.data(), "GIF87a", 6u) == 0 ||
              std::memcmp(data.data(), "GIF89a", 6u) == 0)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const int logical_width = static_cast<int>(read_le16(data.data() + 6u));
        const int logical_height = static_cast<int>(read_le16(data.data() + 8u));
        if (logical_width <= 0 || logical_height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        std::size_t rgb_stride = 0;
        std::size_t rgb_size = 0;
        std::size_t rgba_stride = 0;
        std::size_t rgba_size = 0;
        if (!checked_image_size(logical_width, logical_height, 3, &rgb_stride, &rgb_size) ||
            !checked_image_size(logical_width, logical_height, 4, &rgba_stride, &rgba_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        const std::uint8_t logical_packed = data[10];
        const int background_index = data[11];
        std::size_t pos = 13u;
        std::vector<std::uint8_t> global_palette;
        if ((logical_packed & 0x80u) != 0u) {
            const std::size_t entry_count = std::size_t{1} << ((logical_packed & 0x07u) + 1u);
            if (!read_gif_color_table(data, &pos, entry_count, &global_palette)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        std::uint8_t background[3] = {0, 0, 0};
        gif_palette_color(global_palette, background_index, &background[0], &background[1], &background[2]);

        std::vector<std::uint8_t> canvas(rgb_size, 0);
        bool canvas_is_rgba = false;
        for (std::size_t pixel = 0; pixel < rgb_size; pixel += 3u) {
            canvas[pixel] = background[0];
            canvas[pixel + 1u] = background[1];
            canvas[pixel + 2u] = background[2];
        }

        int pending_disposal = 0;
        int pending_transparency = -1;
        int current_frame = 0;

        while (pos < data.size()) {
            const std::uint8_t introducer = data[pos++];
            if (introducer == 0x3bu) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (introducer == 0x21u) {
                if (pos >= data.size()) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const std::uint8_t label = data[pos++];
                if (label == 0xf9u) {
                    if (pos >= data.size()) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    const std::size_t block_size = data[pos++];
                    if (block_size < 4u || block_size > data.size() - pos) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    const std::uint8_t packed = data[pos];
                    pending_disposal = (packed >> 2) & 0x07;
                    pending_transparency = (packed & 0x01u) != 0u ? static_cast<int>(data[pos + 3u]) : -1;
                    pos += block_size;
                    if (pos >= data.size() || data[pos] != 0u) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    ++pos;
                } else if (!skip_gif_sub_blocks(data, &pos)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                continue;
            }
            if (introducer != 0x2cu || 9u > data.size() - pos) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const int left = static_cast<int>(read_le16(data.data() + pos));
            const int top = static_cast<int>(read_le16(data.data() + pos + 2u));
            const int frame_width = static_cast<int>(read_le16(data.data() + pos + 4u));
            const int frame_height = static_cast<int>(read_le16(data.data() + pos + 6u));
            const std::uint8_t image_packed = data[pos + 8u];
            pos += 9u;
            if (frame_width <= 0 || frame_height <= 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            std::vector<std::uint8_t> local_palette;
            const std::vector<std::uint8_t>* palette = &global_palette;
            if ((image_packed & 0x80u) != 0u) {
                const std::size_t entry_count = std::size_t{1} << ((image_packed & 0x07u) + 1u);
                if (!read_gif_color_table(data, &pos, entry_count, &local_palette)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                palette = &local_palette;
            }
            if (palette->empty() || pos >= data.size()) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const int min_code_size = data[pos++];
            std::vector<std::uint8_t> compressed;
            if (!read_gif_sub_blocks(data, &pos, &compressed)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            const std::size_t expected_pixels = static_cast<std::size_t>(frame_width) *
                                                static_cast<std::size_t>(frame_height);
            std::vector<std::uint8_t> decoded;
            if (!gif_lzw_decode_indices(compressed, min_code_size, expected_pixels, &decoded)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if ((image_packed & 0x40u) != 0u) {
                std::vector<std::uint8_t> deinterlaced;
                if (!gif_deinterlace_indices(decoded, frame_width, frame_height, &deinterlaced)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                decoded = std::move(deinterlaced);
            }

            if (current_frame == 0 && pending_transparency >= 0 && !canvas_is_rgba) {
                canvas.assign(rgba_size, std::uint8_t{0});
                canvas_is_rgba = true;
            }

            std::vector<std::uint8_t> restore_canvas;
            bool restore_canvas_is_rgba = canvas_is_rgba;
            if (pending_disposal == 3) {
                restore_canvas = canvas;
            }
            if (canvas_is_rgba) {
                gif_draw_indexed_frame_rgba(
                    &canvas,
                    logical_width,
                    logical_height,
                    left,
                    top,
                    frame_width,
                    frame_height,
                    decoded,
                    *palette,
                    pending_transparency);
            } else {
                gif_draw_indexed_frame_rgb(
                    &canvas,
                    logical_width,
                    logical_height,
                    left,
                    top,
                    frame_width,
                    frame_height,
                    decoded,
                    *palette,
                    pending_transparency);
            }

            if (current_frame == frame_index) {
                auto* image = new PillowCImage{
                    logical_width,
                    logical_height,
                    canvas_is_rgba ? PILLOW_C_MODE_RGBA : PILLOW_C_MODE_RGB,
                    canvas_is_rgba ? 4 : 3,
                    canvas_is_rgba ? rgba_stride : rgb_stride,
                    std::move(canvas)};
                *out_image = image;
                return PILLOW_C_OK;
            }

            if (pending_disposal == 2) {
                if (canvas_is_rgba) {
                    const std::uint8_t transparent_black[3] = {0, 0, 0};
                    gif_fill_rgba_rect(
                        &canvas,
                        logical_width,
                        logical_height,
                        left,
                        top,
                        frame_width,
                        frame_height,
                        transparent_black,
                        0u);
                } else {
                    gif_fill_rgb_rect(&canvas, logical_width, logical_height, left, top, frame_width, frame_height, background);
                }
            } else if (pending_disposal == 3 && !restore_canvas.empty()) {
                canvas = std::move(restore_canvas);
                canvas_is_rgba = restore_canvas_is_rgba;
            }

            ++current_frame;
            pending_disposal = 0;
            pending_transparency = -1;
        }
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    return PILLOW_C_INVALID_ARGUMENT;
}

int open_gif_frame_image(const char* path, int frame_index, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (frame_index > 0) {
        const int composited_status = open_gif_composited_frame_image(path, frame_index, out_image);
        if (composited_status == PILLOW_C_OK) {
            return PILLOW_C_OK;
        }
        if (composited_status == PILLOW_C_ALLOCATION_FAILED) {
            return composited_status;
        }
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
        if (FAILED(decoder->GetContainerFormat(&container)) || !IsEqualGUID(container, GUID_ContainerFormatGif)) {
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

        if (frame_index > 0) {
            std::size_t stride = 0;
            std::size_t size = 0;
            if (!checked_image_size(width, height, 3, &stride, &size) ||
                stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
                size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            ComPtr<IWICFormatConverter> converter;
            hr = factory->CreateFormatConverter(converter.put());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            hr = converter->Initialize(
                frame.get(),
                GUID_WICPixelFormat24bppBGR,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            std::vector<std::uint8_t> bgr(size, std::uint8_t{0});
            hr = converter->CopyPixels(nullptr, static_cast<UINT>(stride), static_cast<UINT>(bgr.size()), bgr.data());
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            auto* image = new PillowCImage{
                width,
                height,
                PILLOW_C_MODE_RGB,
                3,
                stride,
                std::vector<std::uint8_t>(size)};
            for (int y = 0; y < height; ++y) {
                const std::uint8_t* src_row = bgr.data() + static_cast<std::size_t>(y) * stride;
                std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                for (int x = 0; x < width; ++x) {
                    const std::size_t offset = static_cast<std::size_t>(x) * 3u;
                    dst_row[offset + 0u] = src_row[offset + 2u];
                    dst_row[offset + 1u] = src_row[offset + 1u];
                    dst_row[offset + 2u] = src_row[offset + 0u];
                }
            }
            *out_image = image;
            return PILLOW_C_OK;
        }

        std::vector<std::uint8_t> palette_rgb;
        status = copy_wic_palette_rgb(frame.get(), factory.get(), &palette_rgb);
        if (status != PILLOW_C_OK) {
            return status;
        }
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size(width, height, 1, &stride, &size) ||
            stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID source_format = {};
        hr = frame->GetPixelFormat(&source_format);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICPalette> source_palette;
        hr = factory->CreatePalette(source_palette.put());
        if (FAILED(hr) || FAILED(frame->CopyPalette(source_palette.get()))) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        ComPtr<IWICBitmapSource> source;
        if (IsEqualGUID(source_format, GUID_WICPixelFormat8bppIndexed)) {
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
                GUID_WICPixelFormat8bppIndexed,
                WICBitmapDitherTypeNone,
                source_palette.get(),
                0.0,
                WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            source.reset(converter.get());
            source.get()->AddRef();
        }

        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_P,
            1,
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
        image->palette_rgb = std::move(palette_rgb);
        image->palette_alpha.clear();
        image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_NONE;
        GifMetadata frame_metadata;
        if (!read_gif_metadata(path, 0, &frame_metadata)) {
            delete image;
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (frame_metadata.transparency >= 0) {
            const std::size_t palette_entries = image->palette_rgb.size() / 3u;
            const std::size_t alpha_entries = std::max(
                palette_entries,
                static_cast<std::size_t>(frame_metadata.transparency) + 1u);
            image->palette_alpha.assign(alpha_entries, std::uint8_t{255});
            image->palette_alpha[static_cast<std::size_t>(frame_metadata.transparency)] = 0;
            image->palette_alpha_mode = PILLOW_C_PALETTE_ALPHA_RGBA;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int open_gif_image(const char* path, PillowCImage** out_image)
{
    return open_gif_frame_image(path, 0, out_image);
}


int save_gif_indexed_native(
    const PillowCImage* image,
    const char* path,
    bool has_transparency,
    int transparency,
    const std::uint8_t* comment,
    std::size_t comment_size);

int save_gif_image_with_comment(
    const PillowCImage* image,
    const char* path,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (comment_size > 0u && !comment) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if ((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
        (image->mode == PILLOW_C_MODE_RGB && image->channels == 3)) {
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size_allow_empty(image->width, image->height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        try {
            PillowCImage quantized{
                image->width,
                image->height,
                PILLOW_C_MODE_P,
                1,
                stride,
                std::vector<std::uint8_t>(size)};
            const int status = pillow_c_quantize_exact_image_into(image, 256, &quantized);
            if (status != PILLOW_C_OK) {
                return status;
            }
            return save_gif_image_with_comment(&quantized, path, comment, comment_size);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }
    if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
        std::size_t stride = 0;
        std::size_t size = 0;
        if (!checked_image_size_allow_empty(image->width, image->height, 1, &stride, &size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        try {
            PillowCImage quantized{
                image->width,
                image->height,
                PILLOW_C_MODE_P,
                1,
                stride,
                std::vector<std::uint8_t>(size)};
            bool has_transparency = false;
            int transparency = 0;
            const int status = pillow_c_quantize_exact_rgba_gif_into(image, &quantized, &has_transparency, &transparency);
            if (status != PILLOW_C_OK) {
                return status;
            }
            return save_gif_indexed_native(&quantized, path, has_transparency, transparency, comment, comment_size);
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->palette_rgb.empty() || image->palette_rgb.size() % 3u != 0u || image->palette_rgb.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (comment_size > 0u) {
        return save_gif_indexed_native(image, path, false, 0, comment, comment_size);
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
        int status = create_wic_factory(&factory);
        if (status != PILLOW_C_OK) {
            return status;
        }
        ComPtr<IWICPalette> palette;
        status = create_wic_palette_from_rgb(factory.get(), image->palette_rgb, &palette);
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
        hr = factory->CreateEncoder(GUID_ContainerFormatGif, nullptr, encoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = encoder->SetPalette(palette.get());
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
        hr = frame->SetPalette(palette.get());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        WICPixelFormatGUID format = GUID_WICPixelFormat8bppIndexed;
        WICPixelFormatGUID encoder_format = format;
        hr = frame->SetPixelFormat(&encoder_format);
        if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = frame->WritePixels(
            static_cast<UINT>(image->height),
            static_cast<UINT>(image->stride),
            static_cast<UINT>(image->pixels.size()),
            const_cast<BYTE*>(image->pixels.data()));
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

int save_gif_image_with_comment_options(
    const PillowCImage* image,
    const char* path,
    bool has_transparency,
    int transparency,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (!has_transparency) {
        return save_gif_image_with_comment(image, path, comment, comment_size);
    }
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (comment_size > 0u && !comment) {
        return PILLOW_C_NULL_POINTER;
    }
    if (transparency < 0 || transparency > 255 || image->width <= 0 || image->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (image->mode != PILLOW_C_MODE_P || image->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->palette_rgb.empty() || image->palette_rgb.size() % 3u != 0u || image->palette_rgb.size() > 256u * 3u ||
        image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        image->pixels.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_gif_indexed_native(image, path, true, transparency, comment, comment_size);
}

// BEHAV-SAVEOPTS-001: Pillow 11.3.0's GIF interlace/palette options. The
// palette replaces the global color table (the pixel indices keep their
// meaning, padded to the power-of-two table); interlace sets the 0x40 image
// descriptor flag and emits the rows in the classic four-pass order.
void append_gif_comment_extension(
    std::vector<std::uint8_t>& out,
    const std::uint8_t* comment,
    std::size_t comment_size);
void append_gif_sub_blocks(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& data);
bool gif_lzw_encode_indices_interlaced(
    const std::uint8_t* pixels,
    int width,
    int height,
    std::size_t stride,
    int color_table_entries,
    int min_code_size,
    bool interlace,
    std::vector<std::uint8_t>* out);

int save_gif_indexed_native_with_interlace_palette(
    const PillowCImage* image,
    const char* path,
    bool has_transparency,
    int transparency,
    bool interlace,
    const std::uint8_t* custom_palette,
    std::size_t custom_palette_size,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (comment_size > 0u && !comment) {
        return PILLOW_C_NULL_POINTER;
    }
    if (custom_palette_size > 0u && !custom_palette) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 ||
        image->mode != PILLOW_C_MODE_P || image->channels != 1 ||
        image->width > std::numeric_limits<std::uint16_t>::max() ||
        image->height > std::numeric_limits<std::uint16_t>::max() ||
        (has_transparency && (transparency < 0 || transparency > 255))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    std::vector<std::uint8_t> palette_source_storage;
    if (custom_palette_size > 0u) {
        palette_source_storage.assign(custom_palette, custom_palette + custom_palette_size);
    } else {
        palette_source_storage = image->palette_rgb;
    }
    const std::vector<std::uint8_t>& palette_source = palette_source_storage;
    if (palette_source.empty() || palette_source.size() % 3u != 0u || palette_source.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    // Pillow 11.3.0's palette option uses _normalize_palette + remap_palette:
    // each given palette color matches the image palette by EXACT RGB value
    // (first occurrence); colors absent from the image palette take the
    // first unused index in the given palette's order, and image palette
    // entries beyond the given palette map to index 0.
    std::vector<std::uint8_t> remap;
    const std::uint8_t* remap_source = image->pixels.data();
    std::vector<std::uint8_t> remapped_pixels;
    if (custom_palette_size > 0u) {
        const std::size_t custom_entries = palette_source.size() / 3u;
        const std::size_t image_entries = image->palette_rgb.size() / 3u;
        std::unordered_map<std::uint32_t, int> image_color_index;
        image_color_index.reserve(image_entries * 2u);
        for (std::size_t k = 0; k < image_entries; ++k) {
            const std::uint32_t key = (static_cast<std::uint32_t>(image->palette_rgb[k * 3u]) << 16) |
                (static_cast<std::uint32_t>(image->palette_rgb[k * 3u + 1u]) << 8) |
                static_cast<std::uint32_t>(image->palette_rgb[k * 3u + 2u]);
            if (image_color_index.find(key) == image_color_index.end()) {
                image_color_index.emplace(key, static_cast<int>(k));
            }
        }
        std::vector<int> used;
        used.reserve(custom_entries);
        for (std::size_t c = 0; c < custom_entries; ++c) {
            const std::uint32_t key = (static_cast<std::uint32_t>(palette_source[c * 3u]) << 16) |
                (static_cast<std::uint32_t>(palette_source[c * 3u + 1u]) << 8) |
                static_cast<std::uint32_t>(palette_source[c * 3u + 2u]);
            const auto found = image_color_index.find(key);
            int index = found != image_color_index.end() ? found->second : -1;
            if (index >= 0 &&
                std::find(used.begin(), used.end(), index) != used.end()) {
                index = -1;
            }
            used.push_back(index);
        }
        for (std::size_t i = 0; i < used.size(); ++i) {
            if (used[i] < 0) {
                for (std::size_t j = 0; j < used.size(); ++j) {
                    if (std::find(used.begin(), used.end(), static_cast<int>(j)) == used.end()) {
                        used[i] = static_cast<int>(j);
                        break;
                    }
                }
            }
        }
        remap.reserve(std::min<std::size_t>(image_entries, 256u));
        for (std::size_t k = 0; k < image_entries; ++k) {
            remap.push_back(static_cast<std::uint8_t>(k < used.size() ? used[k] : 0));
        }
        remapped_pixels.assign(image->pixels.size(), 0);
        for (std::size_t i = 0; i < image->pixels.size(); ++i) {
            const std::uint8_t index = image->pixels[i];
            remapped_pixels[i] = index < remap.size() ? remap[index] : 0;
        }
        remap_source = remapped_pixels.data();
    }
    int color_table_entries = 2;
    while (color_table_entries * 3 < static_cast<int>(palette_source.size())) {
        color_table_entries <<= 1;
    }
    if (color_table_entries > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int min_code_size = 0;
    while ((1 << min_code_size) < color_table_entries) {
        ++min_code_size;
    }
    min_code_size = std::max(2, min_code_size);

    try {
        std::vector<std::uint8_t> lzw;
        if (!gif_lzw_encode_indices_interlaced(
                remap_source,
                image->width,
                image->height,
                image->stride,
                color_table_entries,
                min_code_size,
                interlace,
                &lzw)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> gif;
        gif.reserve(32u + palette_source.size() + lzw.size());
        gif.push_back('G');
        gif.push_back('I');
        gif.push_back('F');
        gif.push_back('8');
        gif.push_back(static_cast<std::uint8_t>((has_transparency || comment_size > 0u || interlace) ? '9' : '7'));
        gif.push_back('a');
        append_le16(gif, static_cast<std::uint16_t>(image->width));
        append_le16(gif, static_cast<std::uint16_t>(image->height));
        int table_size_code = 0;
        for (int entries = 2; entries < color_table_entries; entries <<= 1) {
            ++table_size_code;
        }
        const int color_resolution = std::max(0, min_code_size - 1);
        gif.push_back(static_cast<std::uint8_t>(0x80 | ((color_resolution & 0x07) << 4) | (table_size_code & 0x07)));
        gif.push_back(0);
        gif.push_back(0);
        gif.insert(gif.end(), palette_source.begin(), palette_source.end());
        gif.resize(gif.size() + static_cast<std::size_t>(color_table_entries) * 3u - palette_source.size(), 0);

        append_gif_comment_extension(gif, comment, comment_size);

        if (has_transparency) {
            gif.insert(gif.end(), {0x21, 0xf9, 0x04, 0x01});
            append_le16(gif, 0);
            gif.push_back(static_cast<std::uint8_t>(transparency & 0xff));
            gif.push_back(0);
        }

        gif.push_back(0x2c);
        append_le16(gif, 0);
        append_le16(gif, 0);
        append_le16(gif, static_cast<std::uint16_t>(image->width));
        append_le16(gif, static_cast<std::uint16_t>(image->height));
        gif.push_back(static_cast<std::uint8_t>(interlace ? 0x40 : 0));
        gif.push_back(static_cast<std::uint8_t>(min_code_size));
        append_gif_sub_blocks(gif, lzw);
        gif.push_back(0x3b);

        if (!write_binary_file(path, gif)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_gif_image(const PillowCImage* image, const char* path)
{
    return save_gif_image_with_comment(image, path, nullptr, 0u);
}

struct GifBitWriter {
    std::vector<std::uint8_t> bytes;
    std::uint32_t bits = 0;
    int bit_count = 0;

    void write(int code, int size)
    {
        bits |= static_cast<std::uint32_t>(code) << bit_count;
        bit_count += size;
        while (bit_count >= 8) {
            bytes.push_back(static_cast<std::uint8_t>(bits & 0xffu));
            bits >>= 8;
            bit_count -= 8;
        }
    }

    void flush()
    {
        if (bit_count > 0) {
            bytes.push_back(static_cast<std::uint8_t>(bits & 0xffu));
            bits = 0;
            bit_count = 0;
        }
    }
};

int gif_color_table_entries(const PillowCImage* image, int* out_entries, int* out_min_code_size)
{
    if (!image || !out_entries || !out_min_code_size ||
        image->palette_rgb.empty() || image->palette_rgb.size() % 3u != 0u ||
        image->palette_rgb.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t palette_entries = image->palette_rgb.size() / 3u;
    int table_entries = 2;
    while (static_cast<std::size_t>(table_entries) < palette_entries) {
        table_entries <<= 1;
    }
    if (table_entries > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int bits = 0;
    while ((1 << bits) < table_entries) {
        ++bits;
    }
    *out_entries = table_entries;
    *out_min_code_size = std::max(2, bits);
    return PILLOW_C_OK;
}

void append_gif_sub_blocks(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& data)
{
    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::size_t chunk = std::min<std::size_t>(255u, data.size() - offset);
        out.push_back(static_cast<std::uint8_t>(chunk));
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(offset),
                   data.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
        offset += chunk;
    }
    out.push_back(0);
}

void append_gif_comment_extension(
    std::vector<std::uint8_t>& out,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (!comment || comment_size == 0u) {
        return;
    }
    out.push_back(0x21);
    out.push_back(0xfe);
    std::size_t offset = 0;
    while (offset < comment_size) {
        const std::size_t chunk = std::min<std::size_t>(255u, comment_size - offset);
        out.push_back(static_cast<std::uint8_t>(chunk));
        out.insert(
            out.end(),
            comment + offset,
            comment + offset + chunk);
        offset += chunk;
    }
    out.push_back(0);
}

bool gif_lzw_encode_indices(
    const std::uint8_t* pixels,
    int width,
    int height,
    std::size_t stride,
    int color_table_entries,
    int min_code_size,
    std::vector<std::uint8_t>* out);

bool gif_lzw_encode_indices_interlaced(
    const std::uint8_t* pixels,
    int width,
    int height,
    std::size_t stride,
    int color_table_entries,
    int min_code_size,
    bool interlace,
    std::vector<std::uint8_t>* out)
{
    if (!pixels || !out || width <= 0 || height <= 0 || stride < static_cast<std::size_t>(width) ||
        color_table_entries <= 0 || min_code_size < 2 || min_code_size > 8) {
        return false;
    }
    const int clear_code = 1 << min_code_size;
    const int end_code = clear_code + 1;
    int next_code = end_code + 1;
    int code_size = min_code_size + 1;

    GifBitWriter writer;
    std::unordered_map<std::uint32_t, int> dictionary;
    dictionary.reserve(4096);

    // GIF interlace visits rows in the classic four passes.
    const int pass_starts[4] = {0, 4, 2, 1};
    const int pass_steps[4] = {8, 8, 4, 2};
    const int pass_count = interlace ? 4 : 1;

    writer.write(clear_code, code_size);
    int prefix = -1;
    for (int pass = 0; pass < pass_count; ++pass) {
        const int start = interlace ? pass_starts[pass] : 0;
        const int step = interlace ? pass_steps[pass] : 1;
        for (int y = start; y < height; y += step) {
            const std::uint8_t* row = pixels + static_cast<std::size_t>(y) * stride;
            for (int x = 0; x < width; ++x) {
                const int value = row[x];
                if (value < 0 || value >= color_table_entries) {
                    return false;
                }
                if (prefix < 0) {
                    prefix = value;
                    continue;
                }

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
                    if (next_code > (1 << code_size) && code_size < 12) {
                        ++code_size;
                    }
                } else {
                    writer.write(clear_code, code_size);
                    dictionary.clear();
                    next_code = end_code + 1;
                    code_size = min_code_size + 1;
                }
                prefix = value;
            }
        }
    }
    if (prefix < 0) {
        return false;
    }
    writer.write(prefix, code_size);
    writer.write(end_code, code_size);
    writer.flush();
    *out = std::move(writer.bytes);
    return true;
}

bool gif_lzw_encode_image(
    const PillowCImage* image,
    int color_table_entries,
    int min_code_size,
    std::vector<std::uint8_t>* out)
{
    if (!image || !out || image->width <= 0 || image->height <= 0 || color_table_entries <= 0) {
        return false;
    }
    return gif_lzw_encode_indices(
        image->pixels.data(),
        image->width,
        image->height,
        image->stride,
        color_table_entries,
        min_code_size,
        out);
}

bool gif_lzw_encode_indices(
    const std::uint8_t* pixels,
    int width,
    int height,
    std::size_t stride,
    int color_table_entries,
    int min_code_size,
    std::vector<std::uint8_t>* out)
{
    return gif_lzw_encode_indices_interlaced(
        pixels, width, height, stride, color_table_entries, min_code_size, false, out);
}

int gif_table_size_code_for_entries(int entries)
{
    int table_size_code = 0;
    for (int size = 2; size < entries; size <<= 1) {
        ++table_size_code;
    }
    return table_size_code;
}

int gif_color_table_entries_for_palette_size(std::size_t palette_rgb_size, int* out_entries, int* out_min_code_size)
{
    if (!out_entries || !out_min_code_size ||
        palette_rgb_size == 0u || palette_rgb_size % 3u != 0u || palette_rgb_size > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t palette_entries = palette_rgb_size / 3u;
    int table_entries = 2;
    while (static_cast<std::size_t>(table_entries) < palette_entries) {
        table_entries <<= 1;
    }
    if (table_entries > 256) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int bits = 0;
    while ((1 << bits) < table_entries) {
        ++bits;
    }
    *out_entries = table_entries;
    *out_min_code_size = std::max(2, bits);
    return PILLOW_C_OK;
}

struct GifAnimationRect {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

bool gif_palette_index_rgb(const PillowCImage* image, std::uint8_t index, std::uint8_t* rgb)
{
    if (!image || !rgb || image->palette_rgb.size() % 3u != 0u || image->palette_rgb.size() > 256u * 3u) {
        return false;
    }
    const std::size_t offset = static_cast<std::size_t>(index) * 3u;
    if (offset + 2u >= image->palette_rgb.size()) {
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0;
        return true;
    }
    rgb[0] = image->palette_rgb[offset + 0u];
    rgb[1] = image->palette_rgb[offset + 1u];
    rgb[2] = image->palette_rgb[offset + 2u];
    return true;
}

bool gif_palette_pixels_equal(
    const PillowCImage* left,
    std::uint8_t left_index,
    const PillowCImage* right,
    std::uint8_t right_index)
{
    std::uint8_t left_rgb[3] = {};
    std::uint8_t right_rgb[3] = {};
    return gif_palette_index_rgb(left, left_index, left_rgb) &&
           gif_palette_index_rgb(right, right_index, right_rgb) &&
           left_rgb[0] == right_rgb[0] &&
           left_rgb[1] == right_rgb[1] &&
           left_rgb[2] == right_rgb[2];
}

void zero_unused_gif_palette_entries(
    const PillowCImage* image,
    int left,
    int top,
    int width,
    int height,
    std::vector<std::uint8_t>* palette_rgb)
{
    if (!image || !palette_rgb || palette_rgb->empty() || palette_rgb->size() % 3u != 0u ||
        width <= 0 || height <= 0) {
        return;
    }
    std::vector<bool> used_indices(palette_rgb->size() / 3u, false);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(top + y) * image->stride;
        for (int x = 0; x < width; ++x) {
            const std::size_t index = row[left + x];
            if (index < used_indices.size()) {
                used_indices[index] = true;
            }
        }
    }
    for (std::size_t index = 0; index < used_indices.size(); ++index) {
        if (!used_indices[index]) {
            const std::size_t palette_offset = index * 3u;
            (*palette_rgb)[palette_offset] = 0;
            (*palette_rgb)[palette_offset + 1u] = 0;
            (*palette_rgb)[palette_offset + 2u] = 0;
        }
    }
}

bool gif_difference_bbox(const PillowCImage* previous, const PillowCImage* current, GifAnimationRect* out_rect)
{
    if (!previous || !current || !out_rect ||
        previous->width != current->width || previous->height != current->height ||
        previous->channels != 1 || current->channels != 1) {
        return false;
    }
    int left = current->width;
    int top = current->height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < current->height; ++y) {
        const std::uint8_t* prev_row = previous->pixels.data() + static_cast<std::size_t>(y) * previous->stride;
        const std::uint8_t* curr_row = current->pixels.data() + static_cast<std::size_t>(y) * current->stride;
        for (int x = 0; x < current->width; ++x) {
            if (gif_palette_pixels_equal(previous, prev_row[x], current, curr_row[x])) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    if (right < 0 || bottom < 0) {
        out_rect->left = 0;
        out_rect->top = 0;
        out_rect->width = 0;
        out_rect->height = 0;
        return true;
    }
    out_rect->left = left;
    out_rect->top = top;
    out_rect->width = right - left;
    out_rect->height = bottom - top;
    return true;
}

bool gif_difference_bbox_against_background_rgb(
    const PillowCImage* current,
    const std::uint8_t background_rgb[3],
    GifAnimationRect* out_rect)
{
    if (!current || !background_rgb || !out_rect || current->channels != 1) {
        return false;
    }
    int left = current->width;
    int top = current->height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < current->height; ++y) {
        const std::uint8_t* curr_row = current->pixels.data() + static_cast<std::size_t>(y) * current->stride;
        for (int x = 0; x < current->width; ++x) {
            std::uint8_t curr_rgb[3] = {};
            if (!gif_palette_index_rgb(current, curr_row[x], curr_rgb)) {
                return false;
            }
            if (curr_rgb[0] == background_rgb[0] &&
                curr_rgb[1] == background_rgb[1] &&
                curr_rgb[2] == background_rgb[2]) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    if (right < 0 || bottom < 0) {
        out_rect->left = 0;
        out_rect->top = 0;
        out_rect->width = 0;
        out_rect->height = 0;
        return true;
    }
    out_rect->left = left;
    out_rect->top = top;
    out_rect->width = right - left;
    out_rect->height = bottom - top;
    return true;
}

bool gif_difference_bbox_against_background_index(
    const PillowCImage* current,
    std::uint8_t background_index,
    GifAnimationRect* out_rect)
{
    if (!current || !out_rect || current->channels != 1) {
        return false;
    }
    int left = current->width;
    int top = current->height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < current->height; ++y) {
        const std::uint8_t* curr_row = current->pixels.data() + static_cast<std::size_t>(y) * current->stride;
        for (int x = 0; x < current->width; ++x) {
            if (curr_row[x] == background_index) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    if (right < 0 || bottom < 0) {
        out_rect->left = 0;
        out_rect->top = 0;
        out_rect->width = 0;
        out_rect->height = 0;
        return true;
    }
    out_rect->left = left;
    out_rect->top = top;
    out_rect->width = right - left;
    out_rect->height = bottom - top;
    return true;
}

int gif_find_unused_palette_index(const PillowCImage* image, int palette_entries)
{
    if (!image || palette_entries < 0 || palette_entries > 256) {
        return -1;
    }
    if (palette_entries < 256) {
        return palette_entries;
    }
    bool used[256] = {};
    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            used[row[x]] = true;
        }
    }
    for (int index = 255; index >= 0; --index) {
        if (!used[index]) {
            return index;
        }
    }
    return -1;
}

bool gif_image_contains_palette_index(const PillowCImage* image, int index)
{
    if (!image || image->channels != 1 || index < 0 || index > 255) {
        return false;
    }
    const auto needle = static_cast<std::uint8_t>(index);
    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        if (std::find(row, row + image->width, needle) != row + image->width) {
            return true;
        }
    }
    return false;
}

bool gif_image_uses_only_palette_index(const PillowCImage* image, int index)
{
    if (!image || image->channels != 1 || index < 0 || index > 255) {
        return false;
    }
    const auto needle = static_cast<std::uint8_t>(index);
    for (int y = 0; y < image->height; ++y) {
        const std::uint8_t* row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            if (row[x] != needle) {
                return false;
            }
        }
    }
    return true;
}

int gif_sequence_value(const int* values, std::size_t count, std::size_t index, int fallback, bool* ok)
{
    if (!ok || count == 0 || !values) {
        return fallback;
    }
    if (count == 1) {
        return values[0];
    }
    if (index >= count) {
        *ok = false;
        return fallback;
    }
    return values[index];
}

int save_gif_indexed_native(
    const PillowCImage* image,
    const char* path,
    bool has_transparency,
    int transparency,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (comment_size > 0u && !comment) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->width <= 0 || image->height <= 0 ||
        image->mode != PILLOW_C_MODE_P || image->channels != 1 ||
        image->width > std::numeric_limits<std::uint16_t>::max() ||
        image->height > std::numeric_limits<std::uint16_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    int color_table_entries = 0;
    int min_code_size = 0;
    int status = gif_color_table_entries(image, &color_table_entries, &min_code_size);
    if (status != PILLOW_C_OK) {
        return status;
    }

    try {
        std::vector<std::uint8_t> lzw;
        if (!gif_lzw_encode_image(image, color_table_entries, min_code_size, &lzw)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> gif;
        gif.reserve(32u + image->palette_rgb.size() + lzw.size());
        gif.push_back('G');
        gif.push_back('I');
        gif.push_back('F');
        gif.push_back('8');
        gif.push_back(static_cast<std::uint8_t>((has_transparency || comment_size > 0u) ? '9' : '7'));
        gif.push_back('a');
        append_le16(gif, static_cast<std::uint16_t>(image->width));
        append_le16(gif, static_cast<std::uint16_t>(image->height));
        int table_size_code = 0;
        for (int entries = 2; entries < color_table_entries; entries <<= 1) {
            ++table_size_code;
        }
        const int color_resolution = std::max(0, min_code_size - 1);
        gif.push_back(static_cast<std::uint8_t>(0x80 | ((color_resolution & 0x07) << 4) | (table_size_code & 0x07)));
        gif.push_back(0);
        gif.push_back(0);
        gif.insert(gif.end(), image->palette_rgb.begin(), image->palette_rgb.end());
        gif.resize(gif.size() + static_cast<std::size_t>(color_table_entries) * 3u - image->palette_rgb.size(), 0);

        append_gif_comment_extension(gif, comment, comment_size);

        if (has_transparency) {
            gif.insert(gif.end(), {0x21, 0xf9, 0x04, 0x01});
            append_le16(gif, 0);
            gif.push_back(static_cast<std::uint8_t>(transparency & 0xff));
            gif.push_back(0);
        }

        gif.push_back(0x2c);
        append_le16(gif, 0);
        append_le16(gif, 0);
        append_le16(gif, static_cast<std::uint16_t>(image->width));
        append_le16(gif, static_cast<std::uint16_t>(image->height));
        gif.push_back(0);
        gif.push_back(static_cast<std::uint8_t>(min_code_size));
        append_gif_sub_blocks(gif, lzw);
        gif.push_back(0x3b);

        if (!write_binary_file(path, gif)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int save_gif_animation_image(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency,
    int has_background,
    int background,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (!images || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    if (comment_size > 0u && !comment) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image_count == 0 || image_count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        (duration_count != 0 && duration_count != 1 && duration_count != image_count) ||
        (disposal_count != 0 && disposal_count != 1 && disposal_count != image_count) ||
        loop < -1 ||
        include_color_table < -1 || include_color_table > 1 ||
        optimize < -1 || optimize > 1 ||
        (has_transparency != 0 && has_transparency != 1) ||
        (has_transparency && (transparency < 0 || transparency > 255)) ||
        (has_background != 0 && has_background != 1) ||
        (has_background && (background < 0 || background > 255))) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool force_first_local_color_table = include_color_table == 1;
    const bool optimize_enabled = optimize != 0;
    const bool user_has_transparency = has_transparency != 0;
    bool caller_has_transparency = user_has_transparency;
    int caller_transparency = transparency & 0xff;
    const int logical_screen_background = has_background ? (background & 0xff) : 0;
    const PillowCImage* first_input = images[0];
    if (!first_input || first_input->width <= 0 || first_input->height <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<PillowCImage> quantized_images;
    std::vector<const PillowCImage*> quantized_image_ptrs;
    std::vector<bool> quantized_frame_has_transparency;
    bool rgba_animation_transparency = false;
    if (first_input->mode != PILLOW_C_MODE_P || first_input->channels != 1) {
        if (!((first_input->mode == PILLOW_C_MODE_L && first_input->channels == 1) ||
              (first_input->mode == PILLOW_C_MODE_LA && first_input->channels == 2) ||
              (first_input->mode == PILLOW_C_MODE_RGB && first_input->channels == 3) ||
              (first_input->mode == PILLOW_C_MODE_RGBA && first_input->channels == 4))) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        try {
            quantized_images.reserve(image_count);
            quantized_image_ptrs.reserve(image_count);
            quantized_frame_has_transparency.reserve(image_count);
            for (std::size_t i = 0; i < image_count; ++i) {
                const PillowCImage* image = images[i];
                const bool frame_is_l =
                    image && image->mode == PILLOW_C_MODE_L && image->channels == 1;
                const bool frame_is_la =
                    image && image->mode == PILLOW_C_MODE_LA && image->channels == 2;
                const bool frame_is_rgb =
                    image && image->mode == PILLOW_C_MODE_RGB && image->channels == 3;
                const bool frame_is_rgba =
                    image && image->mode == PILLOW_C_MODE_RGBA && image->channels == 4;
                if (!image || (!frame_is_l && !frame_is_la && !frame_is_rgb && !frame_is_rgba) ||
                    image->width != first_input->width || image->height != first_input->height ||
                    image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
                    return i == 0 ? PILLOW_C_INVALID_ARGUMENT : PILLOW_C_MISMATCH;
                }
                const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
                if (refresh_status != PILLOW_C_OK) {
                    return refresh_status;
                }
                std::size_t stride = 0;
                std::size_t size = 0;
                if (!checked_image_size_allow_empty(image->width, image->height, 1, &stride, &size)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                quantized_images.push_back(PillowCImage{
                    image->width,
                    image->height,
                    PILLOW_C_MODE_P,
                    1,
                    stride,
                    std::vector<std::uint8_t>(size)});
                int quantize_status = PILLOW_C_OK;
                if (frame_is_rgba) {
                    bool frame_has_transparency = false;
                    quantize_status = pillow_c_quantize_exact_rgba_gif_animation_frame_into(
                        image,
                        &quantized_images.back(),
                        true,
                        &frame_has_transparency);
                    if (quantize_status == PILLOW_C_INVALID_ARGUMENT) {
                        int frame_transparency = 0;
                        quantize_status = pillow_c_quantize_median_cut_rgba_gif_into(
                            image,
                            &quantized_images.back(),
                            &frame_has_transparency,
                            &frame_transparency);
                    }
                    quantized_frame_has_transparency.push_back(frame_has_transparency);
                    rgba_animation_transparency = rgba_animation_transparency || frame_has_transparency;
                } else if (frame_is_la) {
                    quantize_status = pillow_c_quantize_exact_la_gif_animation_frame_into(image, &quantized_images.back());
                    quantized_frame_has_transparency.push_back(false);
                } else {
                    quantize_status = pillow_c_quantize_exact_image_into(image, 256, &quantized_images.back());
                    quantized_frame_has_transparency.push_back(false);
                }
                if (quantize_status != PILLOW_C_OK) {
                    return quantize_status;
                }
            }
            if (rgba_animation_transparency && !user_has_transparency) {
                caller_has_transparency = true;
                caller_transparency = 0;
            }
            for (const PillowCImage& image : quantized_images) {
                quantized_image_ptrs.push_back(&image);
            }
            images = quantized_image_ptrs.data();
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }

    const PillowCImage* first = images[0];
    if (!first || first->width <= 0 || first->height <= 0 ||
        first->mode != PILLOW_C_MODE_P || first->channels != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int color_table_entries = 0;
    int min_code_size = 0;
    int status = gif_color_table_entries(first, &color_table_entries, &min_code_size);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const int first_source_palette_entries = static_cast<int>(first->palette_rgb.size() / 3u);
    if (color_table_entries < 4) {
        color_table_entries = 4;
    }
    if (caller_has_transparency) {
        while (color_table_entries <= caller_transparency && color_table_entries < 256) {
            color_table_entries <<= 1;
        }
        if (color_table_entries <= caller_transparency) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    min_code_size = 8;
    if (first->width > std::numeric_limits<std::uint16_t>::max() ||
        first->height > std::numeric_limits<std::uint16_t>::max()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    for (std::size_t i = 0; i < image_count; ++i) {
        const PillowCImage* image = images[i];
        if (!image || image->mode != PILLOW_C_MODE_P || image->channels != 1 ||
            image->width != first->width || image->height != first->height ||
            image->stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
            return i == 0 ? PILLOW_C_INVALID_ARGUMENT : PILLOW_C_MISMATCH;
        }
        const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
        if (refresh_status != PILLOW_C_OK) {
            return refresh_status;
        }
        int image_color_table_entries = 0;
        int image_min_code_size = 0;
        status = gif_color_table_entries(image, &image_color_table_entries, &image_min_code_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    try {
        std::vector<std::uint8_t> global_palette_rgb = first->palette_rgb;
        if (optimize_enabled && first_source_palette_entries > 2) {
            zero_unused_gif_palette_entries(
                first,
                0,
                0,
                first->width,
                first->height,
                &global_palette_rgb);
        }

        struct GifAnimationOutputFrame {
            int left = 0;
            int top = 0;
            int width = 0;
            int height = 0;
            int duration_ms = 0;
            int disposal = 0;
            int transparency = -1;
            int color_table_entries = 0;
            int min_code_size = 0;
            bool include_local_color_table = false;
            std::vector<std::uint8_t> palette_rgb;
            std::vector<std::uint8_t> palette_index_map;
            std::vector<std::uint8_t> pixels;
        };

        std::vector<GifAnimationOutputFrame> frames;
        frames.reserve(image_count);
        const PillowCImage* previous_image = nullptr;

        for (std::size_t i = 0; i < image_count; ++i) {
            bool ok = true;
            const int duration = gif_sequence_value(durations_ms, duration_count, i, 0, &ok);
            const int disposal = gif_sequence_value(disposals, disposal_count, i, 0, &ok);
            if (!ok || duration < 0 || disposal < 0 || disposal > 7) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            const PillowCImage* image = images[i];
            const bool current_frame_has_source_transparency =
                rgba_animation_transparency &&
                i < quantized_frame_has_transparency.size() &&
                quantized_frame_has_transparency[i];
            GifAnimationOutputFrame frame;
            frame.duration_ms = duration;
            frame.disposal = disposal;

            if (frames.empty()) {
                const bool first_rgb_frame_preserves_user_transparency =
                    user_has_transparency && first_input->mode == PILLOW_C_MODE_RGB;
                const bool first_frame_uses_user_transparency =
                    user_has_transparency &&
                    first_input->mode == PILLOW_C_MODE_P &&
                    gif_image_contains_palette_index(image, caller_transparency);
                const bool first_frame_collapses_to_transparency =
                    user_has_transparency &&
                    first_input->mode == PILLOW_C_MODE_P &&
                    optimize_enabled &&
                    first_source_palette_entries <= 2 &&
                    caller_transparency < first_source_palette_entries &&
                    !gif_image_uses_only_palette_index(image, caller_transparency);
                frame.left = 0;
                frame.top = 0;
                frame.width = image->width;
                frame.height = image->height;
                frame.color_table_entries = color_table_entries;
                frame.min_code_size = min_code_size;
                frame.include_local_color_table = force_first_local_color_table;
                if (current_frame_has_source_transparency) {
                    frame.transparency = caller_transparency;
                } else if (caller_has_transparency &&
                    (first_frame_uses_user_transparency ||
                     first_frame_collapses_to_transparency ||
                     first_rgb_frame_preserves_user_transparency ||
                     !optimize_enabled ||
                     caller_transparency >= first_source_palette_entries)) {
                    frame.transparency = caller_transparency;
                }
                if (frame.include_local_color_table) {
                    frame.palette_rgb = global_palette_rgb;
                    if (caller_has_transparency && frame.transparency >= 0) {
                        const int current_palette_entries = static_cast<int>(frame.palette_rgb.size() / 3u);
                        if (frame.transparency >= current_palette_entries) {
                            frame.palette_rgb.resize(
                                (static_cast<std::size_t>(frame.transparency) + 1u) * 3u, 0);
                        }
                    }
                }
                frame.pixels.resize(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height));
                for (int y = 0; y < image->height; ++y) {
                    const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                    std::uint8_t* dst_row = frame.pixels.data() + static_cast<std::size_t>(y) * image->width;
                    std::copy_n(src_row, image->width, dst_row);
                }
                frames.push_back(std::move(frame));
                previous_image = image;
                continue;
            }

            GifAnimationRect rect;
            if (!gif_difference_bbox(previous_image, image, &rect)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (rect.width == 0 || rect.height == 0) {
                if (duration > 0) {
                    const long long merged_duration =
                        static_cast<long long>(frames.back().duration_ms) + static_cast<long long>(duration);
                    frames.back().duration_ms = merged_duration > std::numeric_limits<int>::max()
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(merged_duration);
                }
                continue;
            }

            const bool previous_restores_to_background = frames.back().disposal == 2;
            const int source_palette_entries = static_cast<int>(image->palette_rgb.size() / 3u);
            const bool use_transparency_background_rediff =
                previous_restores_to_background && caller_has_transparency;
            bool restored_background_frame = false;
            if (use_transparency_background_rediff) {
                bool compare_background_index = false;
                if (first->palette_rgb == image->palette_rgb) {
                    if (!optimize_enabled ||
                        (source_palette_entries <= 2 && caller_transparency >= source_palette_entries)) {
                        compare_background_index = true;
                    } else {
                        std::vector<std::uint8_t> current_optimized_palette = image->palette_rgb;
                        zero_unused_gif_palette_entries(
                            image,
                            0,
                            0,
                            image->width,
                            image->height,
                            &current_optimized_palette);
                        compare_background_index = current_optimized_palette == global_palette_rgb;
                    }
                } else if (optimize_enabled && source_palette_entries > 2) {
                    std::vector<std::uint8_t> current_optimized_palette = image->palette_rgb;
                    zero_unused_gif_palette_entries(
                        image,
                        0,
                        0,
                        image->width,
                        image->height,
                        &current_optimized_palette);
                    compare_background_index = current_optimized_palette == global_palette_rgb;
                }
                if (compare_background_index) {
                    if (!gif_difference_bbox_against_background_index(
                            image, static_cast<std::uint8_t>(caller_transparency), &rect)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                } else {
                    std::uint8_t background_rgb[3] = {};
                    if (!gif_palette_index_rgb(
                            first, static_cast<std::uint8_t>(caller_transparency), background_rgb) ||
                        !gif_difference_bbox_against_background_rgb(image, background_rgb, &rect)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                }
                if (rect.width == 0 || rect.height == 0) {
                    rect.left = 0;
                    rect.top = 0;
                    rect.width = image->width;
                    rect.height = image->height;
                    restored_background_frame = true;
                }
            } else if (previous_restores_to_background) {
                rect.left = 0;
                rect.top = 0;
                rect.width = image->width;
                rect.height = image->height;
            }

            frame.left = rect.left;
            frame.top = rect.top;
            frame.width = rect.width;
            frame.height = rect.height;
            frame.include_local_color_table = true;
            frame.palette_rgb = image->palette_rgb;
            if ((restored_background_frame ||
                 (previous_restores_to_background && optimize_enabled && !caller_has_transparency)) &&
                optimize_enabled) {
                zero_unused_gif_palette_entries(
                    image,
                    frame.left,
                    frame.top,
                    frame.width,
                    frame.height,
                    &frame.palette_rgb);
            }
            bool generated_optimized_transparency = false;
            const bool optimized_transparent_restored_frame =
                optimize_enabled && restored_background_frame && caller_has_transparency && has_background != 0;
            const bool use_caller_transparency_for_frame =
                current_frame_has_source_transparency ||
                (caller_has_transparency && !rgba_animation_transparency &&
                (!use_transparency_background_rediff || !optimize_enabled ||
                 caller_transparency >= source_palette_entries));
            if (optimized_transparent_restored_frame) {
                frame.transparency = 0;
                if (frame.palette_rgb.size() < 3u) {
                    frame.palette_rgb.resize(3u, 0);
                } else {
                    frame.palette_rgb[0] = 0;
                    frame.palette_rgb[1] = 0;
                    frame.palette_rgb[2] = 0;
                }
            } else if (use_caller_transparency_for_frame) {
                frame.transparency = caller_transparency;
                const int current_palette_entries = static_cast<int>(frame.palette_rgb.size() / 3u);
                if (frame.transparency >= current_palette_entries) {
                    frame.palette_rgb.resize((static_cast<std::size_t>(frame.transparency) + 1u) * 3u, 0);
                } else if (optimize_enabled && !previous_restores_to_background &&
                           current_palette_entries < 256) {
                    frame.palette_rgb.resize((static_cast<std::size_t>(current_palette_entries) + 1u) * 3u, 0);
                }
            } else if (optimize_enabled && !previous_restores_to_background) {
                frame.transparency = gif_find_unused_palette_index(image, source_palette_entries);
                generated_optimized_transparency = true;
                if (frame.transparency >= source_palette_entries) {
                    frame.palette_rgb.resize((static_cast<std::size_t>(frame.transparency) + 1u) * 3u, 0);
                }
            }
            if (!optimize_enabled &&
                restored_background_frame &&
                caller_has_transparency &&
                frame.transparency == caller_transparency) {
                bool all_transparency_index = true;
                for (int y = 0; y < frame.height && all_transparency_index; ++y) {
                    const std::uint8_t* curr_row = image->pixels.data() +
                        static_cast<std::size_t>(frame.top + y) * image->stride;
                    for (int x = 0; x < frame.width; ++x) {
                        if (curr_row[frame.left + x] != static_cast<std::uint8_t>(caller_transparency)) {
                            all_transparency_index = false;
                            break;
                        }
                    }
                }
                if (all_transparency_index) {
                    frame.include_local_color_table = false;
                }
            }
            if (optimize_enabled && frame.include_local_color_table &&
                !optimized_transparent_restored_frame &&
                (caller_has_transparency || generated_optimized_transparency)) {
                bool used_nontransparent_indices[256] = {};
                for (int y = 0; y < frame.height; ++y) {
                    const std::uint8_t* prev_row = previous_image->pixels.data() +
                        static_cast<std::size_t>(frame.top + y) * previous_image->stride;
                    const std::uint8_t* curr_row = image->pixels.data() +
                        static_cast<std::size_t>(frame.top + y) * image->stride;
                    for (int x = 0; x < frame.width; ++x) {
                        const int src_x = frame.left + x;
                        std::uint8_t value = curr_row[src_x];
                        if (frame.transparency >= 0 &&
                            gif_palette_pixels_equal(previous_image, prev_row[src_x], image, curr_row[src_x])) {
                            value = static_cast<std::uint8_t>(frame.transparency);
                        }
                        if (frame.transparency < 0 || value != static_cast<std::uint8_t>(frame.transparency)) {
                            used_nontransparent_indices[value] = true;
                        }
                    }
                }

                std::vector<std::uint8_t> compact_palette;
                std::vector<std::uint8_t> compact_index_map(256u, 0);
                auto copy_palette_entry = [&](int source_index, int target_index) -> bool {
                    if (source_index < 0 || source_index > 255 || target_index < 0 || target_index > 255) {
                        return false;
                    }
                    std::uint8_t rgb[3] = {};
                    if (!gif_palette_index_rgb(image, static_cast<std::uint8_t>(source_index), rgb)) {
                        return false;
                    }
                    const std::size_t required_size = (static_cast<std::size_t>(target_index) + 1u) * 3u;
                    if (compact_palette.size() < required_size) {
                        compact_palette.resize(required_size, 0);
                    }
                    const std::size_t offset = static_cast<std::size_t>(target_index) * 3u;
                    compact_palette[offset] = rgb[0];
                    compact_palette[offset + 1u] = rgb[1];
                    compact_palette[offset + 2u] = rgb[2];
                    compact_index_map[static_cast<std::size_t>(source_index)] =
                        static_cast<std::uint8_t>(target_index);
                    return true;
                };
                auto reserve_palette_entry = [&](int target_index) -> bool {
                    if (target_index < 0 || target_index > 255) {
                        return false;
                    }
                    const std::size_t required_size = (static_cast<std::size_t>(target_index) + 1u) * 3u;
                    if (compact_palette.size() < required_size) {
                        compact_palette.resize(required_size, 0);
                    }
                    compact_index_map[static_cast<std::size_t>(target_index)] =
                        static_cast<std::uint8_t>(target_index);
                    return true;
                };

                if (!copy_palette_entry(0, 0)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                if (caller_has_transparency && frame.transparency >= 0) {
                    const bool transparency_has_source_palette_entry = frame.transparency < source_palette_entries;
                    if (transparency_has_source_palette_entry) {
                        if (!copy_palette_entry(frame.transparency, frame.transparency)) {
                            return PILLOW_C_INVALID_ARGUMENT;
                        }
                    } else if (!reserve_palette_entry(frame.transparency)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                }
                int next_compact_index = 1;
                for (int source_index = 1; source_index < 256; ++source_index) {
                    if (!used_nontransparent_indices[source_index] ||
                        (caller_has_transparency && source_index == frame.transparency)) {
                        continue;
                    }
                    while (caller_has_transparency && next_compact_index == frame.transparency) {
                        ++next_compact_index;
                    }
                    if (next_compact_index > 255 ||
                        !copy_palette_entry(source_index, next_compact_index)) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    ++next_compact_index;
                }
                if (generated_optimized_transparency && frame.transparency >= 0) {
                    if (next_compact_index > 255) {
                        return PILLOW_C_INVALID_ARGUMENT;
                    }
                    const std::size_t required_size =
                        (static_cast<std::size_t>(next_compact_index) + 1u) * 3u;
                    if (compact_palette.size() < required_size) {
                        compact_palette.resize(required_size, 0);
                    }
                    frame.transparency = next_compact_index;
                }
                if (!compact_palette.empty()) {
                    frame.palette_rgb = std::move(compact_palette);
                    frame.palette_index_map = std::move(compact_index_map);
                }
            }
            if (frame.include_local_color_table) {
                status = gif_color_table_entries_for_palette_size(
                    frame.palette_rgb.size(), &frame.color_table_entries, &frame.min_code_size);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            } else {
                frame.color_table_entries = color_table_entries;
                frame.min_code_size = min_code_size;
            }
            if (frame.color_table_entries < 4) {
                frame.color_table_entries = 4;
            }
            frame.min_code_size = 8;

            frame.pixels.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height));
            if (optimized_transparent_restored_frame) {
                std::fill(frame.pixels.begin(), frame.pixels.end(), std::uint8_t{0});
            } else {
                for (int y = 0; y < frame.height; ++y) {
                    const std::uint8_t* prev_row = previous_image->pixels.data() +
                        static_cast<std::size_t>(frame.top + y) * previous_image->stride;
                    const std::uint8_t* curr_row = image->pixels.data() +
                        static_cast<std::size_t>(frame.top + y) * image->stride;
                    std::uint8_t* dst_row = frame.pixels.data() + static_cast<std::size_t>(y) * frame.width;
                    for (int x = 0; x < frame.width; ++x) {
                        const int src_x = frame.left + x;
                        std::uint8_t value = curr_row[src_x];
                        bool transparent_pixel = false;
                        if (optimize_enabled && frame.transparency >= 0 &&
                            gif_palette_pixels_equal(previous_image, prev_row[src_x], image, curr_row[src_x])) {
                            value = static_cast<std::uint8_t>(frame.transparency);
                            transparent_pixel = true;
                        }
                        if (!frame.palette_index_map.empty() && !transparent_pixel) {
                            value = frame.palette_index_map[value];
                        }
                        dst_row[x] = value;
                    }
                }
            }

            frames.push_back(std::move(frame));
            previous_image = image;
        }

        if (frames.size() == 1u && disposal_count > 1u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> gif;
        gif.reserve(32u + global_palette_rgb.size() + frames.size() * (first->pixels.size() + 32u));
        gif.insert(gif.end(), {'G', 'I', 'F', '8', '9', 'a'});
        append_le16(gif, static_cast<std::uint16_t>(first->width));
        append_le16(gif, static_cast<std::uint16_t>(first->height));
        const int table_size_code = gif_table_size_code_for_entries(color_table_entries);
        const int color_resolution = std::max(0, min_code_size - 1);
        gif.push_back(static_cast<std::uint8_t>(0x80 | ((color_resolution & 0x07) << 4) | (table_size_code & 0x07)));
        gif.push_back(static_cast<std::uint8_t>(logical_screen_background));
        gif.push_back(0);
        gif.insert(gif.end(), global_palette_rgb.begin(), global_palette_rgb.end());
        gif.resize(gif.size() + static_cast<std::size_t>(color_table_entries) * 3u - global_palette_rgb.size(), 0);

        if (loop >= 0) {
            gif.insert(gif.end(), {0x21, 0xff, 0x0b, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
                                   0x03, 0x01});
            append_le16(gif, static_cast<std::uint16_t>(loop));
            gif.push_back(0);
        }

        append_gif_comment_extension(gif, comment, comment_size);

        for (const GifAnimationOutputFrame& frame : frames) {
            const int delay_cs = std::min(frame.duration_ms / 10, 65535);
            const std::uint8_t gce_packed = static_cast<std::uint8_t>(
                ((frame.disposal & 0x07) << 2) | (frame.transparency >= 0 ? 0x01 : 0x00));
            gif.insert(gif.end(), {0x21, 0xf9, 0x04, gce_packed});
            append_le16(gif, static_cast<std::uint16_t>(delay_cs));
            gif.push_back(frame.transparency >= 0 ? static_cast<std::uint8_t>(frame.transparency & 0xff) : 0);
            gif.push_back(0);

            gif.push_back(0x2c);
            append_le16(gif, static_cast<std::uint16_t>(frame.left));
            append_le16(gif, static_cast<std::uint16_t>(frame.top));
            append_le16(gif, static_cast<std::uint16_t>(frame.width));
            append_le16(gif, static_cast<std::uint16_t>(frame.height));
            std::uint8_t image_flags = 0;
            if (frame.include_local_color_table) {
                image_flags = static_cast<std::uint8_t>(
                    0x80 | (gif_table_size_code_for_entries(frame.color_table_entries) & 0x07));
            }
            gif.push_back(image_flags);
            if (frame.include_local_color_table) {
                gif.insert(gif.end(), frame.palette_rgb.begin(), frame.palette_rgb.end());
                gif.resize(gif.size() + static_cast<std::size_t>(frame.color_table_entries) * 3u -
                           frame.palette_rgb.size(), 0);
            }
            gif.push_back(static_cast<std::uint8_t>(frame.min_code_size));

            std::vector<std::uint8_t> lzw;
            if (!gif_lzw_encode_indices(
                    frame.pixels.data(),
                    frame.width,
                    frame.height,
                    static_cast<std::size_t>(frame.width),
                    frame.color_table_entries,
                    frame.min_code_size,
                    &lzw)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            append_gif_sub_blocks(gif, lzw);
        }

        gif.push_back(0x3b);
        if (!write_binary_file(path, gif)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_open_gif(
    const char* path,
    PillowCImage** out_image)
{
    return open_gif_image(path, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_open_gif_frame(
    const char* path,
    int frame_index,
    PillowCImage** out_image)
{
    return open_gif_frame_image(path, frame_index, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_frame_count_gif(
    const char* path,
    int* out_count)
{
    return wic_container_frame_count(path, GUID_ContainerFormatGif, out_count);
}

extern "C" __declspec(dllexport) int pillow_c_image_gif_metadata(
    const char* path,
    int frame_index,
    int* out_duration_ms,
    int* out_loop,
    int* out_disposal,
    int* out_background)
{
    if (!path || !out_duration_ms || !out_loop || !out_disposal || !out_background) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_duration_ms = -1;
    *out_loop = -1;
    *out_disposal = -1;
    *out_background = -1;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        GifMetadata metadata;
        if (!read_gif_metadata(path, frame_index, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_duration_ms = metadata.duration_ms;
        *out_loop = metadata.loop;
        *out_disposal = metadata.disposal;
        *out_background = metadata.background;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_gif_metadata_ex(
    const char* path,
    int frame_index,
    int* out_duration_ms,
    int* out_loop,
    int* out_disposal,
    int* out_background,
    int* out_transparency)
{
    if (!path || !out_duration_ms || !out_loop || !out_disposal || !out_background || !out_transparency) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_duration_ms = -1;
    *out_loop = -1;
    *out_disposal = -1;
    *out_background = -1;
    *out_transparency = -1;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        GifMetadata metadata;
        if (!read_gif_metadata(path, frame_index, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_duration_ms = metadata.duration_ms;
        *out_loop = metadata.loop;
        *out_disposal = metadata.disposal;
        *out_background = metadata.background;
        *out_transparency = metadata.transparency;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_gif_open_info(
    const char* path,
    int frame_index,
    int* out_is_87a,
    int* out_has_extension,
    std::uint8_t* out_label,
    std::size_t out_label_size,
    std::size_t* out_extension_offset)
{
    if (!path || !out_is_87a || !out_has_extension || !out_extension_offset) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_is_87a = 0;
    *out_has_extension = 0;
    *out_extension_offset = 0;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        GifMetadata metadata;
        if (!read_gif_metadata(path, frame_index, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // API-OPENINFO-001: Pillow's version (the GIF87a/GIF89a header) and
        // the extension tuple (label bytes, offset after the label) -- the
        // extension exists on the first frame only.
        *out_is_87a = metadata.is_87a ? 1 : 0;
        if (frame_index == 0 && metadata.has_extension) {
            *out_has_extension = 1;
            if (out_label_size < metadata.extension_label.size()) {
                return PILLOW_C_INVALID_LENGTH;
            }
            if (!metadata.extension_label.empty() && !out_label) {
                return PILLOW_C_NULL_POINTER;
            }
            if (!metadata.extension_label.empty()) {
                std::memcpy(out_label, metadata.extension_label.data(),
                            metadata.extension_label.size());
            }
            *out_extension_offset = metadata.extension_offset;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif(
    const PillowCImage* image,
    const char* path)
{
    return save_gif_image(image, path);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_options(
    const PillowCImage* image,
    const char* path,
    int has_transparency,
    int transparency)
{
    if (!has_transparency) {
        return save_gif_image(image, path);
    }
    return save_gif_indexed_native(image, path, true, transparency, nullptr, 0u);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_interlace_palette_options(
    const PillowCImage* image,
    const char* path,
    int has_transparency,
    int transparency,
    int interlace,
    const std::uint8_t* custom_palette,
    std::size_t custom_palette_size)
{
    if (!image || !path) {
        return PILLOW_C_NULL_POINTER;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    if (image->mode == PILLOW_C_MODE_P && image->channels == 1 &&
        !image->palette_rgb.empty() && image->palette_rgb.size() % 3u == 0u) {
        return save_gif_indexed_native_with_interlace_palette(
            image,
            path,
            has_transparency != 0,
            transparency,
            interlace != 0,
            custom_palette,
            custom_palette_size,
            nullptr,
            0u);
    }
    // Non-indexed sources keep the WIC/native quantization routes; interlace
    // and palette need the native writer, so quantize first.
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(image->width, image->height, 1, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        PillowCImage quantized{
            image->width,
            image->height,
            PILLOW_C_MODE_P,
            1,
            stride,
            std::vector<std::uint8_t>(size)};
        if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
            const int status = pillow_c_quantize_exact_image_into(image, 256, &quantized);
            if (status != PILLOW_C_OK) {
                return status;
            }
        } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
            const int status = pillow_c_quantize_exact_image_into(image, 256, &quantized);
            if (status != PILLOW_C_OK) {
                return status;
            }
        } else if (image->mode == PILLOW_C_MODE_RGBA && image->channels == 4) {
            bool has_transparency_out = false;
            int transparency_out = 0;
            const int status = pillow_c_quantize_exact_rgba_gif_into(
                image, &quantized, &has_transparency_out, &transparency_out);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (has_transparency_out && !has_transparency) {
                has_transparency = 1;
                transparency = transparency_out;
            }
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        return save_gif_indexed_native_with_interlace_palette(
            &quantized,
            path,
            has_transparency != 0,
            transparency,
            interlace != 0,
            custom_palette,
            custom_palette_size,
            nullptr,
            0u);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_gif_comment(
    const char* path,
    int frame_index,
    int* out_has_comment,
    std::uint8_t* out_comment,
    std::size_t out_comment_size,
    std::size_t* out_comment_required)
{
    if (!path || !out_has_comment || !out_comment_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_comment = 0;
    *out_comment_required = 0;
    if (frame_index < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        GifMetadata metadata;
        if (!read_gif_metadata(path, frame_index, &metadata)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::size_t required = metadata.comment.size();
        *out_has_comment = required > 0u ? 1 : 0;
        *out_comment_required = required;
        if (required == 0u) {
            return PILLOW_C_OK;
        }
        if (!out_comment) {
            return PILLOW_C_NULL_POINTER;
        }
        if (out_comment_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out_comment, metadata.comment.data(), required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_comment(
    const PillowCImage* image,
    const char* path,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    return save_gif_image_with_comment(image, path, comment, comment_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_comment_options(
    const PillowCImage* image,
    const char* path,
    int has_transparency,
    int transparency,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    if (has_transparency != 0 && has_transparency != 1) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_gif_image_with_comment_options(
        image,
        path,
        has_transparency != 0,
        transparency,
        comment,
        comment_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        -1,
        -1,
        0,
        0,
        0,
        0,
        nullptr,
        0u);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        0,
        0,
        0,
        0,
        nullptr,
        0u);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_metadata_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        has_transparency,
        transparency,
        0,
        0,
        nullptr,
        0u);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_background_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency,
    int has_background,
    int background)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        has_transparency,
        transparency,
        has_background,
        background,
        nullptr,
        0u);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_comment(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        -1,
        -1,
        0,
        0,
        0,
        0,
        comment,
        comment_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_comment_metadata_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        has_transparency,
        transparency,
        0,
        0,
        comment,
        comment_size);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_gif_animation_comment_background_options(
    const PillowCImage* const* images,
    std::size_t image_count,
    const char* path,
    const int* durations_ms,
    std::size_t duration_count,
    int loop,
    const int* disposals,
    std::size_t disposal_count,
    int include_color_table,
    int optimize,
    int has_transparency,
    int transparency,
    int has_background,
    int background,
    const std::uint8_t* comment,
    std::size_t comment_size)
{
    return save_gif_animation_image(
        images,
        image_count,
        path,
        durations_ms,
        duration_count,
        loop,
        disposals,
        disposal_count,
        include_color_table,
        optimize,
        has_transparency,
        transparency,
        has_background,
        background,
        comment,
        comment_size);
}

