// BEHAV-FONTFILE-001: TrueType/OpenType font loading and the FreeTypeFont
// truetype surface (ImageFont.truetype / load).
//
// Mirrors Pillow 11.3.0's ImageFont.truetype + _imagingft:
// - Table arithmetic is exact: getlength uses the hmtx advances with
//   round-half-away 26.6 scaling and the horizontal kern-format-0 pair table
//   (the RAQM-equivalent default engine, verified byte-exact against the
//   local Pillow 11.3.0 oracle), getmetrics uses the FreeType FT_MulFix/
//   PIXEL(x)=(x+63)>>6 rule on the hhea ascender/descender, and getname reads
//   the sfnt name table (Windows platform, en-US preferred) exactly like
//   FreeType.
// - Glyph metrics and masks are rendered through GDI (AddFontMemResourceEx +
//   GetGlyphOutline GGO_METRICS/GGO_GRAY8_BITMAP). The y-axis metrics match
//   FreeType's bytecode-hinted values exactly; the x-axis bearings/widths and
//   the rasterized pixel values are this runtime's documented divergence
//   (GDI rasterizer instead of FreeType -- the same boundary class as the
//   default-font glyph shapes).
// - Error statuses: -60 cannot open resource, -61 unknown file format,
//   -62 invalid face index, -63 invalid encoding; the facade maps them to
//   Pillow's exact messages.
// - TTC collections extract the selected face into a fresh sfnt buffer (GDI
//   rejects collection files); index beyond the face count keeps Pillow's
//   "invalid argument" error.
// - Layout engine 1 (RAQM default) implements the pinned hb arithmetic; the
//   BASIC engine implements the FreeType unhinted FT_MulFix advance rule
//   (Pillow's hinted BASIC advances are a documented divergence), and
//   direction/features/language are accepted-and-ignored like a raqm-less
//   Pillow build.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "pillow_c_font_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int PILLOW_C_FONT_ERR_CANNOT_OPEN = -60;   // "cannot open resource"
constexpr int PILLOW_C_FONT_ERR_UNKNOWN_FORMAT = -61; // "unknown file format"
constexpr int PILLOW_C_FONT_ERR_BAD_INDEX = -62;      // "invalid argument"
constexpr int PILLOW_C_FONT_ERR_BAD_ENCODING = -63;   // "invalid argument"

std::uint16_t read_u16be(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

std::int16_t read_i16be(const std::uint8_t* p)
{
    return static_cast<std::int16_t>((p[0] << 8) | p[1]);
}

std::uint32_t read_u32be(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24) |
        (static_cast<std::uint32_t>(p[1]) << 16) |
        (static_cast<std::uint32_t>(p[2]) << 8) |
        static_cast<std::uint32_t>(p[3]);
}

std::uint16_t read_u16le(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::int16_t read_i16le(const std::uint8_t* p)
{
    return static_cast<std::int16_t>(p[0] | (p[1] << 8));
}

std::uint32_t read_u32le(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) |
        (static_cast<std::uint32_t>(p[3]) << 24);
}

struct FontReader {
    const std::uint8_t* base;
    std::size_t length;

    bool has(std::size_t offset, std::size_t size) const
    {
        return offset <= length && size <= length - offset;
    }

    std::uint16_t u16be(std::size_t offset) const
    {
        return read_u16be(base + offset);
    }

    std::uint32_t u32be(std::size_t offset) const
    {
        return read_u32be(base + offset);
    }

    std::uint16_t u16le(std::size_t offset) const
    {
        return read_u16le(base + offset);
    }

    std::uint32_t u32le(std::size_t offset) const
    {
        return read_u32le(base + offset);
    }
};

struct FontTableRef {
    std::uint32_t tag;
    std::size_t offset;
    std::size_t length;
};

struct PillowCTtFont {
    std::vector<std::uint8_t> bytes;          // original font file bytes
    std::vector<std::uint8_t> resource_bytes; // bytes registered with GDI (TTC: extracted face)
    double size = 0.0;
    int index = 0;
    std::string encoding;
    int layout_engine = PILLOW_C_FONT_LAYOUT_RAQM;

    std::uint16_t upem = 2048;
    std::int16_t hhea_ascender = 0;
    std::int16_t hhea_descender = 0;
    std::uint16_t glyph_count = 0;
    std::string family;
    std::string style;
    bool has_fvar = false;

    std::unordered_map<std::uint32_t, std::uint16_t> cmap;
    std::vector<std::uint16_t> hmtx;
    std::unordered_map<std::uint32_t, std::int16_t> kern;

    HANDLE font_resource = nullptr;
    HDC hdc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ old_bitmap = nullptr;
    HFONT hfont = nullptr;
    std::wstring family_wide;
};

// FreeType arithmetic. FT_MulFix rounds to nearest with ties toward +inf
// (the 16.16 product plus 0x8000, arithmetic shift), matching the values
// pinned against Pillow 11.3.0.
std::int64_t ft_mul_fix(std::int64_t value, std::int64_t scale_16_16)
{
    return (value * scale_16_16 + 0x8000) >> 16;
}

std::int64_t ft_div_fix(std::int64_t value, std::int64_t divisor)
{
    return (value << 16) / divisor;
}

// The RAQM-equivalent 26.6 advance: round half away from zero on
// units * size26_6 / upem (the exact values Pillow's hb layout produces).
std::int64_t round_half_away(double value)
{
    if (value >= 0.0) {
        return static_cast<std::int64_t>(value + 0.5);
    }
    return static_cast<std::int64_t>(value - 0.5);
}

std::int64_t raqm_scale_26_6(std::int64_t units, std::int64_t size_26_6, std::int64_t upem)
{
    return round_half_away(
        (static_cast<double>(units) * static_cast<double>(size_26_6)) / static_cast<double>(upem));
}

// PIXEL(x) = (x + 63) >> 6 -- Pillow's 26.6-to-pixel conversion.
std::int64_t pixel(std::int64_t value_26_6)
{
    return (value_26_6 + 63) >> 6;
}

std::int64_t pixel_ceil(std::int64_t value_26_6)
{
    return pixel(value_26_6);
}

bool utf8_decode_one(const char*& cursor, const char* end, std::uint32_t& out_codepoint)
{
    if (cursor >= end) {
        return false;
    }
    const auto lead = static_cast<unsigned char>(*cursor);
    if (lead < 0x80) {
        ++cursor;
        out_codepoint = lead;
        return true;
    }
    int extra = 0;
    std::uint32_t value = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1;
        value = lead & 0x1F;
        if (value < 2) { // overlong
            out_codepoint = 0xFFFD;
            ++cursor;
            return true;
        }
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2;
        value = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3;
        value = lead & 0x07;
        if (value > 4) { // above U+10FFFF
            out_codepoint = 0xFFFD;
            ++cursor;
            return true;
        }
    } else {
        out_codepoint = 0xFFFD;
        ++cursor;
        return true;
    }
    ++cursor;
    for (int i = 0; i < extra; ++i) {
        if (cursor >= end || (static_cast<unsigned char>(*cursor) & 0xC0) != 0x80) {
            out_codepoint = 0xFFFD;
            return true;
        }
        value = (value << 6) | (static_cast<unsigned char>(*cursor) & 0x3F);
        ++cursor;
    }
    if ((extra == 2 && value < 0x800) || (extra == 3 && value < 0x10000)) { // overlong
        out_codepoint = 0xFFFD;
        return true;
    }
    if (value >= 0xD800 && value <= 0xDFFF) { // surrogate
        out_codepoint = 0xFFFD;
        return true;
    }
    if (value > 0x10FFFF) {
        out_codepoint = 0xFFFD;
        return true;
    }
    out_codepoint = value;
    return true;
}

