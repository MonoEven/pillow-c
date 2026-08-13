#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "pillow_c_internal.h"

int pillow_c_parse_exif_orientation(const std::uint8_t* payload, std::size_t payload_size)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return 0;
    }

    return pillow_c_tiff_parse_orientation(
        payload + sizeof(exif_header),
        payload_size - sizeof(exif_header));
}

bool pillow_c_round_to_i64(double value, std::int64_t* out_value)
{
    if (!out_value || !std::isfinite(value) ||
        value < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return false;
    }
    const double lower = std::floor(value);
    const double fraction = value - lower;
    double rounded = lower;
    if (fraction > 0.5) {
        rounded = lower + 1.0;
    } else if (fraction == 0.5) {
        const auto lower_i = static_cast<std::int64_t>(lower);
        rounded = (lower_i % 2 == 0) ? lower : lower + 1.0;
    }
    *out_value = static_cast<std::int64_t>(rounded);
    return true;
}

int copy_metadata_blob(
    const std::vector<std::uint8_t>& data,
    bool has_blob,
    int* out_has_blob,
    std::uint8_t* out_blob,
    std::size_t out_blob_size,
    std::size_t* out_blob_required)
{
    if (!out_has_blob || !out_blob_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t required = data.size();
    *out_has_blob = has_blob ? 1 : 0;
    *out_blob_required = required;
    if (required == 0u) {
        return PILLOW_C_OK;
    }
    if (!out_blob) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_blob_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_blob, data.data(), required);
    return PILLOW_C_OK;
}

void write_exif_u16_be(std::uint8_t* data, std::uint16_t value)
{
    data[0] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    data[1] = static_cast<std::uint8_t>(value & 0xffu);
}

void write_exif_u32_be(std::uint8_t* data, std::uint32_t value)
{
    data[0] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
    data[1] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    data[2] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    data[3] = static_cast<std::uint8_t>(value & 0xffu);
}

void write_exif_u64_be(std::uint8_t* data, std::uint64_t value)
{
    for (int index = 7; index >= 0; --index) {
        data[index] = static_cast<std::uint8_t>(value & 0xffu);
        value >>= 8u;
    }
}

struct ExifOutputEntry {
    std::uint16_t tag = 0;
    std::uint16_t type = 0;
    std::uint32_t count = 0;
    std::uint32_t uint_value = 0;
    std::uint32_t rational_numerator = 0;
    std::uint32_t rational_denominator = 0;
    const std::uint32_t* rational_array_numerators = nullptr;
    const std::uint32_t* rational_array_denominators = nullptr;
    std::size_t rational_array_value_count = 0;
    std::int32_t signed_rational_numerator = 0;
    std::int32_t signed_rational_denominator = 0;
    const std::int32_t* signed_rational_array_numerators = nullptr;
    const std::int32_t* signed_rational_array_denominators = nullptr;
    std::size_t signed_rational_array_value_count = 0;
    const char* ascii_value = nullptr;
    std::size_t ascii_size = 0;
    const std::uint32_t* short_array_values = nullptr;
    std::size_t short_array_count = 0;
    const std::uint32_t* uint_array_values = nullptr;
    std::size_t uint_array_count = 0;
    const double* double_array_values = nullptr;
    std::size_t double_array_count = 0;
    const float* float_array_values = nullptr;
    std::size_t float_array_count = 0;
    const std::uint8_t* byte_array_values = nullptr;
    std::size_t byte_array_count = 0;
};

