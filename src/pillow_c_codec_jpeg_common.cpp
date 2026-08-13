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

#include "pillow_c_codec_jpeg_internal.h"
#include "pillow_c_internal.h"

namespace pillow_c_jpeg {
bool pillow_round_to_i64(double value, std::int64_t *out_value) {
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
bool jpeg_standalone_marker(std::uint8_t marker) {
  return marker == 0xd8u || marker == 0xd9u ||
         (marker >= 0xd0u && marker <= 0xd7u);
}
int jpeg_density_from_dpi(double dpi_x, double dpi_y, std::uint8_t *out_unit,
                          std::uint16_t *out_x, std::uint16_t *out_y) {
  if (!out_unit || !out_x || !out_y) {
    return PILLOW_C_NULL_POINTER;
  }
  std::int64_t rounded_x = 0;
  std::int64_t rounded_y = 0;
  if (!pillow_round_to_i64(dpi_x, &rounded_x) ||
      !pillow_round_to_i64(dpi_y, &rounded_y)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (rounded_x <= 0 || rounded_y <= 0) {
    *out_unit = 0;
    *out_x = 1;
    *out_y = 1;
    return PILLOW_C_OK;
  }
  *out_unit = 1;
  *out_x = static_cast<std::uint16_t>(rounded_x);
  *out_y = static_cast<std::uint16_t>(rounded_y);
  return PILLOW_C_OK;
}
int patch_jpeg_jfif_density(const char *path, double dpi_x, double dpi_y) {
  std::uint8_t unit = 0;
  std::uint16_t x_density = 0;
  std::uint16_t y_density = 0;
  int status =
      jpeg_density_from_dpi(dpi_x, dpi_y, &unit, &x_density, &y_density);
  if (status != PILLOW_C_OK) {
    return status;
  }
  try {
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data) || data.size() < 2u ||
        data[0] != 0xffu || data[1] != 0xd8u) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t pos = 2u;
    while (pos + 4u <= data.size()) {
      if (data[pos] != 0xffu) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      const std::uint8_t marker = data[pos + 1u];
      pos += 2u;
      if (marker == 0xd9u || marker == 0xdau) {
        break;
      }
      if (jpeg_standalone_marker(marker)) {
        continue;
      }
      if (pos + 2u > data.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      const std::uint16_t length = read_be16(data.data() + pos);
      if (length < 2u || pos + static_cast<std::size_t>(length) > data.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      const std::size_t payload = pos + 2u;
      const std::size_t payload_size = static_cast<std::size_t>(length) - 2u;
      if (marker == 0xe0u && payload_size >= 14u &&
          std::memcmp(data.data() + payload, "JFIF\0", 5u) == 0) {
        data[payload + 7u] = unit;
        data[payload + 8u] =
            static_cast<std::uint8_t>((x_density >> 8) & 0xffu);
        data[payload + 9u] = static_cast<std::uint8_t>(x_density & 0xffu);
        data[payload + 10u] =
            static_cast<std::uint8_t>((y_density >> 8) & 0xffu);
        data[payload + 11u] = static_cast<std::uint8_t>(y_density & 0xffu);
        return write_binary_file(path, data) ? PILLOW_C_OK
                                             : PILLOW_C_INVALID_ARGUMENT;
      }
      pos += length;
    }
    std::vector<std::uint8_t> app0;
    app0.push_back(0xffu);
    app0.push_back(0xe0u);
    append_be16(app0, 16u);
    app0.insert(app0.end(), {'J', 'F', 'I', 'F', 0, 1, 1, unit});
    append_be16(app0, x_density);
    append_be16(app0, y_density);
    app0.push_back(0);
    app0.push_back(0);
    data.insert(data.begin() + 2, app0.begin(), app0.end());
    return write_binary_file(path, data) ? PILLOW_C_OK
                                         : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int append_jpeg_segment(std::vector<std::uint8_t> &out, std::uint8_t marker,
                        const std::uint8_t *payload, std::size_t payload_size) {
  if (payload_size >
      static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) -
          2u) {
    return PILLOW_C_INVALID_LENGTH;
  }
  if (payload_size > 0u && !payload) {
    return PILLOW_C_NULL_POINTER;
  }
  out.push_back(0xffu);
  out.push_back(marker);
  append_be16(out, static_cast<std::uint16_t>(payload_size + 2u));
  if (payload_size > 0u) {
    out.insert(out.end(), payload, payload + payload_size);
  }
  return PILLOW_C_OK;
}
int append_jpeg_icc_segment(std::vector<std::uint8_t> &out,
                            const std::uint8_t *profile,
                            std::size_t profile_size) {
  if (profile_size > 0u && !profile) {
    return PILLOW_C_NULL_POINTER;
  }
  if (profile_size == 0u) {
    return PILLOW_C_OK;
  }
  static constexpr std::uint8_t icc_header_prefix[] = {
      'I', 'C', 'C', '_', 'P', 'R', 'O', 'F', 'I', 'L', 'E', 0};
  static constexpr std::size_t max_payload_size =
      static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) - 2u;
  static constexpr std::size_t icc_segment_header_size =
      sizeof(icc_header_prefix) + 2u;
  static constexpr std::size_t max_profile_chunk_size =
      max_payload_size - icc_segment_header_size;
  static constexpr std::size_t max_profile_size = max_profile_chunk_size * 255u;
  if (profile_size > max_profile_size) {
    return PILLOW_C_INVALID_LENGTH;
  }
  const std::size_t segment_count =
      (profile_size + max_profile_chunk_size - 1u) / max_profile_chunk_size;
  if (segment_count == 0u || segment_count > 255u) {
    return PILLOW_C_INVALID_LENGTH;
  }
  std::size_t offset = 0u;
  for (std::size_t index = 0u; index < segment_count; ++index) {
    const std::size_t chunk_size =
        std::min(max_profile_chunk_size, profile_size - offset);
    const std::size_t payload_size = icc_segment_header_size + chunk_size;
    out.push_back(0xffu);
    out.push_back(0xe2u);
    append_be16(out, static_cast<std::uint16_t>(payload_size + 2u));
    out.insert(out.end(), icc_header_prefix,
               icc_header_prefix + sizeof(icc_header_prefix));
    out.push_back(static_cast<std::uint8_t>(index + 1u));
    out.push_back(static_cast<std::uint8_t>(segment_count));
    out.insert(out.end(), profile + offset, profile + offset + chunk_size);
    offset += chunk_size;
  }
  return PILLOW_C_OK;
}
int append_jpeg_xmp_segment(std::vector<std::uint8_t> &out,
                            const std::uint8_t *xmp, std::size_t xmp_size) {
  if (xmp_size > 0u && !xmp) {
    return PILLOW_C_NULL_POINTER;
  }
  if (xmp_size == 0u) {
    return PILLOW_C_OK;
  }
  static constexpr std::uint8_t xmp_header[] = {
      'h', 't', 't', 'p', ':', '/', '/', 'n', 's', '.', 'a', 'd', 'o', 'b', 'e',
      '.', 'c', 'o', 'm', '/', 'x', 'a', 'p', '/', '1', '.', '0', '/', 0};
  const std::size_t payload_size = sizeof(xmp_header) + xmp_size;
  if (payload_size >
      static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) -
          2u) {
    return PILLOW_C_INVALID_LENGTH;
  }
  out.push_back(0xffu);
  out.push_back(0xe1u);
  append_be16(out, static_cast<std::uint16_t>(payload_size + 2u));
  out.insert(out.end(), xmp_header, xmp_header + sizeof(xmp_header));
  out.insert(out.end(), xmp, xmp + xmp_size);
  return PILLOW_C_OK;
}
int build_jpeg_metadata_segments(
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, std::vector<std::uint8_t> *out_segments) {
  if (!out_segments) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  int status = PILLOW_C_OK;
  try {
    if (exif_size > 0u) {
      status = append_jpeg_segment(*out_segments, 0xe1u, exif, exif_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (xmp_size > 0u) {
      status = append_jpeg_xmp_segment(*out_segments, xmp, xmp_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (icc_profile_size > 0u) {
      status =
          append_jpeg_icc_segment(*out_segments, icc_profile, icc_profile_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (comment_size > 0u) {
      status = append_jpeg_segment(*out_segments, 0xfeu, comment, comment_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
  return PILLOW_C_OK;
}
int jpeg_metadata_insert_position(const std::vector<std::uint8_t> &data,
                                  std::size_t *out_position) {
  if (!out_position) {
    return PILLOW_C_NULL_POINTER;
  }
  if (data.size() < 2u || data[0] != 0xffu || data[1] != 0xd8u) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  *out_position = 2u;
  std::size_t pos = 2u;
  while (pos + 4u <= data.size() && data[pos] == 0xffu) {
    const std::uint8_t marker = data[pos + 1u];
    if (!(marker == 0xe0u || marker == 0xeeu)) {
      break;
    }
    const std::uint16_t length = read_be16(data.data() + pos + 2u);
    if (length < 2u ||
        pos + 2u + static_cast<std::size_t>(length) > data.size()) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t payload = pos + 4u;
    const std::size_t payload_size = static_cast<std::size_t>(length) - 2u;
    if ((marker == 0xe0u && payload_size >= 5u &&
         std::memcmp(data.data() + payload, "JFIF\0", 5u) == 0) ||
        (marker == 0xeeu && payload_size >= 6u &&
         std::memcmp(data.data() + payload, "Adobe", 5u) == 0 &&
         data[payload + 5u] == 0u)) {
      *out_position = pos + 2u + static_cast<std::size_t>(length);
      pos = *out_position;
      continue;
    }
    break;
  }
  return PILLOW_C_OK;
}
int patch_jpeg_metadata_segments(
    const char *path, const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size,
    const std::uint8_t *xmp = nullptr, std::size_t xmp_size = 0u) {
  if (!path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (comment_size == 0u && icc_profile_size == 0u && exif_size == 0u &&
      xmp_size == 0u) {
    return PILLOW_C_OK;
  }
  try {
    std::vector<std::uint8_t> segments;
    int status = build_jpeg_metadata_segments(
        comment, comment_size, icc_profile, icc_profile_size, exif, exif_size,
        xmp, xmp_size, &segments);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t insert_position = 0u;
    status = jpeg_metadata_insert_position(data, &insert_position);
    if (status != PILLOW_C_OK) {
      return status;
    }
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(insert_position),
                segments.begin(), segments.end());
    return write_binary_file(path, data) ? PILLOW_C_OK
                                         : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int patch_jpeg_source_comment_segment(const PillowCImage *image,
                                      const char *path) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->jpeg_comment.empty()) {
    return PILLOW_C_OK;
  }
  return patch_jpeg_metadata_segments(path, image->jpeg_comment.data(),
                                      image->jpeg_comment.size(), nullptr, 0u,
                                      nullptr, 0u);
}
int patch_jpeg_extra_segments(const char *path, const std::uint8_t *extra,
                              std::size_t extra_size) {
  if (!path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (extra_size == 0u) {
    return PILLOW_C_OK;
  }
  if (!extra) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t insert_position = 0u;
    int status = jpeg_metadata_insert_position(data, &insert_position);
    if (status != PILLOW_C_OK) {
      return status;
    }
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(insert_position),
                extra, extra + extra_size);
    return write_binary_file(path, data) ? PILLOW_C_OK
                                         : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int patch_jpeg_metadata_extra_segments(
    const char *path, const std::uint8_t *extra, std::size_t extra_size,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size) {
  if (!path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((extra_size > 0u && !extra) ||
      (comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) ||
      (exif_size > 0u && !exif) || (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  if (extra_size == 0u && comment_size == 0u && icc_profile_size == 0u &&
      exif_size == 0u && xmp_size == 0u) {
    return PILLOW_C_OK;
  }
  try {
    std::vector<std::uint8_t> segments;
    int status = PILLOW_C_OK;
    if (exif_size > 0u) {
      status = append_jpeg_segment(segments, 0xe1u, exif, exif_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (extra_size > 0u) {
      segments.insert(segments.end(), extra, extra + extra_size);
    }
    if (xmp_size > 0u) {
      status = append_jpeg_xmp_segment(segments, xmp, xmp_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (icc_profile_size > 0u) {
      status = append_jpeg_icc_segment(segments, icc_profile, icc_profile_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (comment_size > 0u) {
      status = append_jpeg_segment(segments, 0xfeu, comment, comment_size);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> data;
    if (!read_binary_file(path, &data)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t insert_position = 0u;
    status = jpeg_metadata_insert_position(data, &insert_position);
    if (status != PILLOW_C_OK) {
      return status;
    }
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(insert_position),
                segments.begin(), segments.end());
    return write_binary_file(path, data) ? PILLOW_C_OK
                                         : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
constexpr int JPEG_LUMINANCE_QTABLE[64] = {
    16, 11, 10, 16, 24,  40,  51,  61,  12, 12, 14, 19, 26,  58,  60,  55,
    14, 13, 16, 24, 40,  57,  69,  56,  14, 17, 22, 29, 51,  87,  80,  62,
    18, 22, 37, 56, 68,  109, 103, 77,  24, 35, 55, 64, 81,  104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};
constexpr int JPEG_CHROMINANCE_QTABLE[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
int jpeg_value_category(int value) {
  int magnitude = value < 0 ? -value : value;
  int category = 0;
  while (magnitude != 0) {
    magnitude >>= 1;
    ++category;
  }
  return category;
}
std::uint16_t jpeg_value_bits(int value, int category) {
  if (category == 0) {
    return 0;
  }
  if (value >= 0) {
    return static_cast<std::uint16_t>(value);
  }
  return static_cast<std::uint16_t>(value + ((1 << category) - 1));
}
void jpeg_scaled_qtable(const int base_qtable[64], int quality,
                        int out_qtable[64]) {
  int q = quality == -1 ? 75 : quality;
  q = std::max(1, std::min(q, 100));
  const int scale = q < 50 ? 5000 / q : 200 - q * 2;
  for (int i = 0; i < 64; ++i) {
    int value = (base_qtable[i] * scale + 50) / 100;
    value = std::max(1, std::min(value, 255));
    out_qtable[i] = value;
  }
}
void jpeg_scaled_luminance_qtable(int quality, int out_qtable[64]) {
  jpeg_scaled_qtable(JPEG_LUMINANCE_QTABLE, quality, out_qtable);
}
void jpeg_scaled_chrominance_qtable(int quality, int out_qtable[64]) {
  jpeg_scaled_qtable(JPEG_CHROMINANCE_QTABLE, quality, out_qtable);
}
int jpeg_scaled_custom_qtable(const int *source_qtable, int quality,
                              int out_qtable[64]) {
  if (!source_qtable || !out_qtable) {
    return PILLOW_C_NULL_POINTER;
  }
  if (quality == -1) {
    for (int i = 0; i < 64; ++i) {
      const int source = source_qtable[i];
      if (source < 1 || source > 255) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      out_qtable[i] = source;
    }
    return PILLOW_C_OK;
  }
  int q = quality == -1 ? 75 : quality;
  q = std::max(1, std::min(q, 100));
  const int scale = q < 50 ? 5000 / q : 200 - q * 2;
  for (int i = 0; i < 64; ++i) {
    const int source = source_qtable[i];
    if (source < 1 || source > 255) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    int value = (source * scale + 50) / 100;
    value = std::max(1, std::min(value, 255));
    out_qtable[i] = value;
  }
  return PILLOW_C_OK;
}
std::int64_t jpeg_arithmetic_right_shift(std::int64_t value, int bits) {
  if (value >= 0) {
    return value >> bits;
  }
  return -1 - ((-1 - value) >> bits);
}
std::int64_t jpeg_descale(std::int64_t value, int bits) {
  return jpeg_arithmetic_right_shift(
      value + (static_cast<std::int64_t>(1) << (bits - 1)), bits);
}
void jpeg_fdct_quantize_samples(const int samples[64], const int qtable[64],
                                int out_zz[64]) {
  constexpr int const_bits = 13;
  constexpr int pass1_bits = 2;
  constexpr std::int64_t fix_0_298631336 = 2446;
  constexpr std::int64_t fix_0_390180644 = 3196;
  constexpr std::int64_t fix_0_541196100 = 4433;
  constexpr std::int64_t fix_0_765366865 = 6270;
  constexpr std::int64_t fix_0_899976223 = 7373;
  constexpr std::int64_t fix_1_175875602 = 9633;
  constexpr std::int64_t fix_1_501321110 = 12299;
  constexpr std::int64_t fix_1_847759065 = 15137;
  constexpr std::int64_t fix_1_961570560 = 16069;
  constexpr std::int64_t fix_2_053119869 = 16819;
  constexpr std::int64_t fix_2_562915447 = 20995;
  constexpr std::int64_t fix_3_072711026 = 25172;
  std::int64_t data[64] = {};
  for (int i = 0; i < 64; ++i) {
    data[i] = samples[i];
  }
  for (int row = 0; row < 8; ++row) {
    const int offset = row * 8;
    std::int64_t tmp0 = data[offset] + data[offset + 7];
    std::int64_t tmp7 = data[offset] - data[offset + 7];
    std::int64_t tmp1 = data[offset + 1] + data[offset + 6];
    std::int64_t tmp6 = data[offset + 1] - data[offset + 6];
    std::int64_t tmp2 = data[offset + 2] + data[offset + 5];
    std::int64_t tmp5 = data[offset + 2] - data[offset + 5];
    std::int64_t tmp3 = data[offset + 3] + data[offset + 4];
    std::int64_t tmp4 = data[offset + 3] - data[offset + 4];
    const std::int64_t tmp10 = tmp0 + tmp3;
    const std::int64_t tmp13 = tmp0 - tmp3;
    const std::int64_t tmp11 = tmp1 + tmp2;
    const std::int64_t tmp12 = tmp1 - tmp2;
    data[offset] = (tmp10 + tmp11) * (1 << pass1_bits);
    data[offset + 4] = (tmp10 - tmp11) * (1 << pass1_bits);
    std::int64_t z1 = (tmp12 + tmp13) * fix_0_541196100;
    data[offset + 2] =
        jpeg_descale(z1 + tmp13 * fix_0_765366865, const_bits - pass1_bits);
    data[offset + 6] =
        jpeg_descale(z1 - tmp12 * fix_1_847759065, const_bits - pass1_bits);
    z1 = tmp4 + tmp7;
    std::int64_t z2 = tmp5 + tmp6;
    std::int64_t z3 = tmp4 + tmp6;
    std::int64_t z4 = tmp5 + tmp7;
    const std::int64_t z5 = (z3 + z4) * fix_1_175875602;
    tmp4 *= fix_0_298631336;
    tmp5 *= fix_2_053119869;
    tmp6 *= fix_3_072711026;
    tmp7 *= fix_1_501321110;
    z1 *= -fix_0_899976223;
    z2 *= -fix_2_562915447;
    z3 = z3 * -fix_1_961570560 + z5;
    z4 = z4 * -fix_0_390180644 + z5;
    data[offset + 7] = jpeg_descale(tmp4 + z1 + z3, const_bits - pass1_bits);
    data[offset + 5] = jpeg_descale(tmp5 + z2 + z4, const_bits - pass1_bits);
    data[offset + 3] = jpeg_descale(tmp6 + z2 + z3, const_bits - pass1_bits);
    data[offset + 1] = jpeg_descale(tmp7 + z1 + z4, const_bits - pass1_bits);
  }
  for (int column = 0; column < 8; ++column) {
    std::int64_t tmp0 = data[column] + data[column + 56];
    std::int64_t tmp7 = data[column] - data[column + 56];
    std::int64_t tmp1 = data[column + 8] + data[column + 48];
    std::int64_t tmp6 = data[column + 8] - data[column + 48];
    std::int64_t tmp2 = data[column + 16] + data[column + 40];
    std::int64_t tmp5 = data[column + 16] - data[column + 40];
    std::int64_t tmp3 = data[column + 24] + data[column + 32];
    std::int64_t tmp4 = data[column + 24] - data[column + 32];
    const std::int64_t tmp10 = tmp0 + tmp3;
    const std::int64_t tmp13 = tmp0 - tmp3;
    const std::int64_t tmp11 = tmp1 + tmp2;
    const std::int64_t tmp12 = tmp1 - tmp2;
    data[column] = jpeg_descale(tmp10 + tmp11, pass1_bits);
    data[column + 32] = jpeg_descale(tmp10 - tmp11, pass1_bits);
    std::int64_t z1 = (tmp12 + tmp13) * fix_0_541196100;
    data[column + 16] =
        jpeg_descale(z1 + tmp13 * fix_0_765366865, const_bits + pass1_bits);
    data[column + 48] =
        jpeg_descale(z1 - tmp12 * fix_1_847759065, const_bits + pass1_bits);
    z1 = tmp4 + tmp7;
    std::int64_t z2 = tmp5 + tmp6;
    std::int64_t z3 = tmp4 + tmp6;
    std::int64_t z4 = tmp5 + tmp7;
    const std::int64_t z5 = (z3 + z4) * fix_1_175875602;
    tmp4 *= fix_0_298631336;
    tmp5 *= fix_2_053119869;
    tmp6 *= fix_3_072711026;
    tmp7 *= fix_1_501321110;
    z1 *= -fix_0_899976223;
    z2 *= -fix_2_562915447;
    z3 = z3 * -fix_1_961570560 + z5;
    z4 = z4 * -fix_0_390180644 + z5;
    data[column + 56] = jpeg_descale(tmp4 + z1 + z3, const_bits + pass1_bits);
    data[column + 40] = jpeg_descale(tmp5 + z2 + z4, const_bits + pass1_bits);
    data[column + 24] = jpeg_descale(tmp6 + z2 + z3, const_bits + pass1_bits);
    data[column + 8] = jpeg_descale(tmp7 + z1 + z4, const_bits + pass1_bits);
  }
  int natural[64] = {};
  for (int i = 0; i < 64; ++i) {
    const std::int64_t divisor = static_cast<std::int64_t>(qtable[i]) * 8;
    const bool negative = data[i] < 0;
    std::int64_t magnitude = negative ? -data[i] : data[i];
    magnitude += divisor / 2;
    const int quantized =
        magnitude >= divisor ? static_cast<int>(magnitude / divisor) : 0;
    natural[i] = negative ? -quantized : quantized;
  }
  for (int i = 0; i < 64; ++i) {
    out_zz[i] = natural[JPEG_ZIGZAG[i]];
  }
}
void jpeg_fdct_quantize_luma_block(const PillowCImage *image, int block_x,
                                   int block_y, const int qtable[64],
                                   int out_zz[64]) {
  int samples[64] = {};
  for (int y = 0; y < 8; ++y) {
    const int src_y = std::min(block_y + y, image->height - 1);
    const std::uint8_t *row =
        image->pixels.data() + static_cast<std::size_t>(src_y) * image->stride;
    for (int x = 0; x < 8; ++x) {
      const int src_x = std::min(block_x + x, image->width - 1);
      samples[y * 8 + x] = static_cast<int>(row[src_x]) - 128;
    }
  }
  jpeg_fdct_quantize_samples(samples, qtable, out_zz);
}
void jpeg_fdct_quantize_plane_block(const std::vector<std::uint8_t> &plane,
                                    int width, int height, int block_x,
                                    int block_y, const int qtable[64],
                                    int out_zz[64]) {
  int samples[64] = {};
  for (int y = 0; y < 8; ++y) {
    const int src_y = std::min(block_y + y, height - 1);
    const std::size_t row_offset =
        static_cast<std::size_t>(src_y) * static_cast<std::size_t>(width);
    for (int x = 0; x < 8; ++x) {
      const int src_x = std::min(block_x + x, width - 1);
      samples[y * 8 + x] =
          static_cast<int>(
              plane[row_offset + static_cast<std::size_t>(src_x)]) -
          128;
    }
  }
  jpeg_fdct_quantize_samples(samples, qtable, out_zz);
}
void jpeg_collect_huffman_frequencies(const std::vector<int> &blocks,
                                      std::uint64_t dc_freq[256],
                                      std::uint64_t ac_freq[256],
                                      std::size_t restart_interval_blocks) {
  int previous_dc = 0;
  const std::size_t block_count = blocks.size() / 64u;
  for (std::size_t block = 0; block < block_count; ++block) {
    if (restart_interval_blocks != 0u && block != 0u &&
        block % restart_interval_blocks == 0u) {
      previous_dc = 0;
    }
    const int *coeffs = blocks.data() + block * 64u;
    const int diff = coeffs[0] - previous_dc;
    previous_dc = coeffs[0];
    ++dc_freq[jpeg_value_category(diff)];
    int zero_run = 0;
    for (int i = 1; i < 64; ++i) {
      const int value = coeffs[i];
      if (value == 0) {
        ++zero_run;
        continue;
      }
      while (zero_run > 15) {
        ++ac_freq[0xf0u];
        zero_run -= 16;
      }
      const int category = jpeg_value_category(value);
      ++ac_freq[(zero_run << 4) | category];
      zero_run = 0;
    }
    if (zero_run > 0) {
      ++ac_freq[0x00u];
    }
  }
}
int jpeg_build_optimized_huffman_table(const std::uint64_t frequencies[256],
                                       JpegHuffmanTable *table) {
  if (!table) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    *table = JpegHuffmanTable{};
    constexpr int pseudo_symbol = 256;
    constexpr int max_code_length = 32;
    std::uint64_t frequency[257] = {};
    int code_size[257] = {};
    int others[257] = {};
    bool has_symbol = false;
    for (int symbol = 0; symbol < 256; ++symbol) {
      frequency[symbol] = frequencies[symbol];
      has_symbol = has_symbol || frequencies[symbol] != 0u;
      others[symbol] = -1;
    }
    others[pseudo_symbol] = -1;
    if (!has_symbol) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    frequency[pseudo_symbol] = 1u;
    for (;;) {
      int first = -1;
      std::uint64_t first_frequency = 0u;
      for (int symbol = 0; symbol <= pseudo_symbol; ++symbol) {
        if (frequency[symbol] != 0u &&
            (first < 0 || frequency[symbol] <= first_frequency)) {
          first = symbol;
          first_frequency = frequency[symbol];
        }
      }
      int second = -1;
      std::uint64_t second_frequency = 0u;
      for (int symbol = 0; symbol <= pseudo_symbol; ++symbol) {
        if (symbol != first && frequency[symbol] != 0u &&
            (second < 0 || frequency[symbol] <= second_frequency)) {
          second = symbol;
          second_frequency = frequency[symbol];
        }
      }
      if (second < 0) {
        break;
      }
      frequency[first] += frequency[second];
      frequency[second] = 0u;
      ++code_size[first];
      while (others[first] >= 0) {
        first = others[first];
        ++code_size[first];
      }
      others[first] = second;
      ++code_size[second];
      while (others[second] >= 0) {
        second = others[second];
        ++code_size[second];
      }
    }
    int bits[max_code_length + 1] = {};
    for (int symbol = 0; symbol <= pseudo_symbol; ++symbol) {
      if (code_size[symbol] > max_code_length) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      if (code_size[symbol] > 0) {
        ++bits[code_size[symbol]];
      }
    }
    for (int length = max_code_length; length > 16; --length) {
      while (bits[length] > 0) {
        int shorter = length - 2;
        while (shorter > 0 && bits[shorter] == 0) {
          --shorter;
        }
        if (shorter <= 0 || bits[length] < 2 || bits[shorter] <= 0) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
        bits[length] -= 2;
        ++bits[length - 1];
        bits[shorter + 1] += 2;
        --bits[shorter];
      }
    }
    int longest = 16;
    while (longest > 0 && bits[longest] == 0) {
      --longest;
    }
    if (longest == 0) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    --bits[longest];
    std::size_t symbol_count = 0u;
    for (int length = 1; length <= 16; ++length) {
      if (bits[length] < 0 || bits[length] > 255) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      table->counts[length] = static_cast<std::uint8_t>(bits[length]);
      symbol_count += static_cast<std::size_t>(bits[length]);
    }
    table->symbols.reserve(symbol_count);
    for (int length = 1; length <= max_code_length; ++length) {
      for (int symbol = 0; symbol < 256; ++symbol) {
        if (code_size[symbol] == length &&
            table->symbols.size() < symbol_count) {
          table->symbols.push_back(static_cast<std::uint8_t>(symbol));
        }
      }
    }
    if (table->symbols.size() != symbol_count) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::uint32_t code = 0u;
    std::size_t symbol_index = 0u;
    for (int length = 1; length <= 16; ++length) {
      for (int index = 0; index < bits[length]; ++index) {
        if (symbol_index >= table->symbols.size() || code >= (1u << length)) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint8_t symbol = table->symbols[symbol_index++];
        table->codes[symbol] = static_cast<std::uint16_t>(code);
        table->sizes[symbol] = static_cast<std::uint8_t>(length);
        ++code;
      }
      if (length < 16) {
        code <<= 1;
      }
    }
    if (symbol_index != table->symbols.size()) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
constexpr std::uint8_t JPEG_STD_LUMA_DC_COUNTS[16] = {0, 1, 5, 1, 1, 1, 1, 1,
                                                      1, 0, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t JPEG_STD_LUMA_DC_SYMBOLS[12] = {0, 1, 2, 3, 4,  5,
                                                       6, 7, 8, 9, 10, 11};
constexpr std::uint8_t JPEG_STD_LUMA_AC_COUNTS[16] = {0, 2, 1, 3, 3, 2, 4, 3,
                                                      5, 5, 4, 4, 0, 0, 1, 125};
constexpr std::uint8_t JPEG_STD_LUMA_AC_SYMBOLS[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06,
    0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72,
    0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
    0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
    0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4,
    0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};
constexpr std::uint8_t JPEG_STD_CHROMA_DC_COUNTS[16] = {0, 3, 1, 1, 1, 1, 1, 1,
                                                        1, 1, 1, 0, 0, 0, 0, 0};
constexpr std::uint8_t JPEG_STD_CHROMA_DC_SYMBOLS[12] = {0, 1, 2, 3, 4,  5,
                                                         6, 7, 8, 9, 10, 11};
constexpr std::uint8_t JPEG_STD_CHROMA_AC_COUNTS[16] = {
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119};
constexpr std::uint8_t JPEG_STD_CHROMA_AC_SYMBOLS[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41,
    0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1,
    0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74,
    0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
    0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4,
    0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};
int jpeg_build_huffman_table_from_spec(const std::uint8_t counts[16],
                                       const std::uint8_t *symbols,
                                       std::size_t symbol_count,
                                       JpegHuffmanTable *table) {
  if (!counts || !symbols || !table) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    *table = JpegHuffmanTable{};
    std::size_t expected = 0;
    for (int length = 1; length <= 16; ++length) {
      table->counts[length] = counts[length - 1];
      expected += counts[length - 1];
    }
    if (expected != symbol_count) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    table->symbols.assign(symbols, symbols + symbol_count);
    std::uint16_t code = 0;
    std::size_t symbol_index = 0;
    for (int length = 1; length <= 16; ++length) {
      const int count = table->counts[length];
      for (int i = 0; i < count; ++i) {
        const std::uint8_t symbol = table->symbols[symbol_index++];
        table->codes[symbol] = code;
        table->sizes[symbol] = static_cast<std::uint8_t>(length);
        ++code;
      }
      code = static_cast<std::uint16_t>(code << 1);
    }
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_build_standard_luminance_huffman_tables(JpegHuffmanTable *dc_table,
                                                 JpegHuffmanTable *ac_table) {
  int status = jpeg_build_huffman_table_from_spec(
      JPEG_STD_LUMA_DC_COUNTS, JPEG_STD_LUMA_DC_SYMBOLS,
      sizeof(JPEG_STD_LUMA_DC_SYMBOLS), dc_table);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return jpeg_build_huffman_table_from_spec(
      JPEG_STD_LUMA_AC_COUNTS, JPEG_STD_LUMA_AC_SYMBOLS,
      sizeof(JPEG_STD_LUMA_AC_SYMBOLS), ac_table);
}
int jpeg_build_standard_chrominance_huffman_tables(JpegHuffmanTable *dc_table,
                                                   JpegHuffmanTable *ac_table) {
  int status = jpeg_build_huffman_table_from_spec(
      JPEG_STD_CHROMA_DC_COUNTS, JPEG_STD_CHROMA_DC_SYMBOLS,
      sizeof(JPEG_STD_CHROMA_DC_SYMBOLS), dc_table);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return jpeg_build_huffman_table_from_spec(
      JPEG_STD_CHROMA_AC_COUNTS, JPEG_STD_CHROMA_AC_SYMBOLS,
      sizeof(JPEG_STD_CHROMA_AC_SYMBOLS), ac_table);
}
int jpeg_append_dht_segment(std::vector<std::uint8_t> &out, int table_class,
                            int table_id, const JpegHuffmanTable &table) {
  std::vector<std::uint8_t> payload;
  try {
    payload.push_back(static_cast<std::uint8_t>((table_class << 4) | table_id));
    for (int i = 1; i <= 16; ++i) {
      payload.push_back(table.counts[i]);
    }
    payload.insert(payload.end(), table.symbols.begin(), table.symbols.end());
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
  return append_jpeg_segment(out, 0xc4u, payload.data(), payload.size());
}
void jpeg_bit_writer_emit_byte(JpegBitWriter *writer, std::uint8_t value) {
  writer->out->push_back(value);
  if (value == 0xffu) {
    writer->out->push_back(0u);
  }
}
void jpeg_bit_writer_write(JpegBitWriter *writer, std::uint16_t code,
                           int size) {
  for (int bit = size - 1; bit >= 0; --bit) {
    writer->current = static_cast<std::uint8_t>((writer->current << 1) |
                                                ((code >> bit) & 1u));
    ++writer->used;
    if (writer->used == 8) {
      jpeg_bit_writer_emit_byte(writer, writer->current);
      writer->current = 0;
      writer->used = 0;
    }
  }
}
void jpeg_bit_writer_flush(JpegBitWriter *writer) {
  if (writer->used == 0) {
    return;
  }
  const int remaining = 8 - writer->used;
  const std::uint8_t fill = static_cast<std::uint8_t>((1u << remaining) - 1u);
  const std::uint8_t value =
      static_cast<std::uint8_t>((writer->current << remaining) | fill);
  jpeg_bit_writer_emit_byte(writer, value);
  writer->current = 0;
  writer->used = 0;
}
int jpeg_write_huffman_symbol(JpegBitWriter *writer,
                              const JpegHuffmanTable &table, int symbol) {
  if (symbol < 0 || symbol > 255 || table.sizes[symbol] == 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  jpeg_bit_writer_write(writer, table.codes[symbol], table.sizes[symbol]);
  return PILLOW_C_OK;
}
int jpeg_encode_block_entropy(JpegBitWriter *writer, const int coeffs[64],
                              int *previous_dc,
                              const JpegHuffmanTable &dc_table,
                              const JpegHuffmanTable &ac_table) {
  if (!writer || !previous_dc) {
    return PILLOW_C_NULL_POINTER;
  }
  const int diff = coeffs[0] - *previous_dc;
  *previous_dc = coeffs[0];
  int category = jpeg_value_category(diff);
  int status = jpeg_write_huffman_symbol(writer, dc_table, category);
  if (status != PILLOW_C_OK) {
    return status;
  }
  jpeg_bit_writer_write(writer, jpeg_value_bits(diff, category), category);
  int zero_run = 0;
  for (int i = 1; i < 64; ++i) {
    const int value = coeffs[i];
    if (value == 0) {
      ++zero_run;
      continue;
    }
    while (zero_run > 15) {
      status = jpeg_write_huffman_symbol(writer, ac_table, 0xf0);
      if (status != PILLOW_C_OK) {
        return status;
      }
      zero_run -= 16;
    }
    category = jpeg_value_category(value);
    const int symbol = (zero_run << 4) | category;
    status = jpeg_write_huffman_symbol(writer, ac_table, symbol);
    if (status != PILLOW_C_OK) {
      return status;
    }
    jpeg_bit_writer_write(writer, jpeg_value_bits(value, category), category);
    zero_run = 0;
  }
  if (zero_run > 0) {
    status = jpeg_write_huffman_symbol(writer, ac_table, 0x00);
    if (status != PILLOW_C_OK) {
      return status;
    }
  }
  return PILLOW_C_OK;
}
int jpeg_encode_luma_entropy(const std::vector<int> &blocks,
                             const JpegHuffmanTable &dc_table,
                             const JpegHuffmanTable &ac_table,
                             std::uint16_t restart_interval,
                             std::vector<std::uint8_t> *out) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int previous_dc = 0;
    int restart_index = 0;
    const std::size_t block_count = blocks.size() / 64u;
    for (std::size_t block = 0; block < block_count; ++block) {
      if (restart_interval != 0u && block != 0u &&
          block % restart_interval == 0u) {
        jpeg_bit_writer_flush(&writer);
        out->push_back(0xffu);
        out->push_back(static_cast<std::uint8_t>(0xd0u + (restart_index & 7)));
        ++restart_index;
        previous_dc = 0;
      }
      const int *coeffs = blocks.data() + block * 64u;
      const int status = jpeg_encode_block_entropy(
          &writer, coeffs, &previous_dc, dc_table, ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_successive_value(int value, int al) {
  const int magnitude = value < 0 ? -value : value;
  const int shifted = magnitude >> al;
  return value < 0 ? -shifted : shifted;
}
int jpeg_successive_dc_value(int value, int al) { return value >> al; }
int jpeg_successive_bit(int value, int al) {
  const int magnitude = value < 0 ? -value : value;
  return (magnitude >> al) & 1;
}
int jpeg_append_dri_segment(std::vector<std::uint8_t> &out,
                            std::uint16_t restart_interval) {
  const std::uint8_t dri[] = {
      static_cast<std::uint8_t>((restart_interval >> 8) & 0xffu),
      static_cast<std::uint8_t>(restart_interval & 0xffu)};
  return append_jpeg_segment(out, 0xddu, dri, sizeof(dri));
}
void jpeg_write_restart_marker(JpegBitWriter *writer, int *restart_index) {
  jpeg_bit_writer_flush(writer);
  writer->out->push_back(0xffu);
  writer->out->push_back(
      static_cast<std::uint8_t>(0xd0u + (*restart_index & 7)));
  ++*restart_index;
}
void jpeg_collect_progressive_dc_first_frequencies(
    const std::vector<int> &blocks, int al, std::uint64_t dc_freq[256],
    std::size_t restart_interval_blocks) {
  int previous_dc = 0;
  const std::size_t block_count = blocks.size() / 64u;
  for (std::size_t block = 0; block < block_count; ++block) {
    if (restart_interval_blocks != 0u && block != 0u &&
        block % restart_interval_blocks == 0u) {
      previous_dc = 0;
    }
    const int *coeffs = blocks.data() + block * 64u;
    const int dc = jpeg_successive_dc_value(coeffs[0], al);
    const int diff = dc - previous_dc;
    previous_dc = dc;
    ++dc_freq[jpeg_value_category(diff)];
  }
}
constexpr std::uint32_t JPEG_PROGRESSIVE_MAX_EOB_RUN = 0x7fffu;
constexpr std::size_t JPEG_PROGRESSIVE_MAX_CORRECTION_BITS = 1000u;
int jpeg_progressive_eob_run_symbol(std::uint32_t eob_run) {
  int nbits = 0;
  while ((eob_run >>= 1u) != 0u) {
    ++nbits;
  }
  return nbits << 4;
}
void jpeg_collect_progressive_ac_first_frequencies(
    const std::vector<int> &blocks, int ss, int se, int al,
    std::uint64_t ac_freq[256], std::size_t restart_interval_blocks) {
  std::uint32_t eob_run = 0u;
  const auto flush_eob_run = [&]() {
    if (eob_run == 0u) {
      return;
    }
    ++ac_freq[jpeg_progressive_eob_run_symbol(eob_run)];
    eob_run = 0u;
  };
  const std::size_t block_count = blocks.size() / 64u;
  for (std::size_t block = 0; block < block_count; ++block) {
    if (restart_interval_blocks != 0u && block != 0u &&
        block % restart_interval_blocks == 0u) {
      flush_eob_run();
    }
    const int *coeffs = blocks.data() + block * 64u;
    int zero_run = 0;
    for (int k = ss; k <= se; ++k) {
      const int value = jpeg_successive_value(coeffs[k], al);
      if (value == 0) {
        ++zero_run;
        continue;
      }
      flush_eob_run();
      while (zero_run > 15) {
        ++ac_freq[0xf0u];
        zero_run -= 16;
      }
      const int category = jpeg_value_category(value);
      ++ac_freq[(zero_run << 4) | category];
      zero_run = 0;
    }
    if (zero_run > 0) {
      ++eob_run;
      if (eob_run == JPEG_PROGRESSIVE_MAX_EOB_RUN) {
        flush_eob_run();
      }
    }
  }
  flush_eob_run();
}
void jpeg_collect_progressive_ac_refine_frequencies(
    const std::vector<int> &blocks, int ss, int se, int ah, int al,
    std::uint64_t ac_freq[256], std::size_t restart_interval_blocks) {
  std::uint32_t eob_run = 0u;
  std::size_t eob_correction_bits = 0u;
  const auto flush_eob_run = [&]() {
    if (eob_run == 0u) {
      return;
    }
    ++ac_freq[jpeg_progressive_eob_run_symbol(eob_run)];
    eob_run = 0u;
    eob_correction_bits = 0u;
  };
  const std::size_t block_count = blocks.size() / 64u;
  for (std::size_t block = 0; block < block_count; ++block) {
    if (restart_interval_blocks != 0u && block != 0u &&
        block % restart_interval_blocks == 0u) {
      flush_eob_run();
    }
    const int *coeffs = blocks.data() + block * 64u;
    int zero_run = 0;
    int correction_zero_runs[64] = {};
    std::size_t correction_count = 0u;
    for (int k = ss; k <= se; ++k) {
      const int value = coeffs[k];
      const int magnitude = value < 0 ? -value : value;
      if ((magnitude >> ah) != 0) {
        correction_zero_runs[correction_count++] = zero_run;
        continue;
      }
      if ((magnitude >> al) == 0) {
        ++zero_run;
        continue;
      }
      flush_eob_run();
      while (zero_run > 15) {
        ++ac_freq[0xf0u];
        std::size_t emit_count = 0u;
        while (emit_count < correction_count &&
               correction_zero_runs[emit_count] < 16) {
          ++emit_count;
        }
        for (std::size_t i = emit_count; i < correction_count; ++i) {
          correction_zero_runs[i - emit_count] = correction_zero_runs[i] - 16;
        }
        correction_count -= emit_count;
        zero_run -= 16;
      }
      ++ac_freq[(zero_run << 4) | 1];
      zero_run = 0;
      correction_count = 0u;
    }
    if (zero_run > 0 || correction_count > 0u) {
      ++eob_run;
      eob_correction_bits += correction_count;
      if (eob_run == JPEG_PROGRESSIVE_MAX_EOB_RUN ||
          eob_correction_bits >
              JPEG_PROGRESSIVE_MAX_CORRECTION_BITS - 64u + 1u) {
        flush_eob_run();
      }
    }
  }
  flush_eob_run();
}
int jpeg_encode_progressive_dc_first_scan(const std::vector<int> &blocks,
                                          int al,
                                          const JpegHuffmanTable &dc_table,
                                          std::vector<std::uint8_t> *out,
                                          std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int previous_dc = 0;
    int restart_index = 0;
    const std::size_t block_count = blocks.size() / 64u;
    for (std::size_t block = 0; block < block_count; ++block) {
      if (restart_interval != 0u && block != 0u &&
          block % restart_interval == 0u) {
        jpeg_write_restart_marker(&writer, &restart_index);
        previous_dc = 0;
      }
      const int *coeffs = blocks.data() + block * 64u;
      const int dc = jpeg_successive_dc_value(coeffs[0], al);
      const int diff = dc - previous_dc;
      previous_dc = dc;
      const int category = jpeg_value_category(diff);
      int status = jpeg_write_huffman_symbol(&writer, dc_table, category);
      if (status != PILLOW_C_OK) {
        return status;
      }
      jpeg_bit_writer_write(&writer, jpeg_value_bits(diff, category), category);
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_encode_progressive_dc_refine_scan(const std::vector<int> &blocks,
                                           int al,
                                           std::vector<std::uint8_t> *out,
                                           std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int restart_index = 0;
    const std::size_t block_count = blocks.size() / 64u;
    for (std::size_t block = 0; block < block_count; ++block) {
      if (restart_interval != 0u && block != 0u &&
          block % restart_interval == 0u) {
        jpeg_write_restart_marker(&writer, &restart_index);
      }
      const int *coeffs = blocks.data() + block * 64u;
      jpeg_bit_writer_write(
          &writer,
          static_cast<std::uint16_t>(jpeg_successive_bit(coeffs[0], al)), 1);
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_encode_progressive_ac_first_scan(const std::vector<int> &blocks,
                                          int ss, int se, int al,
                                          const JpegHuffmanTable &ac_table,
                                          std::vector<std::uint8_t> *out,
                                          std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int restart_index = 0;
    std::uint32_t eob_run = 0u;
    const auto flush_eob_run = [&]() {
      if (eob_run == 0u) {
        return PILLOW_C_OK;
      }
      const int symbol = jpeg_progressive_eob_run_symbol(eob_run);
      const int status = jpeg_write_huffman_symbol(&writer, ac_table, symbol);
      if (status != PILLOW_C_OK) {
        return status;
      }
      const int nbits = symbol >> 4;
      if (nbits != 0) {
        jpeg_bit_writer_write(&writer, static_cast<std::uint16_t>(eob_run),
                              nbits);
      }
      eob_run = 0u;
      return PILLOW_C_OK;
    };
    const std::size_t block_count = blocks.size() / 64u;
    for (std::size_t block = 0; block < block_count; ++block) {
      if (restart_interval != 0u && block != 0u &&
          block % restart_interval == 0u) {
        const int status = flush_eob_run();
        if (status != PILLOW_C_OK) {
          return status;
        }
        jpeg_write_restart_marker(&writer, &restart_index);
      }
      const int *coeffs = blocks.data() + block * 64u;
      int zero_run = 0;
      for (int k = ss; k <= se; ++k) {
        const int value = jpeg_successive_value(coeffs[k], al);
        if (value == 0) {
          ++zero_run;
          continue;
        }
        int status = flush_eob_run();
        if (status != PILLOW_C_OK) {
          return status;
        }
        while (zero_run > 15) {
          status = jpeg_write_huffman_symbol(&writer, ac_table, 0xf0);
          if (status != PILLOW_C_OK) {
            return status;
          }
          zero_run -= 16;
        }
        const int category = jpeg_value_category(value);
        const int symbol = (zero_run << 4) | category;
        status = jpeg_write_huffman_symbol(&writer, ac_table, symbol);
        if (status != PILLOW_C_OK) {
          return status;
        }
        jpeg_bit_writer_write(&writer, jpeg_value_bits(value, category),
                              category);
        zero_run = 0;
      }
      if (zero_run > 0) {
        ++eob_run;
        if (eob_run == JPEG_PROGRESSIVE_MAX_EOB_RUN) {
          const int status = flush_eob_run();
          if (status != PILLOW_C_OK) {
            return status;
          }
        }
      }
    }
    const int status = flush_eob_run();
    if (status != PILLOW_C_OK) {
      return status;
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_emit_correction_bits(JpegBitWriter *writer,
                              const std::vector<int> &bits) {
  if (!writer) {
    return PILLOW_C_NULL_POINTER;
  }
  for (int bit : bits) {
    jpeg_bit_writer_write(writer, static_cast<std::uint16_t>(bit & 1), 1);
  }
  return PILLOW_C_OK;
}
int jpeg_emit_progressive_eob_run(JpegBitWriter *writer,
                                  const JpegHuffmanTable &ac_table,
                                  std::uint32_t *eob_run,
                                  std::vector<int> *correction_bits) {
  if (!writer || !eob_run || !correction_bits) {
    return PILLOW_C_NULL_POINTER;
  }
  if (*eob_run == 0u) {
    return correction_bits->empty() ? PILLOW_C_OK : PILLOW_C_MISMATCH;
  }
  const int symbol = jpeg_progressive_eob_run_symbol(*eob_run);
  int status = jpeg_write_huffman_symbol(writer, ac_table, symbol);
  if (status != PILLOW_C_OK) {
    return status;
  }
  const int nbits = symbol >> 4;
  if (nbits > 0) {
    jpeg_bit_writer_write(writer, static_cast<std::uint16_t>(*eob_run), nbits);
  }
  status = jpeg_emit_correction_bits(writer, *correction_bits);
  if (status != PILLOW_C_OK) {
    return status;
  }
  *eob_run = 0u;
  correction_bits->clear();
  return PILLOW_C_OK;
}
int jpeg_encode_progressive_ac_refine_scan(const std::vector<int> &blocks,
                                           int ss, int se, int ah, int al,
                                           const JpegHuffmanTable &ac_table,
                                           std::vector<std::uint8_t> *out,
                                           std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int restart_index = 0;
    const std::size_t block_count = blocks.size() / 64u;
    std::vector<int> correction_bits;
    std::vector<int> correction_zero_runs;
    std::vector<int> eob_correction_bits;
    correction_bits.reserve(static_cast<std::size_t>(se - ss + 1));
    correction_zero_runs.reserve(static_cast<std::size_t>(se - ss + 1));
    eob_correction_bits.reserve(JPEG_PROGRESSIVE_MAX_CORRECTION_BITS);
    std::uint32_t eob_run = 0u;
    for (std::size_t block = 0; block < block_count; ++block) {
      if (restart_interval != 0u && block != 0u &&
          block % restart_interval == 0u) {
        const int status = jpeg_emit_progressive_eob_run(
            &writer, ac_table, &eob_run, &eob_correction_bits);
        if (status != PILLOW_C_OK) {
          return status;
        }
        jpeg_write_restart_marker(&writer, &restart_index);
      }
      const int *coeffs = blocks.data() + block * 64u;
      int zero_run = 0;
      correction_bits.clear();
      correction_zero_runs.clear();
      for (int k = ss; k <= se; ++k) {
        const int value = coeffs[k];
        const int magnitude = value < 0 ? -value : value;
        if ((magnitude >> ah) != 0) {
          correction_bits.push_back((magnitude >> al) & 1);
          correction_zero_runs.push_back(zero_run);
          continue;
        }
        if ((magnitude >> al) == 0) {
          ++zero_run;
          continue;
        }
        if (eob_run != 0u) {
          const int status = jpeg_emit_progressive_eob_run(
              &writer, ac_table, &eob_run, &eob_correction_bits);
          if (status != PILLOW_C_OK) {
            return status;
          }
        }
        while (zero_run > 15) {
          int status = jpeg_write_huffman_symbol(&writer, ac_table, 0xf0);
          if (status != PILLOW_C_OK) {
            return status;
          }
          std::size_t emit_count = 0u;
          while (emit_count < correction_bits.size() &&
                 correction_zero_runs[emit_count] < 16) {
            jpeg_bit_writer_write(
                &writer,
                static_cast<std::uint16_t>(correction_bits[emit_count] & 1), 1);
            ++emit_count;
          }
          correction_bits.erase(correction_bits.begin(),
                                correction_bits.begin() + emit_count);
          correction_zero_runs.erase(correction_zero_runs.begin(),
                                     correction_zero_runs.begin() + emit_count);
          for (int &buffered_zero_run : correction_zero_runs) {
            buffered_zero_run -= 16;
          }
          zero_run -= 16;
        }
        const int symbol = (zero_run << 4) | 1;
        int status = jpeg_write_huffman_symbol(&writer, ac_table, symbol);
        if (status != PILLOW_C_OK) {
          return status;
        }
        jpeg_bit_writer_write(&writer, value < 0 ? 0u : 1u, 1);
        status = jpeg_emit_correction_bits(&writer, correction_bits);
        if (status != PILLOW_C_OK) {
          return status;
        }
        correction_bits.clear();
        correction_zero_runs.clear();
        zero_run = 0;
      }
      if (zero_run > 0 || !correction_bits.empty()) {
        ++eob_run;
        eob_correction_bits.insert(eob_correction_bits.end(),
                                   correction_bits.begin(),
                                   correction_bits.end());
        if (eob_run == JPEG_PROGRESSIVE_MAX_EOB_RUN ||
            eob_correction_bits.size() >
                JPEG_PROGRESSIVE_MAX_CORRECTION_BITS - 64u + 1u) {
          const int status = jpeg_emit_progressive_eob_run(
              &writer, ac_table, &eob_run, &eob_correction_bits);
          if (status != PILLOW_C_OK) {
            return status;
          }
        }
      }
    }
    const int status = jpeg_emit_progressive_eob_run(
        &writer, ac_table, &eob_run, &eob_correction_bits);
    if (status != PILLOW_C_OK) {
      return status;
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_append_luminance_sos_segment(std::vector<std::uint8_t> &out, int ss,
                                      int se, int ah, int al) {
  std::uint8_t sos[] = {1,
                        1,
                        0,
                        static_cast<std::uint8_t>(ss),
                        static_cast<std::uint8_t>(se),
                        static_cast<std::uint8_t>((ah << 4) | al)};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int jpeg_encode_rgb_interleaved_entropy(const std::vector<int> &y_blocks,
                                        const std::vector<int> &cb_blocks,
                                        const std::vector<int> &cr_blocks,
                                        int y_blocks_per_mcu,
                                        const JpegHuffmanTable &luma_dc_table,
                                        const JpegHuffmanTable &luma_ac_table,
                                        const JpegHuffmanTable &chroma_dc_table,
                                        const JpegHuffmanTable &chroma_ac_table,
                                        std::vector<std::uint8_t> *out,
                                        std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  if (y_blocks_per_mcu <= 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const std::size_t mcu_count = cb_blocks.size() / 64u;
  if (cr_blocks.size() / 64u != mcu_count ||
      y_blocks.size() / 64u !=
          mcu_count * static_cast<std::size_t>(y_blocks_per_mcu)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int previous_y_dc = 0;
    int previous_cb_dc = 0;
    int previous_cr_dc = 0;
    int restart_index = 0;
    std::size_t y_block_index = 0;
    for (std::size_t mcu = 0; mcu < mcu_count; ++mcu) {
      if (restart_interval != 0u && mcu != 0u && mcu % restart_interval == 0u) {
        jpeg_bit_writer_flush(&writer);
        out->push_back(0xffu);
        out->push_back(static_cast<std::uint8_t>(0xd0u + (restart_index & 7)));
        ++restart_index;
        previous_y_dc = 0;
        previous_cb_dc = 0;
        previous_cr_dc = 0;
      }
      for (int y_block = 0; y_block < y_blocks_per_mcu; ++y_block) {
        const int *y_coeffs = y_blocks.data() + y_block_index * 64u;
        ++y_block_index;
        const int status = jpeg_encode_block_entropy(
            &writer, y_coeffs, &previous_y_dc, luma_dc_table, luma_ac_table);
        if (status != PILLOW_C_OK) {
          return status;
        }
      }
      const int *cb_coeffs = cb_blocks.data() + mcu * 64u;
      const int *cr_coeffs = cr_blocks.data() + mcu * 64u;
      int status =
          jpeg_encode_block_entropy(&writer, cb_coeffs, &previous_cb_dc,
                                    chroma_dc_table, chroma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_block_entropy(&writer, cr_coeffs, &previous_cr_dc,
                                         chroma_dc_table, chroma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
} // namespace pillow_c_jpeg