std::wstring utf8_to_wide(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (needed <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], needed);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

// --- sfnt parsing --------------------------------------------------------

// Returns PILLOW_C_OK on success, PILLOW_C_FONT_ERR_BAD_INDEX when the face
// index is out of range, or PILLOW_C_FONT_ERR_UNKNOWN_FORMAT for a malformed
// container. On success, TTC collections get their selected face extracted
// into `out_extracted` (GDI rejects collection files).
int parse_sfnt_tables(
    const FontReader& reader,
    int index,
    std::vector<FontTableRef>* out_tables,
    std::vector<std::uint8_t>* out_extracted)
{
    out_tables->clear();
    out_extracted->clear();
    if (!reader.has(0, 12)) {
        return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
    }
    const std::uint32_t magic = reader.u32be(0);
    std::size_t directory_offset = 0;
    bool is_ttc = false;
    if (magic == 0x74746366u) { // 'ttcf'
        is_ttc = true;
        const std::uint32_t font_count = reader.u32be(8);
        if (font_count == 0) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        if (index < 0 || static_cast<std::uint32_t>(index) >= font_count) {
            return PILLOW_C_FONT_ERR_BAD_INDEX;
        }
        const std::size_t offsets_offset = 12;
        if (!reader.has(offsets_offset, static_cast<std::size_t>(index + 1) * 4u)) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        directory_offset = reader.u32be(offsets_offset + static_cast<std::size_t>(index) * 4u);
        if (!reader.has(directory_offset, 12)) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
    } else {
        if (index != 0) {
            return PILLOW_C_FONT_ERR_BAD_INDEX; // single-face font with a nonzero index
        }
        directory_offset = 0;
    }

    const std::uint16_t table_count = reader.u16be(directory_offset + 4);
    if (table_count == 0) {
        return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
    }
    const std::size_t records_offset = directory_offset + 12;
    if (!reader.has(records_offset, static_cast<std::size_t>(table_count) * 16u)) {
        return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
    }
    std::unordered_map<std::uint32_t, FontTableRef> by_tag;
    for (std::size_t i = 0; i < table_count; ++i) {
        const std::size_t record = records_offset + i * 16u;
        FontTableRef ref;
        ref.tag = reader.u32be(record);
        ref.offset = reader.u32be(record + 8);
        ref.length = reader.u32be(record + 12);
        if (!reader.has(ref.offset, ref.length)) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        by_tag[ref.tag] = ref;
    }

    for (const std::uint32_t tag : {
             static_cast<std::uint32_t>(0x68656164u), // 'head'
             static_cast<std::uint32_t>(0x68686561u), // 'hhea'
             static_cast<std::uint32_t>(0x6D617870u), // 'maxp'
             static_cast<std::uint32_t>(0x686D7478u), // 'hmtx'
             static_cast<std::uint32_t>(0x636D6170u), // 'cmap'
             static_cast<std::uint32_t>(0x6E616D65u), // 'name'
         }) {
        if (by_tag.find(tag) == by_tag.end()) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
    }
    for (const auto& entry : by_tag) {
        out_tables->push_back(entry.second);
    }

    if (!is_ttc) {
        return PILLOW_C_OK;
    }

    // Extract the selected face into a standalone sfnt buffer (GDI rejects
    // TTC collections passed to AddFontMemResourceEx).
    std::size_t data_offset = 12 + 16u * out_tables->size();
    std::size_t total = data_offset;
    for (const auto& table : *out_tables) {
        total += (table.length + 3u) & ~std::size_t{3u};
    }
    out_extracted->assign(total, 0);
    std::uint8_t* out = out_extracted->data();
    out[0] = 0;
    out[1] = 1;
    out[2] = 0;
    out[3] = 0;
    // numTables
    out[4] = static_cast<std::uint8_t>((out_tables->size() >> 8) & 0xFF);
    out[5] = static_cast<std::uint8_t>(out_tables->size() & 0xFF);
    const std::uint16_t count = static_cast<std::uint16_t>(out_tables->size());
    std::uint16_t max_power = 1;
    std::uint16_t selector = 0;
    while (max_power * 2u <= count) {
        max_power *= 2u;
        ++selector;
    }
    const std::uint16_t search_range = static_cast<std::uint16_t>(max_power * 16u);
    const std::uint16_t range_shift = static_cast<std::uint16_t>(count * 16u - search_range);
    out[6] = static_cast<std::uint8_t>((search_range >> 8) & 0xFF);
    out[7] = static_cast<std::uint8_t>(search_range & 0xFF);
    out[8] = static_cast<std::uint8_t>((selector >> 8) & 0xFF);
    out[9] = static_cast<std::uint8_t>(selector & 0xFF);
    out[10] = static_cast<std::uint8_t>((range_shift >> 8) & 0xFF);
    out[11] = static_cast<std::uint8_t>(range_shift & 0xFF);

    std::size_t cursor = data_offset;
    for (std::size_t i = 0; i < out_tables->size(); ++i) {
        const FontTableRef& table = (*out_tables)[i];
        std::uint8_t* record = out + 12 + i * 16u;
        record[0] = static_cast<std::uint8_t>((table.tag >> 24) & 0xFF);
        record[1] = static_cast<std::uint8_t>((table.tag >> 16) & 0xFF);
        record[2] = static_cast<std::uint8_t>((table.tag >> 8) & 0xFF);
        record[3] = static_cast<std::uint8_t>(table.tag & 0xFF);
        // checksum zeroed -- GDI tolerates it
        record[8] = static_cast<std::uint8_t>((cursor >> 24) & 0xFF);
        record[9] = static_cast<std::uint8_t>((cursor >> 16) & 0xFF);
        record[10] = static_cast<std::uint8_t>((cursor >> 8) & 0xFF);
        record[11] = static_cast<std::uint8_t>(cursor & 0xFF);
        record[12] = static_cast<std::uint8_t>((table.length >> 24) & 0xFF);
        record[13] = static_cast<std::uint8_t>((table.length >> 16) & 0xFF);
        record[14] = static_cast<std::uint8_t>((table.length >> 8) & 0xFF);
        record[15] = static_cast<std::uint8_t>(table.length & 0xFF);
        std::memcpy(out + cursor, reader.base + table.offset, table.length);
        cursor += (table.length + 3u) & ~std::size_t{3u};
    }
    return PILLOW_C_OK;
}

const FontTableRef* find_table(const std::vector<FontTableRef>& tables, std::uint32_t tag)
{
    for (const auto& table : tables) {
        if (table.tag == tag) {
            return &table;
        }
    }
    return nullptr;
}