int copy_exif_entries_internal_uint_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
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
    const int* uint_array_tags,
    const std::uint32_t* uint_array_values,
    std::size_t uint_array_value_count,
    const std::size_t* uint_array_offsets,
    const std::size_t* uint_array_counts,
    std::size_t uint_array_count,
    const int* double_array_tags,
    const double* double_array_values,
    std::size_t double_array_value_count,
    const std::size_t* double_array_offsets,
    const std::size_t* double_array_counts,
    std::size_t double_array_count,
    const int* float_array_tags,
    const float* float_array_values,
    std::size_t float_array_value_count,
    const std::size_t* float_array_offsets,
    const std::size_t* float_array_counts,
    std::size_t float_array_count,
    const int* byte_array_tags,
    const std::uint8_t* byte_array_values,
    std::size_t byte_array_value_count,
    const std::size_t* byte_array_offsets,
    const std::size_t* byte_array_counts,
    std::size_t byte_array_count,
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* signed_rational_array_tags,
    const std::int32_t* signed_rational_array_numerators,
    const std::int32_t* signed_rational_array_denominators,
    std::size_t signed_rational_array_value_count,
    const std::size_t* signed_rational_array_offsets,
    const std::size_t* signed_rational_array_counts,
    std::size_t signed_rational_array_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    if (!out_exif_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_exif_required = 0u;
    if (orientation < 0 || orientation > 0xffff ||
        ascii_count > 0xffffu || uint_count > 0xffffu ||
        rational_count > 0xffffu || rational_array_count > 0xffffu ||
        short_array_count > 0xffffu || uint_array_count > 0xffffu || double_array_count > 0xffffu ||
        float_array_count > 0xffffu ||
        byte_array_count > 0xffffu || signed_rational_count > 0xffffu ||
        signed_rational_array_count > 0xffffu ||
        undefined_count > 0xffffu) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (ascii_count > 0u && (!ascii_tags || !ascii_values)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (uint_count > 0u && (!uint_tags || !uint_values || !uint_types)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (rational_count > 0u && (!rational_tags || !rational_numerators || !rational_denominators)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (rational_array_count > 0u && (!rational_array_tags || !rational_array_numerators ||
        !rational_array_denominators || !rational_array_offsets || !rational_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (short_array_count > 0u && (!short_array_tags || !short_array_values || !short_array_offsets || !short_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (uint_array_count > 0u && (!uint_array_tags || !uint_array_values || !uint_array_offsets || !uint_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (double_array_count > 0u && (!double_array_tags || !double_array_values || !double_array_offsets || !double_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (float_array_count > 0u && (!float_array_tags || !float_array_values || !float_array_offsets || !float_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (byte_array_count > 0u && (!byte_array_tags || !byte_array_values || !byte_array_offsets || !byte_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (signed_rational_count > 0u && (!signed_rational_tags || !signed_rational_numerators || !signed_rational_denominators)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (signed_rational_array_count > 0u && (!signed_rational_array_tags || !signed_rational_array_numerators ||
        !signed_rational_array_denominators || !signed_rational_array_offsets || !signed_rational_array_counts)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (undefined_count > 0u && (!undefined_tags || !undefined_values || !undefined_offsets || !undefined_counts)) {
        return PILLOW_C_NULL_POINTER;
    }

    std::vector<ExifOutputEntry> entries;
    entries.reserve(ascii_count + uint_count + rational_count + rational_array_count + short_array_count + uint_array_count + double_array_count + float_array_count + byte_array_count +
        signed_rational_count + signed_rational_array_count + undefined_count + (orientation != 0 ? 1u : 0u));
    for (std::size_t index = 0; index < ascii_count; ++index) {
        const int tag = ascii_tags[index];
        const char* value = ascii_values[index];
        if (!value) {
            return PILLOW_C_NULL_POINTER;
        }
        if (tag <= 0 || tag > 0xffff || tag == 274) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::size_t value_size = std::strlen(value);
        if (value_size > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1u) {
            return PILLOW_C_INVALID_LENGTH;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 2u;
        entry.count = static_cast<std::uint32_t>(value_size + 1u);
        entry.ascii_value = value;
        entry.ascii_size = value_size;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < uint_count; ++index) {
        const int tag = uint_tags[index];
        const int type = uint_types[index];
        const std::uint32_t value = uint_values[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || (type != 3 && type != 4)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (type == 3 && value > 0xffffu) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = static_cast<std::uint16_t>(type);
        entry.count = 1u;
        entry.uint_value = value;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < rational_count; ++index) {
        const int tag = rational_tags[index];
        const std::uint32_t numerator = rational_numerators[index];
        const std::uint32_t denominator = rational_denominators[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || denominator == 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 5u;
        entry.count = 1u;
        entry.rational_numerator = numerator;
        entry.rational_denominator = denominator;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < rational_array_count; ++index) {
        const int tag = rational_array_tags[index];
        const std::size_t value_offset = rational_array_offsets[index];
        const std::size_t value_count = rational_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > rational_array_value_count || value_count > rational_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        for (std::size_t value_index = 0; value_index < value_count; ++value_index) {
            if (rational_array_denominators[value_offset + value_index] == 0u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 5u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.rational_array_numerators = rational_array_numerators + value_offset;
        entry.rational_array_denominators = rational_array_denominators + value_offset;
        entry.rational_array_value_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < short_array_count; ++index) {
        const int tag = short_array_tags[index];
        const std::size_t value_offset = short_array_offsets[index];
        const std::size_t value_count = short_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > short_array_value_count || value_count > short_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        for (std::size_t value_index = 0; value_index < value_count; ++value_index) {
            if (short_array_values[value_offset + value_index] > 0xffffu) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 3u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.short_array_values = short_array_values + value_offset;
        entry.short_array_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < uint_array_count; ++index) {
        const int tag = uint_array_tags[index];
        const std::size_t value_offset = uint_array_offsets[index];
        const std::size_t value_count = uint_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > uint_array_value_count || value_count > uint_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 4u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.uint_array_values = uint_array_values + value_offset;
        entry.uint_array_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < double_array_count; ++index) {
        const int tag = double_array_tags[index];
        const std::size_t value_offset = double_array_offsets[index];
        const std::size_t value_count = double_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > double_array_value_count || value_count > double_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 12u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.double_array_values = double_array_values + value_offset;
        entry.double_array_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < float_array_count; ++index) {
        const int tag = float_array_tags[index];
        const std::size_t value_offset = float_array_offsets[index];
        const std::size_t value_count = float_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > float_array_value_count || value_count > float_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 11u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.float_array_values = float_array_values + value_offset;
        entry.float_array_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < byte_array_count; ++index) {
        const int tag = byte_array_tags[index];
        const std::size_t value_offset = byte_array_offsets[index];
        const std::size_t value_count = byte_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > byte_array_value_count || value_count > byte_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 1u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.byte_array_values = byte_array_values + value_offset;
        entry.byte_array_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < signed_rational_count; ++index) {
        const int tag = signed_rational_tags[index];
        const std::int32_t numerator = signed_rational_numerators[index];
        const std::int32_t denominator = signed_rational_denominators[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || denominator == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 10u;
        entry.count = 1u;
        entry.signed_rational_numerator = numerator;
        entry.signed_rational_denominator = denominator;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < signed_rational_array_count; ++index) {
        const int tag = signed_rational_array_tags[index];
        const std::size_t value_offset = signed_rational_array_offsets[index];
        const std::size_t value_count = signed_rational_array_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > signed_rational_array_value_count ||
            value_count > signed_rational_array_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        for (std::size_t value_index = 0; value_index < value_count; ++value_index) {
            if (signed_rational_array_denominators[value_offset + value_index] == 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 10u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.signed_rational_array_numerators = signed_rational_array_numerators + value_offset;
        entry.signed_rational_array_denominators = signed_rational_array_denominators + value_offset;
        entry.signed_rational_array_value_count = value_count;
        entries.push_back(entry);
    }
    for (std::size_t index = 0; index < undefined_count; ++index) {
        const int tag = undefined_tags[index];
        const std::size_t value_offset = undefined_offsets[index];
        const std::size_t value_count = undefined_counts[index];
        if (tag <= 0 || tag > 0xffff || tag == 274 || value_count == 0u ||
            value_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (value_offset > undefined_value_count || value_count > undefined_value_count - value_offset) {
            return PILLOW_C_INVALID_LENGTH;
        }
        ExifOutputEntry entry;
        entry.tag = static_cast<std::uint16_t>(tag);
        entry.type = 7u;
        entry.count = static_cast<std::uint32_t>(value_count);
        entry.byte_array_values = undefined_values + value_offset;
        entry.byte_array_count = value_count;
        entries.push_back(entry);
    }
    if (orientation != 0) {
        ExifOutputEntry entry;
        entry.tag = 274u;
        entry.type = 3u;
        entry.count = 1u;
        entry.uint_value = static_cast<std::uint32_t>(orientation);
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const ExifOutputEntry& left, const ExifOutputEntry& right) {
        return left.tag < right.tag;
    });

    static constexpr std::uint8_t exif_prefix[] = {'E', 'x', 'i', 'f', 0, 0};
    const std::size_t entry_count = entries.size();
    if (entry_count > 0xffffu) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t tiff_header_size = 8u;
    const std::size_t ifd_count_size = 2u;
    const std::size_t entry_size = 12u;
    const std::size_t next_ifd_size = 4u;
    const std::size_t data_offset = tiff_header_size + ifd_count_size + entry_count * entry_size + next_ifd_size;
    std::size_t total_size = sizeof(exif_prefix) + data_offset;
    for (const auto& entry : entries) {
        if (entry.type == 2u && entry.count > 4u) {
            std::size_t stored_size = static_cast<std::size_t>(entry.count);
            if ((stored_size & 1u) != 0u) {
                ++stored_size;
            }
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if (entry.type == 5u && entry.rational_array_numerators) {
            if (entry.rational_array_value_count > std::numeric_limits<std::size_t>::max() / 8u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::size_t stored_size = entry.rational_array_value_count * 8u;
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if (entry.type == 10u && entry.signed_rational_array_numerators) {
            if (entry.signed_rational_array_value_count > std::numeric_limits<std::size_t>::max() / 8u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::size_t stored_size = entry.signed_rational_array_value_count * 8u;
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if (entry.type == 5u || entry.type == 10u) {
            if (total_size > std::numeric_limits<std::size_t>::max() - 8u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += 8u;
        } else if (entry.type == 3u && entry.short_array_values && entry.short_array_count > 2u) {
            if (entry.short_array_count > std::numeric_limits<std::size_t>::max() / 2u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::size_t stored_size = entry.short_array_count * 2u;
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if (entry.type == 4u && entry.uint_array_values && entry.uint_array_count > 1u) {
            if (entry.uint_array_count > std::numeric_limits<std::size_t>::max() / 4u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::size_t stored_size = entry.uint_array_count * 4u;
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if (entry.type == 12u && entry.double_array_values) {
            if (entry.double_array_count > std::numeric_limits<std::size_t>::max() / 8u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::size_t stored_size = entry.double_array_count * 8u;
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if (entry.type == 11u && entry.float_array_values && entry.float_array_count > 1u) {
            if (entry.float_array_count > std::numeric_limits<std::size_t>::max() / 4u) {
                return PILLOW_C_INVALID_LENGTH;
            }
            const std::size_t stored_size = entry.float_array_count * 4u;
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        } else if ((entry.type == 1u || entry.type == 7u) && entry.byte_array_values && entry.byte_array_count > 4u) {
            std::size_t stored_size = entry.byte_array_count;
            if ((stored_size & 1u) != 0u) {
                ++stored_size;
            }
            if (total_size > std::numeric_limits<std::size_t>::max() - stored_size) {
                return PILLOW_C_INVALID_LENGTH;
            }
            total_size += stored_size;
        }
    }

    *out_exif_required = total_size;
    if (!out_exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_exif_size < total_size) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::memset(out_exif, 0, total_size);
    std::memcpy(out_exif, exif_prefix, sizeof(exif_prefix));
    std::uint8_t* tiff = out_exif + sizeof(exif_prefix);
    tiff[0] = 'M';
    tiff[1] = 'M';
    write_exif_u16_be(tiff + 2u, 42u);
    write_exif_u32_be(tiff + 4u, 8u);
    write_exif_u16_be(tiff + 8u, static_cast<std::uint16_t>(entry_count));

    std::size_t variable_data_offset = data_offset;
    for (std::size_t index = 0; index < entry_count; ++index) {
        const auto& entry = entries[index];
        std::uint8_t* ifd_entry = tiff + 10u + index * entry_size;
        write_exif_u16_be(ifd_entry, entry.tag);
        write_exif_u16_be(ifd_entry + 2u, entry.type);
        write_exif_u32_be(ifd_entry + 4u, entry.count);
        if ((entry.type == 1u || entry.type == 7u) && entry.byte_array_values) {
            std::uint8_t* value_target = nullptr;
            if (entry.byte_array_count <= 4u) {
                value_target = ifd_entry + 8u;
            } else {
                write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
                value_target = tiff + variable_data_offset;
                variable_data_offset += entry.byte_array_count;
                if ((variable_data_offset & 1u) != 0u) {
                    ++variable_data_offset;
                }
            }
            std::memcpy(value_target, entry.byte_array_values, entry.byte_array_count);
        } else if (entry.type == 3u && entry.short_array_values) {
            const std::size_t value_bytes = entry.short_array_count * 2u;
            std::uint8_t* value_target = nullptr;
            if (value_bytes <= 4u) {
                value_target = ifd_entry + 8u;
            } else {
                write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
                value_target = tiff + variable_data_offset;
                variable_data_offset += value_bytes;
            }
            for (std::size_t value_index = 0; value_index < entry.short_array_count; ++value_index) {
                write_exif_u16_be(
                    value_target + value_index * 2u,
                    static_cast<std::uint16_t>(entry.short_array_values[value_index]));
            }
        } else if (entry.type == 3u) {
            write_exif_u16_be(ifd_entry + 8u, static_cast<std::uint16_t>(entry.uint_value));
        } else if (entry.type == 4u && entry.uint_array_values) {
            const std::size_t value_bytes = entry.uint_array_count * 4u;
            std::uint8_t* value_target = nullptr;
            if (value_bytes <= 4u) {
                value_target = ifd_entry + 8u;
            } else {
                write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
                value_target = tiff + variable_data_offset;
                variable_data_offset += value_bytes;
            }
            for (std::size_t value_index = 0; value_index < entry.uint_array_count; ++value_index) {
                write_exif_u32_be(value_target + value_index * 4u, entry.uint_array_values[value_index]);
            }
        } else if (entry.type == 4u) {
            write_exif_u32_be(ifd_entry + 8u, entry.uint_value);
        } else if (entry.type == 12u && entry.double_array_values) {
            write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
            std::uint8_t* value_target = tiff + variable_data_offset;
            for (std::size_t value_index = 0; value_index < entry.double_array_count; ++value_index) {
                std::uint64_t bits = 0u;
                std::memcpy(&bits, entry.double_array_values + value_index, sizeof(bits));
                write_exif_u64_be(value_target + value_index * 8u, bits);
            }
            variable_data_offset += entry.double_array_count * 8u;
        } else if (entry.type == 11u && entry.float_array_values) {
            const std::size_t value_bytes = entry.float_array_count * 4u;
            std::uint8_t* value_target = nullptr;
            if (value_bytes <= 4u) {
                value_target = ifd_entry + 8u;
            } else {
                write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
                value_target = tiff + variable_data_offset;
                variable_data_offset += value_bytes;
            }
            for (std::size_t value_index = 0; value_index < entry.float_array_count; ++value_index) {
                std::uint32_t bits = 0u;
                std::memcpy(&bits, entry.float_array_values + value_index, sizeof(bits));
                write_exif_u32_be(value_target + value_index * 4u, bits);
            }
        } else if (entry.type == 5u && entry.rational_array_numerators) {
            write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
            std::uint8_t* value_target = tiff + variable_data_offset;
            for (std::size_t value_index = 0; value_index < entry.rational_array_value_count; ++value_index) {
                write_exif_u32_be(value_target + value_index * 8u, entry.rational_array_numerators[value_index]);
                write_exif_u32_be(value_target + value_index * 8u + 4u, entry.rational_array_denominators[value_index]);
            }
            variable_data_offset += entry.rational_array_value_count * 8u;
        } else if (entry.type == 5u) {
            write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
            std::uint8_t* value_target = tiff + variable_data_offset;
            write_exif_u32_be(value_target, entry.rational_numerator);
            write_exif_u32_be(value_target + 4u, entry.rational_denominator);
            variable_data_offset += 8u;
        } else if (entry.type == 10u && entry.signed_rational_array_numerators) {
            write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
            std::uint8_t* value_target = tiff + variable_data_offset;
            for (std::size_t value_index = 0; value_index < entry.signed_rational_array_value_count; ++value_index) {
                write_exif_u32_be(
                    value_target + value_index * 8u,
                    static_cast<std::uint32_t>(entry.signed_rational_array_numerators[value_index]));
                write_exif_u32_be(
                    value_target + value_index * 8u + 4u,
                    static_cast<std::uint32_t>(entry.signed_rational_array_denominators[value_index]));
            }
            variable_data_offset += entry.signed_rational_array_value_count * 8u;
        } else if (entry.type == 10u) {
            write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
            std::uint8_t* value_target = tiff + variable_data_offset;
            write_exif_u32_be(value_target, static_cast<std::uint32_t>(entry.signed_rational_numerator));
            write_exif_u32_be(value_target + 4u, static_cast<std::uint32_t>(entry.signed_rational_denominator));
            variable_data_offset += 8u;
        } else if (entry.count <= 4u) {
            std::memcpy(ifd_entry + 8u, entry.ascii_value, entry.ascii_size);
        } else {
            write_exif_u32_be(ifd_entry + 8u, static_cast<std::uint32_t>(variable_data_offset));
            std::uint8_t* value_target = tiff + variable_data_offset;
            std::memcpy(value_target, entry.ascii_value, entry.ascii_size);
            variable_data_offset += static_cast<std::size_t>(entry.count);
            if ((variable_data_offset & 1u) != 0u) {
                ++variable_data_offset;
            }
        }
    }
    return PILLOW_C_OK;
}

// Native EXIF readers and metadata ABI exports live in this translation unit.
// The shared metadata seam keeps image ownership in the DLL without routing
// metadata through the monolithic implementation unit.

int copy_exif_orientation_bytes(int orientation, std::uint8_t* out_exif, std::size_t out_exif_size, std::size_t* out_exif_required)
{
    static constexpr std::uint8_t empty_exif[] = {
        0x45u, 0x78u, 0x69u, 0x66u, 0x00u, 0x00u,
        0x4du, 0x4du, 0x00u, 0x2au, 0x00u, 0x00u, 0x00u, 0x08u,
        0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    static constexpr std::uint8_t orientation_exif[] = {
        0x45u, 0x78u, 0x69u, 0x66u, 0x00u, 0x00u,
        0x4du, 0x4du, 0x00u, 0x2au, 0x00u, 0x00u, 0x00u, 0x08u,
        0x00u, 0x01u,
        0x01u, 0x12u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
    };
    if (!out_exif_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (orientation < 0 || orientation > 0xffff) {
        *out_exif_required = 0u;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool has_orientation = orientation != 0;
    const std::size_t required = has_orientation ? sizeof(orientation_exif) : sizeof(empty_exif);
    *out_exif_required = required;
    if (!out_exif) {
        return out_exif_size == 0u ? PILLOW_C_OK : PILLOW_C_NULL_POINTER;
    }
    if (out_exif_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!has_orientation) {
        std::memcpy(out_exif, empty_exif, required);
        return PILLOW_C_OK;
    }
    std::memcpy(out_exif, orientation_exif, required);
    out_exif[24] = static_cast<std::uint8_t>((orientation >> 8) & 0xff);
    out_exif[25] = static_cast<std::uint8_t>(orientation & 0xff);
    return PILLOW_C_OK;
}

int copy_exif_entries_internal_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
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
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* signed_rational_array_tags,
    const std::int32_t* signed_rational_array_numerators,
    const std::int32_t* signed_rational_array_denominators,
    std::size_t signed_rational_array_value_count,
    const std::size_t* signed_rational_array_offsets,
    const std::size_t* signed_rational_array_counts,
    std::size_t signed_rational_array_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_internal_uint_array_bytes(
        orientation,
        ascii_tags,
        ascii_values,
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
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        byte_array_tags,
        byte_array_values,
        byte_array_value_count,
        byte_array_offsets,
        byte_array_counts,
        byte_array_count,
        signed_rational_tags,
        signed_rational_numerators,
        signed_rational_denominators,
        signed_rational_count,
        signed_rational_array_tags,
        signed_rational_array_numerators,
        signed_rational_array_denominators,
        signed_rational_array_value_count,
        signed_rational_array_offsets,
        signed_rational_array_counts,
        signed_rational_array_count,
        undefined_tags,
        undefined_values,
        undefined_value_count,
        undefined_offsets,
        undefined_counts,
        undefined_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_undefined_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
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
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_internal_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
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
        signed_rational_tags,
        signed_rational_numerators,
        signed_rational_denominators,
        signed_rational_count,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        undefined_tags,
        undefined_values,
        undefined_value_count,
        undefined_offsets,
        undefined_counts,
        undefined_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_rational_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
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
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_internal_bytes(
        orientation,
        ascii_tags,
        ascii_values,
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
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_signed_rational_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
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
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_undefined_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
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
        signed_rational_tags,
        signed_rational_numerators,
        signed_rational_denominators,
        signed_rational_count,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_byte_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
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
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_signed_rational_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
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
        nullptr,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_short_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    const int* short_array_tags,
    const std::uint32_t* short_array_values,
    std::size_t short_array_value_count,
    const std::size_t* short_array_offsets,
    const std::size_t* short_array_counts,
    std::size_t short_array_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_byte_array_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
        short_array_tags,
        short_array_values,
        short_array_value_count,
        short_array_offsets,
        short_array_counts,
        short_array_count,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_full_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_short_array_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
        nullptr,
        nullptr,
        0u,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_typed_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_full_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        nullptr,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int copy_exif_entries_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_typed_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        nullptr,
        nullptr,
        nullptr,
        0u,
        out_exif,
        out_exif_size,
        out_exif_required);
}

int parse_exif_ascii_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::string* out_value,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_value || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_value->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 2u || count == 0u) {
            return PILLOW_C_OK;
        }

        const std::uint8_t* value = nullptr;
        if (count <= 4u) {
            value = entry + 8u;
        } else {
            const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
            if (value_offset > tiff_size || static_cast<std::size_t>(count) > tiff_size - static_cast<std::size_t>(value_offset)) {
                return PILLOW_C_OK;
            }
            value = tiff + value_offset;
        }
        std::size_t value_size = 0u;
        while (value_size < static_cast<std::size_t>(count) && value[value_size] != 0u) {
            ++value_size;
        }
        out_value->assign(reinterpret_cast<const char*>(value), value_size);
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_uint_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::uint32_t* out_value,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_value || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_value = 0u;
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (count != 1u) {
            return PILLOW_C_OK;
        }
        if (type == 3u) {
            *out_value = pillow_c_tiff_read16(entry + 8u, little_endian);
        } else if (type == 4u) {
            *out_value = pillow_c_tiff_read32(entry + 8u, little_endian);
        } else {
            return PILLOW_C_OK;
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_rational_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::uint32_t* out_numerator,
    std::uint32_t* out_denominator,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_numerator || !out_denominator || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_numerator = 0u;
    *out_denominator = 0u;
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 5u || count != 1u) {
            return PILLOW_C_OK;
        }
        const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
        if (value_offset > tiff_size || 8u > tiff_size - static_cast<std::size_t>(value_offset)) {
            return PILLOW_C_OK;
        }
        const std::uint8_t* value = tiff + value_offset;
        *out_numerator = pillow_c_tiff_read32(value, little_endian);
        *out_denominator = pillow_c_tiff_read32(value + 4u, little_endian);
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_rational_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<std::uint32_t>* out_numerators,
    std::vector<std::uint32_t>* out_denominators,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_numerators || !out_denominators || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_numerators->clear();
    out_denominators->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 5u || count == 0u ||
            count > std::numeric_limits<std::size_t>::max() / 8u) {
            return PILLOW_C_OK;
        }
        const std::size_t value_bytes = static_cast<std::size_t>(count) * 8u;
        const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
        if (value_offset > tiff_size || value_bytes > tiff_size - static_cast<std::size_t>(value_offset)) {
            return PILLOW_C_OK;
        }
        const std::uint8_t* value = tiff + value_offset;
        out_numerators->reserve(count);
        out_denominators->reserve(count);
        for (std::uint32_t value_index = 0u; value_index < count; ++value_index) {
            const std::uint8_t* item = value + static_cast<std::size_t>(value_index) * 8u;
            out_numerators->push_back(pillow_c_tiff_read32(item, little_endian));
            out_denominators->push_back(pillow_c_tiff_read32(item + 4u, little_endian));
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_signed_rational_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<std::int32_t>* out_numerators,
    std::vector<std::int32_t>* out_denominators,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_numerators || !out_denominators || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_numerators->clear();
    out_denominators->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        if (pillow_c_tiff_read16(entry, little_endian) != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        if (!pillow_c_tiff_read_signed_rational_array_entry_value(
                tiff,
                tiff_size,
                little_endian,
                entry,
                out_numerators,
                out_denominators)) {
            return PILLOW_C_OK;
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_signed_rational_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::int32_t* out_numerator,
    std::int32_t* out_denominator,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_numerator || !out_denominator || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_numerator = 0;
    *out_denominator = 0;
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 10u || count != 1u) {
            return PILLOW_C_OK;
        }
        const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
        if (value_offset > tiff_size || 8u > tiff_size - static_cast<std::size_t>(value_offset)) {
            return PILLOW_C_OK;
        }
        const std::uint8_t* value = tiff + value_offset;
        const std::uint32_t raw_numerator = pillow_c_tiff_read32(value, little_endian);
        const std::uint32_t raw_denominator = pillow_c_tiff_read32(value + 4u, little_endian);
        *out_numerator = raw_numerator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(raw_numerator)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_numerator) - 0x100000000ll);
        *out_denominator = raw_denominator <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(raw_denominator)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(raw_denominator) - 0x100000000ll);
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_ushort_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<std::uint32_t>* out_values,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_values || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_values->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 3u || count == 0u) {
            return PILLOW_C_OK;
        }
        const std::size_t value_bytes = static_cast<std::size_t>(count) * 2u;
        const std::uint8_t* value = nullptr;
        if (value_bytes <= 4u) {
            value = entry + 8u;
        } else {
            const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
            if (value_offset > tiff_size || value_bytes > tiff_size - static_cast<std::size_t>(value_offset)) {
                return PILLOW_C_OK;
            }
            value = tiff + value_offset;
        }
        out_values->reserve(count);
        for (std::uint32_t value_index = 0; value_index < count; ++value_index) {
            out_values->push_back(pillow_c_tiff_read16(value + static_cast<std::size_t>(value_index) * 2u, little_endian));
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_uint_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<std::uint32_t>* out_values,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_values || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_values->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 4u || count == 0u) {
            return PILLOW_C_OK;
        }
        const std::size_t value_bytes = static_cast<std::size_t>(count) * 4u;
        const std::uint8_t* value = nullptr;
        if (value_bytes <= 4u) {
            value = entry + 8u;
        } else {
            const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
            if (value_offset > tiff_size || value_bytes > tiff_size - static_cast<std::size_t>(value_offset)) {
                return PILLOW_C_OK;
            }
            value = tiff + value_offset;
        }
        out_values->reserve(count);
        for (std::uint32_t value_index = 0; value_index < count; ++value_index) {
            out_values->push_back(pillow_c_tiff_read32(value + static_cast<std::size_t>(value_index) * 4u, little_endian));
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_float_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<float>* out_values,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_values || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_values->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 11u || count == 0u || count > std::numeric_limits<std::size_t>::max() / 4u) {
            return PILLOW_C_OK;
        }
        const std::size_t value_size = static_cast<std::size_t>(count) * 4u;
        const std::uint8_t* value = nullptr;
        if (value_size <= 4u) {
            value = entry + 8u;
        } else {
            const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
            if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
                return PILLOW_C_OK;
            }
            value = tiff + value_offset;
        }
        out_values->reserve(count);
        for (std::uint32_t value_index = 0u; value_index < count; ++value_index) {
            const std::uint32_t bits = pillow_c_tiff_read32(value + static_cast<std::size_t>(value_index) * 4u, little_endian);
            float number = 0.0f;
            std::memcpy(&number, &bits, sizeof(number));
            out_values->push_back(number);
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_double_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<double>* out_values,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_values || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_values->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 12u || count == 0u || count > std::numeric_limits<std::size_t>::max() / 8u) {
            return PILLOW_C_OK;
        }
        const std::size_t value_size = static_cast<std::size_t>(count) * 8u;
        const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
        if (value_offset > tiff_size || value_size > tiff_size - static_cast<std::size_t>(value_offset)) {
            return PILLOW_C_OK;
        }
        const std::uint8_t* value = tiff + value_offset;
        out_values->reserve(count);
        for (std::uint32_t value_index = 0u; value_index < count; ++value_index) {
            const std::uint64_t bits = pillow_c_tiff_read64(value + static_cast<std::size_t>(value_index) * 8u, little_endian);
            double number = 0.0;
            std::memcpy(&number, &bits, sizeof(number));
            out_values->push_back(number);
        }
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_byte_array_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<std::uint8_t>* out_values,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_values || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_values->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 1u || count == 0u) {
            return PILLOW_C_OK;
        }
        const std::uint8_t* value = nullptr;
        if (count <= 4u) {
            value = entry + 8u;
        } else {
            const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
            if (value_offset > tiff_size || count > tiff_size - static_cast<std::size_t>(value_offset)) {
                return PILLOW_C_OK;
            }
            value = tiff + value_offset;
        }
        out_values->assign(value, value + count);
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}

int parse_exif_undefined_tag(
    const std::uint8_t* payload,
    std::size_t payload_size,
    int requested_tag,
    std::vector<std::uint8_t>* out_values,
    bool* out_has_value)
{
    static constexpr std::uint8_t exif_header[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (!out_values || !out_has_value) {
        return PILLOW_C_NULL_POINTER;
    }
    out_values->clear();
    *out_has_value = false;
    if (requested_tag <= 0 || requested_tag > 0xffff) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!payload || payload_size < sizeof(exif_header) + 8u ||
        std::memcmp(payload, exif_header, sizeof(exif_header)) != 0) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* tiff = payload + sizeof(exif_header);
    const std::size_t tiff_size = payload_size - sizeof(exif_header);
    const bool little_endian = tiff[0] == 'I' && tiff[1] == 'I';
    const bool big_endian = tiff[0] == 'M' && tiff[1] == 'M';
    if ((!little_endian && !big_endian) || pillow_c_tiff_read16(tiff + 2u, little_endian) != 42u) {
        return PILLOW_C_OK;
    }
    const std::uint32_t ifd_offset = pillow_c_tiff_read32(tiff + 4u, little_endian);
    if (ifd_offset > tiff_size || tiff_size - ifd_offset < 2u) {
        return PILLOW_C_OK;
    }

    const std::uint8_t* ifd = tiff + ifd_offset;
    const std::uint16_t entry_count = pillow_c_tiff_read16(ifd, little_endian);
    const std::size_t entries_offset = static_cast<std::size_t>(ifd_offset) + 2u;
    if (entries_offset > tiff_size || entry_count > (tiff_size - entries_offset) / 12u) {
        return PILLOW_C_OK;
    }

    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::uint8_t* entry = tiff + entries_offset + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = pillow_c_tiff_read16(entry, little_endian);
        if (tag != static_cast<std::uint16_t>(requested_tag)) {
            continue;
        }
        const std::uint16_t type = pillow_c_tiff_read16(entry + 2u, little_endian);
        const std::uint32_t count = pillow_c_tiff_read32(entry + 4u, little_endian);
        if (type != 7u || count == 0u) {
            return PILLOW_C_OK;
        }
        const std::uint8_t* value = nullptr;
        if (count <= 4u) {
            value = entry + 8u;
        } else {
            const std::uint32_t value_offset = pillow_c_tiff_read32(entry + 8u, little_endian);
            if (value_offset > tiff_size || count > tiff_size - static_cast<std::size_t>(value_offset)) {
                return PILLOW_C_OK;
            }
            value = tiff + value_offset;
        }
        out_values->assign(value, value + count);
        *out_has_value = true;
        return PILLOW_C_OK;
    }
    return PILLOW_C_OK;
}


extern "C" __declspec(dllexport) int pillow_c_exif_orientation_bytes(
    int orientation,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_orientation_bytes(orientation, out_exif, out_exif_size, out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_typed_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_typed_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_full_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_full_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_short_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
    const int* short_array_tags,
    const std::uint32_t* short_array_values,
    std::size_t short_array_value_count,
    const std::size_t* short_array_offsets,
    const std::size_t* short_array_counts,
    std::size_t short_array_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_short_array_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
        short_array_tags,
        short_array_values,
        short_array_value_count,
        short_array_offsets,
        short_array_counts,
        short_array_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_byte_array_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
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
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_byte_array_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
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
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_signed_rational_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
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
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_signed_rational_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
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
        signed_rational_tags,
        signed_rational_numerators,
        signed_rational_denominators,
        signed_rational_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_entries_undefined_bytes(
    int orientation,
    const int* ascii_tags,
    const char* const* ascii_values,
    std::size_t ascii_count,
    const int* uint_tags,
    const std::uint32_t* uint_values,
    const int* uint_types,
    std::size_t uint_count,
    const int* rational_tags,
    const std::uint32_t* rational_numerators,
    const std::uint32_t* rational_denominators,
    std::size_t rational_count,
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
    const int* signed_rational_tags,
    const std::int32_t* signed_rational_numerators,
    const std::int32_t* signed_rational_denominators,
    std::size_t signed_rational_count,
    const int* undefined_tags,
    const std::uint8_t* undefined_values,
    std::size_t undefined_value_count,
    const std::size_t* undefined_offsets,
    const std::size_t* undefined_counts,
    std::size_t undefined_count,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    return copy_exif_entries_undefined_bytes(
        orientation,
        ascii_tags,
        ascii_values,
        ascii_count,
        uint_tags,
        uint_values,
        uint_types,
        uint_count,
        rational_tags,
        rational_numerators,
        rational_denominators,
        rational_count,
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
        signed_rational_tags,
        signed_rational_numerators,
        signed_rational_denominators,
        signed_rational_count,
        undefined_tags,
        undefined_values,
        undefined_value_count,
        undefined_offsets,
        undefined_counts,
        undefined_count,
        out_exif,
        out_exif_size,
        out_exif_required);
}

extern "C" __declspec(dllexport) int pillow_c_exif_ascii_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    char* out_value,
    std::size_t out_value_size,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    std::string value;
    bool has_value = false;
    const int status = parse_exif_ascii_tag(exif, exif_size, tag, &value, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? value.size() + 1u : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_value) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_size < value.size() + 1u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_value, value.c_str(), value.size() + 1u);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_uint_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint32_t* out_value)
{
    if (!out_has_tag || !out_value) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::uint32_t value = 0u;
    const int status = parse_exif_uint_tag(exif, exif_size, tag, &value, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value = has_value ? value : 0u;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_rational_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint32_t* out_numerator,
    std::uint32_t* out_denominator)
{
    if (!out_has_tag || !out_numerator || !out_denominator) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::uint32_t numerator = 0u;
    std::uint32_t denominator = 0u;
    const int status = parse_exif_rational_tag(exif, exif_size, tag, &numerator, &denominator, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_numerator = 0u;
        *out_denominator = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_numerator = has_value ? numerator : 0u;
    *out_denominator = has_value ? denominator : 0u;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_rational_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint32_t* out_numerators,
    std::uint32_t* out_denominators,
    std::size_t out_value_count,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<std::uint32_t> numerators;
    std::vector<std::uint32_t> denominators;
    const int status = parse_exif_rational_array_tag(exif, exif_size, tag, &numerators, &denominators, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? numerators.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_numerators || !out_denominators) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_count < numerators.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_numerators, numerators.data(), numerators.size() * sizeof(std::uint32_t));
    std::memcpy(out_denominators, denominators.data(), denominators.size() * sizeof(std::uint32_t));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_signed_rational_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::int32_t* out_numerators,
    std::int32_t* out_denominators,
    std::size_t out_value_count,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<std::int32_t> numerators;
    std::vector<std::int32_t> denominators;
    const int status = parse_exif_signed_rational_array_tag(
        exif,
        exif_size,
        tag,
        &numerators,
        &denominators,
        &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? numerators.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_numerators || !out_denominators) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_count < numerators.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_numerators, numerators.data(), numerators.size() * sizeof(std::int32_t));
    std::memcpy(out_denominators, denominators.data(), denominators.size() * sizeof(std::int32_t));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_signed_rational_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::int32_t* out_numerator,
    std::int32_t* out_denominator)
{
    if (!out_has_tag || !out_numerator || !out_denominator) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::int32_t numerator = 0;
    std::int32_t denominator = 0;
    const int status = parse_exif_signed_rational_tag(exif, exif_size, tag, &numerator, &denominator, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_numerator = 0;
        *out_denominator = 0;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_numerator = has_value ? numerator : 0;
    *out_denominator = has_value ? denominator : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_ushort_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint32_t* out_values,
    std::size_t out_value_count,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<std::uint32_t> values;
    const int status = parse_exif_ushort_array_tag(exif, exif_size, tag, &values, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? values.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_count < values.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_values, values.data(), values.size() * sizeof(std::uint32_t));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_uint_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint32_t* out_values,
    std::size_t out_value_count,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<std::uint32_t> values;
    const int status = parse_exif_uint_array_tag(exif, exif_size, tag, &values, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? values.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_count < values.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_values, values.data(), values.size() * sizeof(std::uint32_t));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_double_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    double* out_values,
    std::size_t out_value_count,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<double> values;
    const int status = parse_exif_double_array_tag(exif, exif_size, tag, &values, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? values.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_count < values.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_values, values.data(), values.size() * sizeof(double));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_float_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    float* out_values,
    std::size_t out_value_count,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<float> values;
    const int status = parse_exif_float_array_tag(exif, exif_size, tag, &values, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? values.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_count < values.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_values, values.data(), values.size() * sizeof(float));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_byte_array_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint8_t* out_values,
    std::size_t out_value_size,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<std::uint8_t> values;
    const int status = parse_exif_byte_array_tag(exif, exif_size, tag, &values, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? values.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_size < values.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_values, values.data(), values.size());
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_exif_undefined_tag(
    const std::uint8_t* exif,
    std::size_t exif_size,
    int tag,
    int* out_has_tag,
    std::uint8_t* out_values,
    std::size_t out_value_size,
    std::size_t* out_value_required)
{
    if (!out_has_tag || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    bool has_value = false;
    std::vector<std::uint8_t> values;
    const int status = parse_exif_undefined_tag(exif, exif_size, tag, &values, &has_value);
    if (status != PILLOW_C_OK) {
        *out_has_tag = 0;
        *out_value_required = 0u;
        return status;
    }
    *out_has_tag = has_value ? 1 : 0;
    *out_value_required = has_value ? values.size() : 0u;
    if (!has_value) {
        return PILLOW_C_OK;
    }
    if (!out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_value_size < values.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_values, values.data(), values.size());
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_resolution(
    const PillowCImage* image,
    int* out_has_dpi,
    double* out_dpi_x,
    double* out_dpi_y,
    int* out_jfif,
    int* out_jfif_major,
    int* out_jfif_minor,
    int* out_jfif_unit,
    int* out_jfif_density_x,
    int* out_jfif_density_y)
{
    if (!image || !out_has_dpi || !out_dpi_x || !out_dpi_y || !out_jfif || !out_jfif_major || !out_jfif_minor ||
        !out_jfif_unit || !out_jfif_density_x || !out_jfif_density_y) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_dpi = image->has_dpi ? 1 : 0;
    *out_dpi_x = image->dpi_x;
    *out_dpi_y = image->dpi_y;
    *out_jfif = image->has_jfif ? ((image->jfif_major << 8) | image->jfif_minor) : 0;
    *out_jfif_major = image->has_jfif ? image->jfif_major : 0;
    *out_jfif_minor = image->has_jfif ? image->jfif_minor : 0;
    *out_jfif_unit = image->has_jfif ? image->jfif_unit : -1;
    *out_jfif_density_x = image->has_jfif ? image->jfif_density_x : 0;
    *out_jfif_density_y = image->has_jfif ? image->jfif_density_y : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_hotspot(
    const PillowCImage* image,
    int* out_has_hotspot,
    int* out_hotspot_x,
    int* out_hotspot_y)
{
    if (!image || !out_has_hotspot || !out_hotspot_x || !out_hotspot_y) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_hotspot = image->has_hotspot ? 1 : 0;
    *out_hotspot_x = image->has_hotspot ? image->hotspot_x : 0;
    *out_hotspot_y = image->has_hotspot ? image->hotspot_y : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_dib_compression(
    const PillowCImage* image,
    int* out_has_compression,
    int* out_compression)
{
    if (!image || !out_has_compression || !out_compression) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_compression = image->has_dib_compression ? 1 : 0;
    *out_compression = image->has_dib_compression ? image->dib_compression : -1;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_gamma(
    const PillowCImage* image,
    int* out_has_gamma,
    double* out_gamma)
{
    if (!image || !out_has_gamma || !out_gamma) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_gamma = image->has_png_gamma ? 1 : 0;
    *out_gamma = image->png_gamma;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_srgb(
    const PillowCImage* image,
    int* out_has_srgb,
    int* out_srgb)
{
    if (!image || !out_has_srgb || !out_srgb) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_srgb = image->has_png_srgb ? 1 : 0;
    *out_srgb = image->has_png_srgb ? image->png_srgb : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_chromaticity(
    const PillowCImage* image,
    int* out_has_chromaticity,
    double* out_values,
    std::size_t value_count)
{
    if (!image || !out_has_chromaticity || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (value_count < 8u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    *out_has_chromaticity = image->has_png_chromaticity ? 1 : 0;
    for (std::size_t i = 0; i < 8u; ++i) {
        out_values[i] = image->png_chromaticity[i];
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_text_count(
    const PillowCImage* image,
    std::size_t* out_count)
{
    if (!image || !out_count) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_count = image->png_text.size();
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_text(
    const PillowCImage* image,
    std::size_t index,
    char* out_key,
    std::size_t out_key_size,
    std::size_t* out_key_required,
    char* out_value,
    std::size_t out_value_size,
    std::size_t* out_value_required)
{
    if (!image || !out_key_required || !out_value_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (index >= image->png_text.size()) {
        *out_key_required = 0;
        *out_value_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const auto& item = image->png_text[index];
    const std::size_t key_required = item.first.size() + 1u;
    const std::size_t value_required = item.second.size() + 1u;
    *out_key_required = key_required;
    *out_value_required = value_required;
    if (!out_key || !out_value) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_key_size < key_required || out_value_size < value_required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_key, item.first.c_str(), key_required);
    std::memcpy(out_value, item.second.c_str(), value_required);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_icc_profile(
    const PillowCImage* image,
    int* out_has_profile,
    std::uint8_t* out_profile,
    std::size_t out_profile_size,
    std::size_t* out_profile_required)
{
    if (!image || !out_has_profile || !out_profile_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t required = image->png_icc_profile.size();
    *out_has_profile = required > 0u ? 1 : 0;
    *out_profile_required = required;
    if (required == 0u) {
        return PILLOW_C_OK;
    }
    if (!out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_profile_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_profile, image->png_icc_profile.data(), required);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_exif(
    const PillowCImage* image,
    int* out_has_exif,
    std::uint8_t* out_exif,
    std::size_t out_exif_size,
    std::size_t* out_exif_required)
{
    if (!image || !out_has_exif || !out_exif_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t required = image->png_exif.size();
    *out_has_exif = required > 0u ? 1 : 0;
    *out_exif_required = required;
    if (required == 0u) {
        return PILLOW_C_OK;
    }
    if (!out_exif) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_exif_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_exif, image->png_exif.data(), required);
    return PILLOW_C_OK;
}

int copy_metadata_blob(
    const std::vector<std::uint8_t>& data,
    int* out_has_blob,
    std::uint8_t* out_blob,
    std::size_t out_blob_size,
    std::size_t* out_blob_required)
{
    return copy_metadata_blob(
        data, !data.empty(), out_has_blob, out_blob, out_blob_size, out_blob_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_xmp(
    const PillowCImage* image,
    int* out_has_xmp,
    std::uint8_t* out_xmp,
    std::size_t out_xmp_size,
    std::size_t* out_xmp_required)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    return copy_metadata_blob(image->xmp, out_has_xmp, out_xmp, out_xmp_size, out_xmp_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_transparency(
    const PillowCImage* image,
    int* out_has_transparency,
    int* out_transparency)
{
    if (!image || !out_has_transparency || !out_transparency) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_transparency = image->has_png_transparency ? 1 : 0;
    *out_transparency = image->has_png_transparency ? image->png_transparency : -1;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_transparency_table(
    const PillowCImage* image,
    int* out_has_transparency,
    std::uint8_t* out_transparency,
    std::size_t out_transparency_size,
    std::size_t* out_transparency_required)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    return copy_metadata_blob(
        image->png_transparency_table,
        out_has_transparency,
        out_transparency,
        out_transparency_size,
        out_transparency_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_metadata_png_rgb_transparency(
    const PillowCImage* image,
    int* out_has_transparency,
    int* out_r,
    int* out_g,
    int* out_b)
{
    if (!image || !out_has_transparency || !out_r || !out_g || !out_b) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_has_transparency = image->has_png_rgb_transparency ? 1 : 0;
    *out_r = image->has_png_rgb_transparency ? image->png_rgb_transparency[0] : -1;
    *out_g = image->has_png_rgb_transparency ? image->png_rgb_transparency[1] : -1;
    *out_b = image->has_png_rgb_transparency ? image->png_rgb_transparency[2] : -1;
    return PILLOW_C_OK;
}