bool parse_cmap_format4(
    const std::uint8_t* base,
    std::size_t length,
    std::unordered_map<std::uint32_t, std::uint16_t>* out)
{
    if (length < 16) {
        return false;
    }
    const std::size_t seg_count_x2 = read_u16be(base + 6);
    if (seg_count_x2 == 0 || (seg_count_x2 & 1) != 0) {
        return false;
    }
    const std::size_t seg_count = seg_count_x2 / 2;
    if (length < 16u + seg_count_x2 * 4u) {
        return false;
    }
    const std::size_t end_codes = 14;
    const std::size_t start_codes = end_codes + seg_count_x2 + 2;
    const std::size_t id_deltas = start_codes + seg_count_x2;
    const std::size_t id_range_offsets = id_deltas + seg_count_x2;
    const std::size_t glyph_array = id_range_offsets + seg_count_x2;
    if (glyph_array > length) {
        return false;
    }
    for (std::size_t seg = 0; seg < seg_count; ++seg) {
        const std::uint16_t end_code = read_u16be(base + end_codes + seg * 2);
        const std::uint16_t start_code = read_u16be(base + start_codes + seg * 2);
        if (start_code == 0xFFFF) {
            break;
        }
        const std::uint16_t range_offset = read_u16be(base + id_range_offsets + seg * 2);
        const std::int32_t delta = static_cast<std::int16_t>(read_u16be(base + id_deltas + seg * 2));
        const std::uint32_t limit = std::min<std::uint32_t>(end_code, 0xFFFE);
        for (std::uint32_t cp = start_code; cp <= limit; ++cp) {
            std::uint16_t glyph = 0;
            if (range_offset == 0) {
                glyph = static_cast<std::uint16_t>((static_cast<std::int32_t>(cp) + delta) & 0xFFFF);
            } else {
                // Per the OpenType spec the offset is measured from the
                // idRangeOffset entry itself.
                const std::size_t address =
                    id_range_offsets + seg * 2 + range_offset + (cp - start_code) * 2u;
                if (address + 2 > length) {
                    return false;
                }
                glyph = read_u16be(base + address);
                if (glyph != 0) {
                    glyph = static_cast<std::uint16_t>((static_cast<std::int32_t>(glyph) + delta) & 0xFFFF);
                }
            }
            if (glyph != 0) {
                (*out)[cp] = glyph;
            }
        }
    }
    return true;
}

bool parse_cmap_format12(
    const std::uint8_t* base,
    std::size_t length,
    std::unordered_map<std::uint32_t, std::uint16_t>* out)
{
    if (length < 16) {
        return false;
    }
    const std::uint32_t group_count = read_u32be(base + 12);
    if (length < 16u + static_cast<std::size_t>(group_count) * 12u) {
        return false;
    }
    for (std::uint32_t i = 0; i < group_count; ++i) {
        const std::size_t record = 16 + static_cast<std::size_t>(i) * 12u;
        const std::uint32_t start_code = read_u32be(base + record);
        const std::uint32_t end_code = read_u32be(base + record + 4);
        const std::uint32_t start_glyph = read_u32be(base + record + 8);
        if (start_code > end_code) {
            return false;
        }
        for (std::uint32_t cp = start_code; cp <= end_code && cp <= 0x10FFFF; ++cp) {
            const std::uint64_t glyph =
                static_cast<std::uint64_t>(start_glyph) + (cp - start_code);
            if (glyph > 0 && glyph <= 0xFFFF) {
                (*out)[cp] = static_cast<std::uint16_t>(glyph);
            }
        }
    }
    return true;
}

bool parse_cmap_table(
    const FontReader& reader,
    const FontTableRef& table,
    std::unordered_map<std::uint32_t, std::uint16_t>* out)
{
    out->clear();
    if (table.length < 4) {
        return false;
    }
    const std::size_t base_offset = table.offset;
    const std::uint16_t subtable_count = reader.u16be(base_offset + 2);
    if (table.length < 4u + static_cast<std::size_t>(subtable_count) * 8u) {
        return false;
    }
    // Preferred subtable order: (3,1) format 4, (3,10) format 12, (0,x)
    // format 4, then any format 4/12.
    struct Candidate {
        int priority;
        std::size_t offset;
    };
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < subtable_count; ++i) {
        const std::size_t record = base_offset + 4 + i * 8u;
        const std::uint16_t platform = reader.u16be(record);
        const std::uint16_t encoding = reader.u16be(record + 2);
        const std::size_t subtable = base_offset + reader.u32be(record + 4);
        if (!reader.has(subtable, 2)) {
            continue;
        }
        const std::uint16_t format = reader.u16be(subtable);
        int priority = 9;
        if (format == 4) {
            if (platform == 3 && encoding == 1) {
                priority = 0;
            } else if (platform == 0) {
                priority = 2;
            } else if (platform == 3 && encoding == 0) {
                priority = 3;
            } else {
                priority = 5;
            }
        } else if (format == 12) {
            if (platform == 3 && encoding == 10) {
                priority = 1;
            } else {
                priority = 4;
            }
        }
        if (priority < 9) {
            candidates.push_back(Candidate{priority, subtable});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.priority < b.priority;
    });
    for (const Candidate& candidate : candidates) {
        if (!reader.has(candidate.offset, 2)) {
            continue;
        }
        const std::uint16_t format = reader.u16be(candidate.offset);
        const std::size_t subtable_length = base_offset + table.length - candidate.offset;
        bool parsed = false;
        if (format == 4) {
            parsed = parse_cmap_format4(reader.base + candidate.offset, subtable_length, out);
        } else if (format == 12) {
            parsed = parse_cmap_format12(reader.base + candidate.offset, subtable_length, out);
        }
        if (parsed && !out->empty()) {
            return true;
        }
        out->clear();
    }
    return false;
}

bool parse_name_table(
    const FontReader& reader,
    const FontTableRef& table,
    std::string* out_family,
    std::string* out_style)
{
    if (table.length < 6) {
        return false;
    }
    const std::size_t base = table.offset;
    const std::uint16_t name_count = reader.u16be(base + 2);
    const std::size_t storage = base + reader.u16be(base + 4);
    if (storage > base + table.length) {
        return false;
    }
    const std::size_t records = base + 6;
    if (!reader.has(records, static_cast<std::size_t>(name_count) * 12u)) {
        return false;
    }
    auto decode = [&](std::size_t record, bool wide) -> std::string {
        const std::size_t offset = storage + reader.u16be(record + 10);
        const std::size_t length = reader.u16be(record + 8);
        if (!reader.has(offset, length)) {
            return std::string();
        }
        if (!wide) {
            return std::string(reinterpret_cast<const char*>(reader.base + offset), length);
        }
        if ((length & 1) != 0) {
            return std::string();
        }
        std::string result;
        result.reserve(length / 2);
        for (std::size_t i = 0; i + 1 < length; i += 2) {
            const std::uint32_t cp = read_u16be(reader.base + offset + i);
            // UTF-16BE -> UTF-8 for BMP code points.
            if (cp < 0x80) {
                result.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        return result;
    };
    auto pick = [&](std::uint16_t name_id) -> std::string {
        // FreeType-style preference: Windows (3,1,en-US), Windows (3,1,any),
        // then Apple Roman (1,0,0), then the first record.
        std::size_t first_record = std::numeric_limits<std::size_t>::max();
        std::size_t windows_any = std::numeric_limits<std::size_t>::max();
        std::size_t windows_en = std::numeric_limits<std::size_t>::max();
        std::size_t apple_roman = std::numeric_limits<std::size_t>::max();
        for (std::size_t i = 0; i < name_count; ++i) {
            const std::size_t record = records + i * 12u;
            if (reader.u16be(record + 6) != name_id) {
                continue;
            }
            if (first_record == std::numeric_limits<std::size_t>::max()) {
                first_record = i;
            }
            const std::uint16_t platform = reader.u16be(record);
            const std::uint16_t encoding = reader.u16be(record + 2);
            const std::uint16_t language = reader.u16be(record + 4);
            if (platform == 3 && encoding == 1) {
                if (language == 0x409 && windows_en == std::numeric_limits<std::size_t>::max()) {
                    windows_en = i;
                }
                if (windows_any == std::numeric_limits<std::size_t>::max()) {
                    windows_any = i;
                }
            } else if (platform == 1 && encoding == 0 && language == 0 &&
                       apple_roman == std::numeric_limits<std::size_t>::max()) {
                apple_roman = i;
            }
        }
        const std::size_t chosen = windows_en != std::numeric_limits<std::size_t>::max()
            ? windows_en
            : (windows_any != std::numeric_limits<std::size_t>::max()
                   ? windows_any
                   : (apple_roman != std::numeric_limits<std::size_t>::max()
                          ? apple_roman
                          : first_record));
        if (chosen == std::numeric_limits<std::size_t>::max()) {
            return std::string();
        }
        const std::size_t record = records + chosen * 12u;
        const std::uint16_t platform = reader.u16be(record);
        return decode(record, platform != 1);
    };
    *out_family = pick(1);
    *out_style = pick(2);
    return !out_family->empty();
}

bool parse_kern_table(
    const FontReader& reader,
    const FontTableRef& table,
    std::unordered_map<std::uint32_t, std::int16_t>* out)
{
    out->clear();
    if (table.length < 4) {
        return true; // an empty/broken kern table means "no kerning"
    }
    const std::size_t base = table.offset;
    std::size_t cursor = base;
    const std::uint16_t version = reader.u16be(cursor);
    cursor += 2;
    std::uint32_t subtable_count = 0;
    if (version == 0) {
        if (!reader.has(cursor, 2)) {
            return true;
        }
        subtable_count = reader.u16be(cursor);
        cursor += 2;
    } else {
        // Apple version 1: u32 version, u32 nTables.
        if (!reader.has(cursor, 8)) {
            return true;
        }
        cursor += 4; // skip the real version field
        subtable_count = reader.u32be(cursor);
        cursor += 4;
    }
    for (std::uint32_t i = 0; i < subtable_count; ++i) {
        if (!reader.has(cursor, 6)) {
            break;
        }
        const std::uint16_t sub_version = reader.u16be(cursor);
        const std::uint16_t sub_length = reader.u16be(cursor + 2);
        const std::uint16_t coverage = reader.u16be(cursor + 4);
        const std::uint16_t format = static_cast<std::uint16_t>(coverage >> 8);
        if (sub_length < 6 || !reader.has(cursor, sub_length)) {
            break;
        }
        const bool horizontal = (coverage & 0x0001) != 0;
        const bool cross_stream = (coverage & 0x0004) != 0;
        if (sub_version == 0 && format == 0 && horizontal && !cross_stream && sub_length >= 14) {
            const std::uint16_t pair_count = reader.u16be(cursor + 6);
            const std::size_t pairs = cursor + 14;
            if (reader.has(pairs, static_cast<std::size_t>(pair_count) * 6u)) {
                for (std::uint16_t p = 0; p < pair_count; ++p) {
                    const std::size_t record = pairs + static_cast<std::size_t>(p) * 6u;
                    const std::uint16_t left = reader.u16be(record);
                    const std::uint16_t right = reader.u16be(record + 2);
                    const std::int16_t value =
                        static_cast<std::int16_t>(reader.u16be(record + 4));
                    const std::uint32_t key = (static_cast<std::uint32_t>(left) << 16) | right;
                    (*out)[key] = value;
                }
            }
        }
        cursor += sub_length;
    }
    return true;
}

// --- GDI glyph metrics ---------------------------------------------------

struct GdiGlyphMetrics {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int32_t origin_x = 0; // px, grayscale-grid-fitted integer origin
    std::int32_t origin_y = 0;
};

// --- the shared glyph-run walk -------------------------------------------

struct GlyphRunEntry {
    std::uint16_t glyph;
    std::int64_t x_offset_26_6; // pen position before the glyph
};

bool build_glyph_run(
    const PillowCTtFont& font,
    const char* text,
    std::vector<GlyphRunEntry>* out_run,
    std::int64_t* out_final_pen_26_6,
    std::int64_t* out_total_26_6)
{
    out_run->clear();
    const std::size_t text_length = std::strlen(text);
    const char* cursor = text;
    const char* end = text + text_length;
    const std::int64_t size_26_6 = static_cast<std::int64_t>(font.size * 64.0);
    const std::int64_t scale_16_16 = ft_div_fix(size_26_6, font.upem);

    std::int64_t pen = 0;
    std::int64_t total = 0;
    std::uint16_t previous = 0;
    bool has_previous = false;
    while (cursor < end) {
        std::uint32_t codepoint = 0;
        utf8_decode_one(cursor, end, codepoint);
        std::uint16_t glyph = 0;
        const auto found = font.cmap.find(codepoint);
        if (found != font.cmap.end() && found->second < font.glyph_count) {
            glyph = found->second;
        }
        const std::uint16_t advance_units =
            glyph < font.hmtx.size() ? font.hmtx[glyph] : (font.hmtx.empty() ? 0 : font.hmtx.back());
        std::int64_t advance_26_6 = 0;
        if (font.layout_engine == PILLOW_C_FONT_LAYOUT_BASIC) {
            advance_26_6 = ft_mul_fix(advance_units, scale_16_16);
        } else {
            advance_26_6 = raqm_scale_26_6(advance_units, size_26_6, font.upem);
        }
        if (has_previous) {
            const std::uint32_t key = (static_cast<std::uint32_t>(previous) << 16) | glyph;
            const auto pair = font.kern.find(key);
            if (pair != font.kern.end() && font.layout_engine == PILLOW_C_FONT_LAYOUT_RAQM) {
                const std::int64_t kern_26_6 = raqm_scale_26_6(pair->second, size_26_6, font.upem);
                pen += kern_26_6;
                total += kern_26_6;
            }
        }
        out_run->push_back(GlyphRunEntry{glyph, pen});
        pen += advance_26_6;
        total += advance_26_6;
        previous = glyph;
        has_previous = true;
    }
    *out_final_pen_26_6 = pen;
    *out_total_26_6 = total;
    return true;
}

struct RunExtents {
    int width = 0;
    int height = 0;
    int offset_x = 0;
    int offset_y = 0;
    bool has_ink = false;
};

// Renders one glyph to an 8-bit gray buffer (GGO_GRAY8_BITMAP, 65 levels,
// DWORD-padded rows) and reports whether it produced any ink. The metrics
// returned by the gray8 call (grayscale-grid-fitted black box and origin)
// are the authoritative placement for both extents and blitting; GDI reports
// a 1x1 black box for outline-less glyphs such as space, so emptiness is
// decided from the rendered pixels, matching FreeType's no-outline glyphs.
bool render_glyph_gray8(
    const PillowCTtFont& font,
    std::uint16_t glyph,
    GdiGlyphMetrics* out_metrics,
    std::vector<std::uint8_t>* out_buffer)
{
    MAT2 identity{};
    identity.eM11.value = 1;
    identity.eM12.value = 0;
    identity.eM21.value = 0;
    identity.eM22.value = 1;
    GLYPHMETRICS metrics{};
    const std::uint32_t needed = GetGlyphOutlineW(
        font.hdc,
        glyph,
        GGO_GRAY8_BITMAP | GGO_GLYPH_INDEX,
        &metrics,
        0,
        nullptr,
        &identity);
    if (needed == GDI_ERROR || needed == 0 || metrics.gmBlackBoxX == 0 || metrics.gmBlackBoxY == 0) {
        return false;
    }
    out_buffer->assign(needed, 0);
    if (GetGlyphOutlineW(
            font.hdc,
            glyph,
            GGO_GRAY8_BITMAP | GGO_GLYPH_INDEX,
            &metrics,
            needed,
            out_buffer->data(),
            &identity) == GDI_ERROR) {
        return false;
    }
    bool any_ink = false;
    for (const std::uint8_t level : *out_buffer) {
        if (level != 0) {
            any_ink = true;
            break;
        }
    }
    if (!any_ink) {
        return false;
    }
    out_metrics->width = metrics.gmBlackBoxX;
    out_metrics->height = metrics.gmBlackBoxY;
    out_metrics->origin_x = metrics.gmptGlyphOrigin.x;
    out_metrics->origin_y = metrics.gmptGlyphOrigin.y;
    return true;
}

// The pinned Pillow getsize assembly: per-glyph ink extents accumulate in
// 26.6 with the pen tracked across empty glyphs, width = (x_max - x_min) >> 6,
// offset.x = PIXEL(x_min), height = (y_top - y_bottom) >> 6,
// offset.y = ascent - PIXEL(y_top).
int compute_run_extents(
    const PillowCTtFont& font,
    const std::vector<GlyphRunEntry>& run,
    std::int64_t final_pen_26_6,
    int ascent_px,
    RunExtents* out)
{
    std::int64_t x_min = 0;
    std::int64_t x_max = 0;
    std::int64_t y_top = 0;
    std::int64_t y_bottom = 0;
    bool has_ink = false;
    std::vector<std::uint8_t> buffer;
    for (const GlyphRunEntry& entry : run) {
        GdiGlyphMetrics metrics{};
        if (!render_glyph_gray8(font, entry.glyph, &metrics, &buffer)) {
            continue;
        }
        const std::int64_t bearing_x_26_6 =
            entry.x_offset_26_6 + static_cast<std::int64_t>(metrics.origin_x) * 64;
        const std::int64_t width_26_6 = static_cast<std::int64_t>(metrics.width) * 64;
        const std::int64_t top_26_6 = static_cast<std::int64_t>(metrics.origin_y) * 64;
        if (!has_ink) {
            x_min = std::min(x_min, bearing_x_26_6);
            x_max = std::max(x_max, bearing_x_26_6 + width_26_6);
            y_top = top_26_6;
            y_bottom = top_26_6 - static_cast<std::int64_t>(metrics.height) * 64;
            has_ink = true;
        } else {
            x_min = std::min(x_min, bearing_x_26_6);
            x_max = std::max(x_max, bearing_x_26_6 + width_26_6);
            y_top = std::max(y_top, top_26_6);
            y_bottom =
                std::min(y_bottom, top_26_6 - static_cast<std::int64_t>(metrics.height) * 64);
        }
    }
    out->has_ink = has_ink;
    if (has_ink) {
        x_max = std::max(x_max, final_pen_26_6);
        out->width = static_cast<int>((x_max - x_min) >> 6);
        out->offset_x = static_cast<int>((x_min + 63) >> 6);
        out->height = static_cast<int>((y_top - y_bottom) >> 6);
        out->offset_y = ascent_px - static_cast<int>((y_top + 63) >> 6);
    } else {
        out->width = static_cast<int>(final_pen_26_6 >> 6);
        out->offset_x = 0;
        out->height = 0;
        out->offset_y = ascent_px;
    }
    return PILLOW_C_OK;
}

int font_tt_ascent_px(const PillowCTtFont& font, int* out_ascent, int* out_descent)
{
    const std::int64_t size_26_6 = static_cast<std::int64_t>(font.size * 64.0);
    const std::int64_t scale_16_16 = ft_div_fix(size_26_6, font.upem);
    const std::int64_t ascender_26_6 = ft_mul_fix(font.hhea_ascender, scale_16_16);
    const std::int64_t descender_26_6 = ft_mul_fix(font.hhea_descender, scale_16_16);
    *out_ascent = static_cast<int>(pixel_ceil(ascender_26_6));
    *out_descent = static_cast<int>(pixel_ceil(-descender_26_6));
    return PILLOW_C_OK;
}

// --- mask rendering ------------------------------------------------------

int render_glyph_run_mask(
    const PillowCTtFont& font,
    const std::vector<GlyphRunEntry>& run,
    const RunExtents& extents,
    int ascent_px,
    bool rgba,
    int ink,
    PillowCImage** out_image)
{
    const int channels = rgba ? 4 : 1;
    const std::size_t pixel_count =
        static_cast<std::size_t>(std::max(extents.width, 0)) * static_cast<std::size_t>(std::max(extents.height, 0));
    try {
        auto* image = new PillowCImage{
            extents.width,
            extents.height,
            rgba ? PILLOW_C_MODE_RGBA : PILLOW_C_MODE_L,
            channels,
            static_cast<std::size_t>(std::max(extents.width, 0)),
            std::vector<std::uint8_t>(pixel_count * static_cast<std::size_t>(channels), 0)};
        *out_image = image;

        std::vector<std::uint8_t> buffer;
        for (const GlyphRunEntry& entry : run) {
            GdiGlyphMetrics metrics{};
            if (!render_glyph_gray8(font, entry.glyph, &metrics, &buffer)) {
                continue;
            }
            const std::size_t stride = (metrics.width + 3u) & ~std::size_t{3u};
            const int render_x =
                static_cast<int>(entry.x_offset_26_6 >> 6) + metrics.origin_x - extents.offset_x;
            const int render_y =
                ascent_px - metrics.origin_y - extents.offset_y;
            for (std::uint32_t row = 0; row < metrics.height; ++row) {
                for (std::uint32_t col = 0; col < metrics.width; ++col) {
                    const std::uint8_t level = buffer[row * stride + col];
                    if (level == 0) {
                        continue;
                    }
                    const int px = render_x + static_cast<int>(col);
                    const int py = render_y + static_cast<int>(row);
                    if (px < 0 || py < 0 || px >= extents.width || py >= extents.height) {
                        continue;
                    }
                    const std::size_t pixel_index =
                        (static_cast<std::size_t>(py) * static_cast<std::size_t>(extents.width) +
                         static_cast<std::size_t>(px)) *
                        static_cast<std::size_t>(channels);
                    const std::uint8_t value = static_cast<std::uint8_t>(std::min<int>(255, level * 4));
                    if (!rgba) {
                        std::uint8_t& current = image->pixels[pixel_index];
                        current = std::max(current, value);
                    } else {
                        const std::uint8_t alpha =
                            static_cast<std::uint8_t>((value * std::max(0, std::min(255, ink))) / 255);
                        if (alpha == 0) {
                            continue;
                        }
                        const std::uint8_t ink_byte = static_cast<std::uint8_t>(std::max(0, std::min(255, ink)));
                        std::uint8_t& r = image->pixels[pixel_index];
                        std::uint8_t& g = image->pixels[pixel_index + 1];
                        std::uint8_t& b = image->pixels[pixel_index + 2];
                        std::uint8_t& a = image->pixels[pixel_index + 3];
                        r = std::max(r, ink_byte);
                        g = std::max(g, ink_byte);
                        b = std::max(b, ink_byte);
                        a = std::max(a, alpha);
                    }
                }
            }
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// --- the public seams ----------------------------------------------------

int font_tt_load_common(
    const std::uint8_t* data,
    std::size_t length,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font)
{
    if (!out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_font = nullptr;
    if (!data || length < 4) {
        return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
    }
    if (size <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (encoding && encoding[0] != '\0' && std::strcmp(encoding, "unic") != 0) {
        return PILLOW_C_FONT_ERR_BAD_ENCODING;
    }

    FontReader reader{data, length};
    std::vector<FontTableRef> tables;
    std::vector<std::uint8_t> extracted;
    const int parse_status = parse_sfnt_tables(reader, index, &tables, &extracted);
    if (parse_status != PILLOW_C_OK) {
        return parse_status;
    }

    try {
        std::unique_ptr<PillowCTtFont> font(new PillowCTtFont());
        font->size = size;
        font->index = index;
        font->encoding = encoding ? encoding : "";
        font->layout_engine =
            (layout_engine == PILLOW_C_FONT_LAYOUT_BASIC) ? PILLOW_C_FONT_LAYOUT_BASIC : PILLOW_C_FONT_LAYOUT_RAQM;
        font->bytes.assign(data, data + length);
        font->resource_bytes = extracted.empty() ? font->bytes : std::move(extracted);

        const FontTableRef* head = find_table(tables, 0x68656164u);
        const FontTableRef* hhea = find_table(tables, 0x68686561u);
        const FontTableRef* maxp = find_table(tables, 0x6D617870u);
        const FontTableRef* hmtx = find_table(tables, 0x686D7478u);
        const FontTableRef* cmap_table = find_table(tables, 0x636D6170u);
        const FontTableRef* name_table = find_table(tables, 0x6E616D65u);
        const FontTableRef* kern_table = find_table(tables, 0x6B65726Eu); // 'kern'
        const FontTableRef* fvar = find_table(tables, 0x66766172u);       // 'fvar'
        if (!head || !hhea || !maxp || !hmtx || !cmap_table || !name_table ||
            head->length < 54 || hhea->length < 36 || maxp->length < 6 || hmtx->length < 4) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }

        font->upem = reader.u16be(head->offset + 18);
        if (font->upem == 0) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        font->hhea_ascender = static_cast<std::int16_t>(reader.u16be(hhea->offset + 4));
        font->hhea_descender = static_cast<std::int16_t>(reader.u16be(hhea->offset + 6));
        const std::uint16_t hmtx_count = reader.u16be(hhea->offset + 34);
        font->glyph_count = reader.u16be(maxp->offset + 4);
        if (font->glyph_count == 0) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        if (!reader.has(hmtx->offset, static_cast<std::size_t>(hmtx_count) * 4u)) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        font->hmtx.reserve(font->glyph_count);
        for (std::uint16_t i = 0; i < hmtx_count; ++i) {
            font->hmtx.push_back(reader.u16be(hmtx->offset + static_cast<std::size_t>(i) * 4u));
        }
        const std::uint16_t last_advance = font->hmtx.empty() ? 0 : font->hmtx.back();
        while (font->hmtx.size() < font->glyph_count) {
            font->hmtx.push_back(last_advance);
        }
        if (!parse_cmap_table(reader, *cmap_table, &font->cmap)) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        if (!parse_name_table(reader, *name_table, &font->family, &font->style)) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        if (kern_table) {
            parse_kern_table(reader, *kern_table, &font->kern);
        }
        font->has_fvar = fvar != nullptr;

        // GDI registration. TTC faces use the extracted standalone sfnt;
        // AddFontMemResourceEx does not copy, so resource_bytes stays alive
        // inside the font object.
        DWORD registered = 0;
        font->font_resource = AddFontMemResourceEx(
            font->resource_bytes.data(),
            static_cast<DWORD>(font->resource_bytes.size()),
            nullptr,
            &registered);
        if (!font->font_resource) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        font->family_wide = utf8_to_wide(font->family);
        if (font->family_wide.empty()) {
            font->family_wide = L"Arial";
        }
        font->hfont = CreateFontW(
            -static_cast<int>(std::max<long>(1, std::lround(size))),
            0,
            0,
            0,
            FW_DONTCARE,
            0,
            0,
            0,
            DEFAULT_CHARSET,
            OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            font->family_wide.c_str());
        if (!font->hfont) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        font->hdc = CreateCompatibleDC(nullptr);
        if (!font->hdc) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        font->bitmap = CreateCompatibleBitmap(font->hdc, 16, 16);
        if (!font->bitmap) {
            return PILLOW_C_FONT_ERR_UNKNOWN_FORMAT;
        }
        font->old_bitmap = SelectObject(font->hdc, font->bitmap);
        SelectObject(font->hdc, font->hfont);

        auto* handle = new PillowCFont{PILLOW_C_FONT_TRUETYPE, font.release()};
        *out_font = handle;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

int font_tt_load_bytes(
    const std::uint8_t* data,
    std::size_t length,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font)
{
    return font_tt_load_common(data, length, size, index, encoding, layout_engine, out_font);
}

int font_tt_load_file(
    const char* path,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font)
{
    if (!out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_font = nullptr;
    if (!path || !path[0]) {
        return PILLOW_C_FONT_ERR_CANNOT_OPEN;
    }
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return PILLOW_C_FONT_ERR_CANNOT_OPEN;
    }
    LARGE_INTEGER file_size{};
    std::vector<std::uint8_t> data;
    int status = PILLOW_C_FONT_ERR_CANNOT_OPEN;
    if (GetFileSizeEx(file, &file_size) && file_size.QuadPart > 0 &&
        file_size.QuadPart <= 256 * 1024 * 1024) {
        try {
            data.assign(static_cast<std::size_t>(file_size.QuadPart), 0);
            DWORD read = 0;
            if (ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr) &&
                read == data.size()) {
                status = font_tt_load_common(
                    data.data(), data.size(), size, index, encoding, layout_engine, out_font);
            }
        } catch (const std::bad_alloc&) {
            status = PILLOW_C_ALLOCATION_FAILED;
        }
    }
    CloseHandle(file);
    return status;
}

int font_tt_free(PillowCFont* font)
{
    if (!font) {
        return PILLOW_C_OK;
    }
    auto* tt = static_cast<PillowCTtFont*>(font->truetype);
    if (tt) {
        if (tt->hdc) {
            if (tt->old_bitmap) {
                SelectObject(tt->hdc, tt->old_bitmap);
            }
            DeleteDC(tt->hdc);
        }
        if (tt->bitmap) {
            DeleteObject(tt->bitmap);
        }
        if (tt->hfont) {
            DeleteObject(tt->hfont);
        }
        if (tt->font_resource) {
            RemoveFontMemResourceEx(tt->font_resource);
        }
        delete tt;
    }
    delete font;
    return PILLOW_C_OK;
}

int font_tt_variant(const PillowCFont* font, PillowCFont** out_font)
{
    if (!font || !out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_font = nullptr;
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return font_tt_load_common(
        tt->bytes.data(),
        tt->bytes.size(),
        tt->size,
        tt->index,
        tt->encoding.c_str(),
        tt->layout_engine,
        out_font);
}

int font_tt_is_variable(const PillowCFont* font, int* out_variable)
{
    if (!font || !out_variable) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_variable = tt->has_fvar ? 1 : 0;
    return PILLOW_C_OK;
}

int font_tt_getlength(const PillowCFont* font, const char* text, double* out_length)
{
    if (!font || !text || !out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<GlyphRunEntry> run;
    std::int64_t final_pen = 0;
    std::int64_t total = 0;
    build_glyph_run(*tt, text, &run, &final_pen, &total);
    *out_length = static_cast<double>(total) / 64.0;
    return PILLOW_C_OK;
}

int font_tt_getmetrics(const PillowCFont* font, int* out_ascent, int* out_descent)
{
    if (!font || !out_ascent || !out_descent) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return font_tt_ascent_px(*tt, out_ascent, out_descent);
}

int font_tt_getname(
    const PillowCFont* font,
    char* out_family,
    std::size_t family_size,
    std::size_t* out_family_required,
    char* out_style,
    std::size_t style_size,
    std::size_t* out_style_required)
{
    if (!font || !out_family_required || !out_style_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t family_required = tt->family.size() + 1;
    const std::size_t style_required = tt->style.size() + 1;
    *out_family_required = family_required;
    *out_style_required = style_required;
    if (!out_family || !out_style) {
        return PILLOW_C_NULL_POINTER;
    }
    if (family_size < family_required || style_size < style_required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_family, tt->family.c_str(), family_required);
    std::memcpy(out_style, tt->style.c_str(), style_required);
    return PILLOW_C_OK;
}

int font_tt_getbbox_impl(
    const PillowCTtFont& font,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (text[0] == '\0') {
        // Pillow's getsize("") is ((0,0),(0,0)).
        *out_left = 0;
        *out_top = 0;
        *out_right = 0;
        *out_bottom = 0;
        return PILLOW_C_OK;
    }
    std::vector<GlyphRunEntry> run;
    std::int64_t final_pen = 0;
    std::int64_t total = 0;
    build_glyph_run(font, text, &run, &final_pen, &total);
    int ascent = 0;
    int descent = 0;
    font_tt_ascent_px(font, &ascent, &descent);
    RunExtents extents{};
    const int status = compute_run_extents(font, run, final_pen, ascent, &extents);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = extents.offset_x;
    *out_top = extents.offset_y;
    *out_right = extents.offset_x + extents.width;
    *out_bottom = extents.offset_y + extents.height;
    return PILLOW_C_OK;
}

int font_tt_getbbox(
    const PillowCFont* font,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font || !text || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return font_tt_getbbox_impl(*tt, text, out_left, out_top, out_right, out_bottom);
}

int font_tt_getbbox_anchor(
    const PillowCFont* font,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font || !text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t anchor_length = std::strlen(anchor);
    if (anchor_length != 2 || (anchor[0] != 'l' && anchor[0] != 'm' && anchor[0] != 'r') ||
        (anchor[1] != 'a' && anchor[1] != 't' && anchor[1] != 'm' && anchor[1] != 's' &&
         anchor[1] != 'b')) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    const int status = font_tt_getbbox_impl(*tt, text, &left, &top, &right, &bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    const int width = right - left;
    const int height = bottom - top;
    int shift_x = 0;
    if (anchor[0] == 'm') {
        shift_x = width / 2;
    } else if (anchor[0] == 'r') {
        shift_x = width - 1;
    }
    int shift_y = 0;
    if (anchor[1] == 't') {
        shift_y = top;
    } else if (anchor[1] == 'm') {
        shift_y = top + (height + 1) / 2;
    } else if (anchor[1] == 's' || anchor[1] == 'b') {
        shift_y = top + height;
    }
    *out_left = left - shift_x;
    *out_top = top - shift_y;
    *out_right = right - shift_x;
    *out_bottom = bottom - shift_y;
    return PILLOW_C_OK;
}

int font_tt_getmask(
    const PillowCFont* font,
    const char* text,
    const char* mode,
    int ink,
    PillowCImage** out_image)
{
    if (!font || !text || !mode || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const auto* tt = static_cast<const PillowCTtFont*>(font->truetype);
    if (!tt) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool rgba = std::strcmp(mode, "RGBA") == 0;
    if (text[0] == '\0') {
        try {
            *out_image = new PillowCImage{
                0,
                0,
                rgba ? PILLOW_C_MODE_RGBA : PILLOW_C_MODE_L,
                rgba ? 4 : 1,
                0,
                std::vector<std::uint8_t>()};
            return PILLOW_C_OK;
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
    }
    std::vector<GlyphRunEntry> run;
    std::int64_t final_pen = 0;
    std::int64_t total = 0;
    build_glyph_run(*tt, text, &run, &final_pen, &total);
    int ascent = 0;
    int descent = 0;
    font_tt_ascent_px(*tt, &ascent, &descent);
    RunExtents extents{};
    const int status = compute_run_extents(*tt, run, final_pen, ascent, &extents);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return render_glyph_run_mask(*tt, run, extents, ascent, rgba, ink, out_image);
}

// Public exports (deliberate additions for BEHAV-FONTFILE-001).
extern "C" __declspec(dllexport) int pillow_c_font_load_file(
    const char* path,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font)
{
    return font_tt_load_file(path, size, index, encoding, layout_engine, out_font);
}

extern "C" __declspec(dllexport) int pillow_c_font_load_bytes(
    const std::uint8_t* data,
    std::size_t length,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font)
{
    return font_tt_load_bytes(data, length, size, index, encoding, layout_engine, out_font);
}

extern "C" __declspec(dllexport) int pillow_c_font_is_variable(
    const PillowCFont* font,
    int* out_variable)
{
    return font_tt_is_variable(font, out_variable);
}

// --- PILfont bitmap font (kind 3, BEHAV-FONTFILE-002) ----------------------
//
// Pillow 11.3.0's ImageFont.load parses the "PILfont" bitmap format: 256
// glyph entries of 10 big-endian int16s
// [dx, dy, dst_x0, dst_y0, dst_x1, dst_y1, src_x0, src_y0, src_x1, src_y1]
// plus a mode-1 or L glyph image. The pinned semantics (oracle
// probe_pilfont*.py):
// - getsize width = sum(dx); the font height = max(dst_y1) - min(dst_y0)
//   over all 256 glyphs;
// - each glyph blits its source box 1:1 into the mask at
//   (x + dst_x0, dst_y0 - min_dst_y0 + (max_dst_y1 - dst_y1)) and the pen
//   advances by dx (every UTF-8 byte, including \n, is a glyph index);
// - the mask mirrors the glyph-image mode: mode 1 stays a packed MSB-first
//   mode-1 image, L stays an L image with the source values verbatim;
// - a source box whose size differs from the destination box raises the
//   exact SystemError surface (`<method 'getmask' of 'ImagingFont'
//   objects> returned a result with an exception set`); out-of-range
//   coordinates are clipped.

namespace {

constexpr int PILLOW_C_FONT_ERR_PIL_MASK = -64;

std::int16_t pil_read_i16be(const std::uint8_t* p)
{
    return static_cast<std::int16_t>((p[0] << 8) | p[1]);
}

struct PillowCPilGlyph {
    std::int16_t dx = 0;
    std::int16_t dy = 0;
    std::int16_t dst_x0 = 0;
    std::int16_t dst_y0 = 0;
    std::int16_t dst_x1 = 0;
    std::int16_t dst_y1 = 0;
    std::int16_t src_x0 = 0;
    std::int16_t src_y0 = 0;
    std::int16_t src_x1 = 0;
    std::int16_t src_y1 = 0;
};

struct PillowCPilFont {
    PillowCPilGlyph glyphs[256];
    int font_height = 0;
    int max_dst_y1 = 0;
    int min_dst_y0 = 0;
    int image_mode = PILLOW_C_MODE_L;
    int image_width = 0;
    int image_height = 0;
    std::size_t image_stride = 0;
    std::vector<std::uint8_t> image;
};

} // namespace

int font_pil_load(
    const std::uint8_t* metrics,
    std::size_t metrics_size,
    const PillowCImage* glyph_image,
    PillowCFont** out_font)
{
    if (!out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_font = nullptr;
    if (!metrics || metrics_size < 5120 || !glyph_image) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (glyph_image->mode != PILLOW_C_MODE_1 && glyph_image->mode != PILLOW_C_MODE_L) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        std::unique_ptr<PillowCPilFont> font(new PillowCPilFont());
        font->image_mode = glyph_image->mode;
        font->image_width = glyph_image->width;
        font->image_height = glyph_image->height;
        font->image_stride = glyph_image->stride;
        font->image = glyph_image->pixels;
        for (int i = 0; i < 256; ++i) {
            const std::uint8_t* entry = metrics + static_cast<std::size_t>(i) * 20u;
            PillowCPilGlyph& glyph = font->glyphs[i];
            glyph.dx = pil_read_i16be(entry);
            glyph.dy = pil_read_i16be(entry + 2);
            glyph.dst_x0 = pil_read_i16be(entry + 4);
            glyph.dst_y0 = pil_read_i16be(entry + 6);
            glyph.dst_x1 = pil_read_i16be(entry + 8);
            glyph.dst_y1 = pil_read_i16be(entry + 10);
            glyph.src_x0 = pil_read_i16be(entry + 12);
            glyph.src_y0 = pil_read_i16be(entry + 14);
            glyph.src_x1 = pil_read_i16be(entry + 16);
            glyph.src_y1 = pil_read_i16be(entry + 18);
            font->max_dst_y1 = std::max(font->max_dst_y1, static_cast<int>(glyph.dst_y1));
            font->min_dst_y0 = std::min(font->min_dst_y0, static_cast<int>(glyph.dst_y0));
        }
        font->font_height = font->max_dst_y1 - font->min_dst_y0;
        if (font->font_height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_font = new PillowCFont{PILLOW_C_FONT_PIL, font.release()};
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int font_pil_free(PillowCFont* font)
{
    if (!font) {
        return PILLOW_C_OK;
    }
    delete static_cast<PillowCPilFont*>(font->truetype);
    delete font;
    return PILLOW_C_OK;
}

int font_pil_text_width(const PillowCPilFont& font, const char* text)
{
    const std::size_t length = std::strlen(text);
    int width = 0;
    for (std::size_t i = 0; i < length; ++i) {
        width += font.glyphs[static_cast<unsigned char>(text[i])].dx;
    }
    return width;
}

int font_pil_getlength(const PillowCFont* font, const char* text, double* out_length)
{
    if (!font || !text || !out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* pil = static_cast<const PillowCPilFont*>(font->truetype);
    if (!pil) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_length = static_cast<double>(font_pil_text_width(*pil, text));
    return PILLOW_C_OK;
}

int font_pil_getbbox(
    const PillowCFont* font,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font || !text || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    const auto* pil = static_cast<const PillowCPilFont*>(font->truetype);
    if (!pil) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_left = 0;
    *out_top = 0;
    *out_right = font_pil_text_width(*pil, text);
    *out_bottom = pil->font_height;
    return PILLOW_C_OK;
}

int font_pil_getmask(
    const PillowCFont* font,
    const char* text,
    const char* mode,
    int ink,
    PillowCImage** out_image)
{
    (void)mode;
    (void)ink;
    if (!font || !text || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const auto* pil = static_cast<const PillowCPilFont*>(font->truetype);
    if (!pil) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int width = font_pil_text_width(*pil, text);
    const int height = pil->font_height;
    const bool mode1 = pil->image_mode == PILLOW_C_MODE_1;
    // Mode-1 images use the DLL's byte-per-pixel storage (0/255); the facade
    // raw encoder packs them MSB-first for ToBytes.
    const std::size_t stride = static_cast<std::size_t>(std::max(width, 0));
    try {
        auto* image = new PillowCImage{
            width,
            height,
            mode1 ? PILLOW_C_MODE_1 : PILLOW_C_MODE_L,
            1,
            stride,
            std::vector<std::uint8_t>(stride * static_cast<std::size_t>(std::max(height, 0)), 0)};
        *out_image = image;
        const std::size_t length = std::strlen(text);
        int pen = 0;
        for (std::size_t i = 0; i < length; ++i) {
            const PillowCPilGlyph& glyph = pil->glyphs[static_cast<unsigned char>(text[i])];
            const int dst_width = glyph.dst_x1 - glyph.dst_x0;
            const int dst_height = glyph.dst_y1 - glyph.dst_y0;
            const int src_width = glyph.src_x1 - glyph.src_x0;
            const int src_height = glyph.src_y1 - glyph.src_y0;
            if (src_width != dst_width || src_height != dst_height) {
                return PILLOW_C_FONT_ERR_PIL_MASK;
            }
            const int base_x = pen + glyph.dst_x0;
            const int base_y = glyph.dst_y0 - pil->min_dst_y0;
            for (int sy = 0; sy < src_height; ++sy) {
                const int my = base_y + sy;
                if (my < 0 || my >= height) {
                    continue;
                }
                for (int sx = 0; sx < src_width; ++sx) {
                    const int mx = base_x + sx;
                    if (mx < 0 || mx >= width) {
                        continue;
                    }
                    const int source_x = glyph.src_x0 + sx;
                    const int source_y = glyph.src_y0 + sy;
                    if (source_x < 0 || source_y < 0 || source_x >= pil->image_width ||
                        source_y >= pil->image_height) {
                        continue;
                    }
                    if (!mode1) {
                        const std::uint8_t value =
                            pil->image[static_cast<std::size_t>(source_y) * pil->image_stride +
                                       static_cast<std::size_t>(source_x)];
                        if (value == 0) {
                            continue;
                        }
                        image->pixels[static_cast<std::size_t>(my) * stride +
                                      static_cast<std::size_t>(mx)] = value;
                    } else {
                        const std::uint8_t source_byte = pil->image[
                            static_cast<std::size_t>(source_y) * pil->image_stride +
                            static_cast<std::size_t>(source_x)];
                        if (source_byte == 0) {
                            continue;
                        }
                        image->pixels[static_cast<std::size_t>(my) * stride +
                                      static_cast<std::size_t>(mx)] = 255;
                    }
                }
            }
            pen += glyph.dx;
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

// Public export (deliberate addition for BEHAV-FONTFILE-002).
extern "C" __declspec(dllexport) int pillow_c_font_load_pil(
    const std::uint8_t* metrics,
    std::size_t metrics_size,
    const PillowCImage* glyph_image,
    PillowCFont** out_font)
{
    return font_pil_load(metrics, metrics_size, glyph_image, out_font);
}
