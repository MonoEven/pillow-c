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
#include "pillow_c_wic_internal.h"
#include <wincodec.h>
#include <windows.h>

namespace pillow_c_jpeg {
struct JpegMetadata {
  int components = 0;
  int exif_orientation = 0;
  bool has_dpi = false;
  double dpi_x = 0.0;
  double dpi_y = 0.0;
  bool has_jfif = false;
  int jfif_major = 0;
  int jfif_minor = 0;
  int jfif_unit = -1;
  int jfif_density_x = 0;
  int jfif_density_y = 0;
  std::vector<std::uint8_t> comment;
  bool has_icc_profile = false;
  bool has_icc_profile_none = false;
  std::vector<std::uint8_t> icc_profile;
  bool icc_finalized = false;
  std::vector<std::vector<std::uint8_t>> icc_markers;
  std::vector<std::pair<int, std::vector<std::uint8_t>>> photoshop_resources;
  bool has_photoshop_resolution_info = false;
  double photoshop_x_resolution = 0.0;
  int photoshop_displayed_units_x = 0;
  double photoshop_y_resolution = 0.0;
  int photoshop_displayed_units_y = 0;
  std::vector<std::uint8_t> exif;
  std::vector<std::uint8_t> xmp;
  std::vector<int> qtables;
  std::size_t qtable_count = 0;
  int subsampling = -1;
};
constexpr int JPEG_DECODE_ZIGZAG[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
bool jpeg_is_sof_marker(std::uint8_t marker) {
  return (marker >= 0xc0u && marker <= 0xcfu) && marker != 0xc4u &&
         marker != 0xc8u && marker != 0xccu;
}
void apply_jpeg_jfif_metadata(const std::uint8_t *payload,
                              std::size_t payload_size,
                              JpegMetadata *metadata) {
  if (!payload || payload_size < 12u || !metadata ||
      std::memcmp(payload, "JFIF\0", 5u) != 0) {
    return;
  }
  metadata->has_jfif = true;
  metadata->jfif_major = payload[5];
  metadata->jfif_minor = payload[6];
  metadata->jfif_unit = payload[7];
  metadata->jfif_density_x = read_be16(payload + 8u);
  metadata->jfif_density_y = read_be16(payload + 10u);
  if (metadata->jfif_unit == 1) {
    metadata->has_dpi = true;
    metadata->dpi_x = static_cast<double>(metadata->jfif_density_x);
    metadata->dpi_y = static_cast<double>(metadata->jfif_density_y);
  } else if (metadata->jfif_unit == 2) {
    metadata->has_dpi = true;
    metadata->dpi_x = static_cast<double>(metadata->jfif_density_x) * 2.54;
    metadata->dpi_y = static_cast<double>(metadata->jfif_density_y) * 2.54;
  }
}
bool jpeg_payload_starts_with(const std::uint8_t *payload,
                              std::size_t payload_size, const char *prefix,
                              std::size_t prefix_size) {
  return payload && prefix && payload_size >= prefix_size &&
         std::memcmp(payload, prefix, prefix_size) == 0;
}
void apply_jpeg_exif_metadata(const std::uint8_t *payload,
                              std::size_t payload_size,
                              JpegMetadata *metadata) {
  static constexpr char exif_header[] = {'E', 'x', 'i', 'f', 0, 0};
  if (!metadata || !jpeg_payload_starts_with(payload, payload_size, exif_header,
                                             sizeof(exif_header))) {
    return;
  }
  if (metadata->exif.empty()) {
    metadata->exif.assign(payload, payload + payload_size);
  } else {
    metadata->exif.insert(metadata->exif.end(), payload + sizeof(exif_header),
                          payload + payload_size);
  }
  if (metadata->exif_orientation == 0) {
    metadata->exif_orientation =
        pillow_c_parse_exif_orientation(payload, payload_size);
  }
}
void apply_jpeg_xmp_metadata(const std::uint8_t *payload,
                             std::size_t payload_size, JpegMetadata *metadata) {
  static constexpr char xmp_header[] = {
      'h', 't', 't', 'p', ':', '/', '/', 'n', 's', '.', 'a', 'd', 'o', 'b', 'e',
      '.', 'c', 'o', 'm', '/', 'x', 'a', 'p', '/', '1', '.', '0', '/', 0};
  if (!metadata || !jpeg_payload_starts_with(payload, payload_size, xmp_header,
                                             sizeof(xmp_header))) {
    return;
  }
  metadata->xmp.assign(payload + sizeof(xmp_header), payload + payload_size);
}
void apply_jpeg_photoshop_metadata(const std::uint8_t *payload,
                                   std::size_t payload_size,
                                   JpegMetadata *metadata) {
  static constexpr char photoshop_header[] = {'P', 'h', 'o', 't', 'o', 's', 'h',
                                              'o', 'p', ' ', '3', '.', '0', 0};
  static constexpr char resource_signature[] = {'8', 'B', 'I', 'M'};
  if (!metadata ||
      !jpeg_payload_starts_with(payload, payload_size, photoshop_header,
                                sizeof(photoshop_header))) {
    return;
  }
  std::size_t offset = sizeof(photoshop_header);
  while (offset <= payload_size &&
         payload_size - offset >= sizeof(resource_signature) &&
         std::memcmp(payload + offset, resource_signature,
                     sizeof(resource_signature)) == 0) {
    offset += sizeof(resource_signature);
    if (payload_size - offset < 3u) {
      break;
    }
    const int code = read_be16(payload + offset);
    offset += 2u;
    const std::size_t name_size = payload[offset++];
    if (name_size > payload_size - offset) {
      break;
    }
    offset += name_size;
    if ((offset & 1u) != 0u) {
      ++offset;
    }
    if (offset > payload_size || payload_size - offset < 4u) {
      break;
    }
    const std::size_t data_size = read_be32(payload + offset);
    offset += 4u;
    if (data_size > payload_size - offset) {
      break;
    }
    if (code == 0x03ed && data_size >= 14u) {
      const std::uint8_t *data = payload + offset;
      metadata->has_photoshop_resolution_info = true;
      metadata->photoshop_x_resolution =
          static_cast<double>(read_be32(data)) / 65536.0;
      metadata->photoshop_displayed_units_x = read_be16(data + 4u);
      metadata->photoshop_y_resolution =
          static_cast<double>(read_be32(data + 8u)) / 65536.0;
      metadata->photoshop_displayed_units_y = read_be16(data + 12u);
    } else if (code != 0x03ed) {
      auto existing = std::find_if(
          metadata->photoshop_resources.begin(),
          metadata->photoshop_resources.end(),
          [code](const auto &resource) { return resource.first == code; });
      std::vector<std::uint8_t> value(payload + offset,
                                      payload + offset + data_size);
      if (existing == metadata->photoshop_resources.end()) {
        metadata->photoshop_resources.emplace_back(code, std::move(value));
      } else {
        existing->second = std::move(value);
      }
    }
    offset += data_size;
    if ((offset & 1u) != 0u) {
      ++offset;
    }
  }
}
bool apply_jpeg_icc_profile_metadata(const std::uint8_t *payload,
                                     std::size_t payload_size,
                                     JpegMetadata *metadata) {
  static constexpr char icc_header[] = {'I', 'C', 'C', '_', 'P', 'R',
                                        'O', 'F', 'I', 'L', 'E', 0};
  if (!metadata) {
    return false;
  }
  if (metadata->icc_finalized ||
      !jpeg_payload_starts_with(payload, payload_size, icc_header,
                                sizeof(icc_header))) {
    return true;
  }
  if (payload_size < sizeof(icc_header) + 2u) {
    return false;
  }
  metadata->icc_markers.emplace_back(payload, payload + payload_size);
  return true;
}
void finalize_jpeg_icc_profile_metadata(JpegMetadata *metadata) {
  static constexpr std::size_t icc_header_size = 14u;
  if (!metadata || metadata->icc_markers.empty()) {
    return;
  }
  std::sort(metadata->icc_markers.begin(), metadata->icc_markers.end());
  if (static_cast<std::size_t>(metadata->icc_markers.front()[13]) !=
      metadata->icc_markers.size()) {
    metadata->has_icc_profile_none = true;
    metadata->icc_markers.clear();
    return;
  }
  metadata->has_icc_profile = true;
  std::size_t total_size = 0u;
  for (const auto &marker : metadata->icc_markers) {
    total_size += marker.size() - icc_header_size;
  }
  metadata->icc_profile.reserve(total_size);
  for (const auto &marker : metadata->icc_markers) {
    metadata->icc_profile.insert(metadata->icc_profile.end(),
                                 marker.begin() + icc_header_size,
                                 marker.end());
  }
  metadata->icc_markers.clear();
}
void apply_jpeg_qtable_metadata(const std::uint8_t *payload,
                                std::size_t payload_size,
                                JpegMetadata *metadata) {
  if (!payload || !metadata) {
    return;
  }
  std::size_t offset = 0u;
  while (offset < payload_size) {
    const std::uint8_t table_info = payload[offset++];
    const int precision = table_info >> 4;
    const int table_id = table_info & 0x0f;
    if (precision != 0 || table_id > 3 || payload_size - offset < 64u) {
      return;
    }
    const std::size_t required_count = static_cast<std::size_t>(table_id + 1);
    if (metadata->qtables.size() < required_count * 64u) {
      metadata->qtables.resize(required_count * 64u, 0);
    }
    metadata->qtable_count = std::max(metadata->qtable_count, required_count);
    int *table =
        metadata->qtables.data() + static_cast<std::size_t>(table_id) * 64u;
    for (int i = 0; i < 64; ++i) {
      table[JPEG_DECODE_ZIGZAG[i]] =
          payload[offset + static_cast<std::size_t>(i)];
    }
    offset += 64u;
  }
}
int jpeg_sampling_to_subsampling(int h, int v) {
  if (h == 1 && v == 1) {
    return 0;
  }
  if (h == 2 && v == 1) {
    return 1;
  }
  if (h == 2 && v == 2) {
    return 2;
  }
  return -1;
}
void apply_jpeg_sof_metadata(const std::uint8_t *payload,
                             std::size_t payload_size, JpegMetadata *metadata) {
  if (!payload || !metadata || payload_size < 6u) {
    return;
  }
  const int component_count = payload[5];
  metadata->components = component_count;
  if ((component_count != 3 && component_count != 4) ||
      payload_size < 6u + static_cast<std::size_t>(component_count) * 3u) {
    metadata->subsampling = -1;
    return;
  }
  if (component_count == 3) {
    const std::uint8_t *y = payload + 6u;
    const std::uint8_t *cb = y + 3u;
    const std::uint8_t *cr = cb + 3u;
    if (y[0] != 1u || cb[0] != 2u || cr[0] != 3u || (cb[1] >> 4) != 1u ||
        (cb[1] & 0x0fu) != 1u || (cr[1] >> 4) != 1u || (cr[1] & 0x0fu) != 1u) {
      metadata->subsampling = -1;
      return;
    }
    metadata->subsampling =
        jpeg_sampling_to_subsampling(y[1] >> 4, y[1] & 0x0f);
    return;
  }
  const std::uint8_t *c = payload + 6u;
  const std::uint8_t *m = c + 3u;
  const std::uint8_t *y = m + 3u;
  const std::uint8_t *k = y + 3u;
  if (c[0] != 'C' || m[0] != 'M' || y[0] != 'Y' || k[0] != 'K' ||
      (m[1] >> 4) != 1u || (m[1] & 0x0fu) != 1u || (y[1] >> 4) != 1u ||
      (y[1] & 0x0fu) != 1u || (k[1] >> 4) != 1u || (k[1] & 0x0fu) != 1u) {
    metadata->subsampling = -1;
    return;
  }
  metadata->subsampling = jpeg_sampling_to_subsampling(c[1] >> 4, c[1] & 0x0f);
}
bool read_jpeg_metadata(const char *path, JpegMetadata *metadata) {
  if (!path || !metadata) {
    return false;
  }
  *metadata = JpegMetadata{};
  std::vector<std::uint8_t> data;
  if (!read_binary_file(path, &data)) {
    return false;
  }
  if (data.size() < 4 || data[0] != 0xffu || data[1] != 0xd8u) {
    return false;
  }
  std::size_t offset = 2u;
  while (offset < data.size()) {
    if (data[offset] != 0xffu) {
      return false;
    }
    while (offset < data.size() && data[offset] == 0xffu) {
      ++offset;
    }
    if (offset >= data.size()) {
      return false;
    }
    const std::uint8_t marker = data[offset++];
    if (marker == 0xd9u || marker == 0xdau) {
      break;
    }
    if (marker == 0x01u || (marker >= 0xd0u && marker <= 0xd7u)) {
      continue;
    }
    if (offset + 2u > data.size()) {
      return false;
    }
    const std::uint16_t segment_length = read_be16(data.data() + offset);
    if (segment_length < 2u || offset + segment_length > data.size()) {
      return false;
    }
    const std::uint8_t *segment_payload = data.data() + offset + 2u;
    const std::size_t segment_payload_size =
        static_cast<std::size_t>(segment_length) - 2u;
    if (marker == 0xe0u && !metadata->has_jfif) {
      apply_jpeg_jfif_metadata(segment_payload, segment_payload_size, metadata);
    }
    if (marker == 0xe1u) {
      apply_jpeg_exif_metadata(segment_payload, segment_payload_size, metadata);
      apply_jpeg_xmp_metadata(segment_payload, segment_payload_size, metadata);
    }
    if (marker == 0xe2u) {
      if (!apply_jpeg_icc_profile_metadata(segment_payload,
                                           segment_payload_size, metadata)) {
        return false;
      }
    }
    if (marker == 0xedu) {
      apply_jpeg_photoshop_metadata(segment_payload, segment_payload_size,
                                    metadata);
    }
    if (marker == 0xfeu) {
      metadata->comment.assign(segment_payload,
                               segment_payload + segment_payload_size);
    }
    if (marker == 0xdbu) {
      apply_jpeg_qtable_metadata(segment_payload, segment_payload_size,
                                 metadata);
    }
    if (jpeg_is_sof_marker(marker)) {
      if (segment_payload_size < 6u) {
        return false;
      }
      if (!metadata->icc_finalized) {
        finalize_jpeg_icc_profile_metadata(metadata);
        metadata->icc_finalized = true;
      }
      apply_jpeg_sof_metadata(segment_payload, segment_payload_size, metadata);
    }
    offset += segment_length;
  }
  return metadata->components > 0;
}
bool read_jpeg_component_count_and_orientation(const char *path,
                                               int *components,
                                               int *orientation) {
  if (!path || !components || !orientation) {
    return false;
  }
  JpegMetadata metadata;
  if (!read_jpeg_metadata(path, &metadata)) {
    return false;
  }
  *components = metadata.components;
  *orientation = metadata.exif_orientation;
  return true;
}
bool read_jpeg_component_count(const char *path, int *components) {
  int orientation = 0;
  return read_jpeg_component_count_and_orientation(path, components,
                                                   &orientation);
}
bool jpeg_h2v1_nearest_interleave_ycbcr(
    const std::vector<std::uint8_t> &y_pixels, std::size_t y_stride,
    const std::vector<std::uint8_t> &cb_pixels, std::size_t cb_stride,
    const std::vector<std::uint8_t> &cr_pixels, std::size_t cr_stride,
    int chroma_width, int width, int height,
    std::vector<std::uint8_t> *output) {
  if (!output || chroma_width <= 0 ||
      static_cast<std::int64_t>(chroma_width) * 2 != width) {
    return false;
  }
  const std::size_t output_stride = static_cast<std::size_t>(width) * 3u;
  for (int y = 0; y < height; ++y) {
    const auto *y_row =
        y_pixels.data() + static_cast<std::size_t>(y) * y_stride;
    const auto *cb_row =
        cb_pixels.data() + static_cast<std::size_t>(y) * cb_stride;
    const auto *cr_row =
        cr_pixels.data() + static_cast<std::size_t>(y) * cr_stride;
    auto *output_row =
        output->data() + static_cast<std::size_t>(y) * output_stride;
    for (int x = 0; x < width; ++x) {
      const std::size_t output_offset = static_cast<std::size_t>(x) * 3u;
      const std::size_t chroma_offset = static_cast<std::size_t>(x / 2);
      output_row[output_offset] = y_row[x];
      output_row[output_offset + 1u] = cb_row[chroma_offset];
      output_row[output_offset + 2u] = cr_row[chroma_offset];
    }
  }
  return true;
}
bool jpeg_h2v1_fancy_interleave_ycbcr(
    const std::vector<std::uint8_t> &y_pixels, std::size_t y_stride,
    const std::vector<std::uint8_t> &cb_pixels, std::size_t cb_stride,
    const std::vector<std::uint8_t> &cr_pixels, std::size_t cr_stride,
    int chroma_width, int width, int height,
    std::vector<std::uint8_t> *output) {
  if (!output || chroma_width < 2 ||
      static_cast<std::int64_t>(chroma_width) * 2 != width) {
    return false;
  }
  const std::size_t output_stride = static_cast<std::size_t>(width) * 3u;
  for (int y = 0; y < height; ++y) {
    const auto *y_row =
        y_pixels.data() + static_cast<std::size_t>(y) * y_stride;
    const auto *cb_row =
        cb_pixels.data() + static_cast<std::size_t>(y) * cb_stride;
    const auto *cr_row =
        cr_pixels.data() + static_cast<std::size_t>(y) * cr_stride;
    auto *output_row =
        output->data() + static_cast<std::size_t>(y) * output_stride;
    for (int x = 0; x < width; ++x) {
      output_row[static_cast<std::size_t>(x) * 3u] = y_row[x];
    }
    const auto upsample_channel = [chroma_width, width,
                                   output_row](const std::uint8_t *input,
                                               std::size_t channel) {
      int input_value = input[0];
      output_row[channel] = static_cast<std::uint8_t>(input_value);
      output_row[3u + channel] =
          static_cast<std::uint8_t>((input_value * 3 + input[1] + 2) >> 2);
      for (int x = 1; x < chroma_width - 1; ++x) {
        input_value = input[x] * 3;
        output_row[static_cast<std::size_t>(x * 2) * 3u + channel] =
            static_cast<std::uint8_t>((input_value + input[x - 1] + 1) >> 2);
        output_row[static_cast<std::size_t>(x * 2 + 1) * 3u + channel] =
            static_cast<std::uint8_t>((input_value + input[x + 1] + 2) >> 2);
      }
      input_value = input[chroma_width - 1];
      output_row[static_cast<std::size_t>(width - 2) * 3u + channel] =
          static_cast<std::uint8_t>(
              (input_value * 3 + input[chroma_width - 2] + 1) >> 2);
      output_row[static_cast<std::size_t>(width - 1) * 3u + channel] =
          static_cast<std::uint8_t>(input_value);
    };
    upsample_channel(cb_row, 1u);
    upsample_channel(cr_row, 2u);
  }
  return true;
}
bool jpeg_h2v2_fancy_interleave_ycbcr(
    const std::vector<std::uint8_t> &y_pixels, std::size_t y_stride,
    const std::vector<std::uint8_t> &cb_pixels, std::size_t cb_stride,
    const std::vector<std::uint8_t> &cr_pixels, std::size_t cr_stride,
    int chroma_width, int chroma_height, int width, int height,
    std::vector<std::uint8_t> *output) {
  if (!output || chroma_width < 2 || chroma_height <= 0 ||
      static_cast<std::int64_t>(chroma_width) * 2 != width ||
      static_cast<std::int64_t>(chroma_height) * 2 != height) {
    return false;
  }
  const std::size_t output_stride = static_cast<std::size_t>(width) * 3u;
  for (int y = 0; y < height; ++y) {
    const auto *y_row =
        y_pixels.data() + static_cast<std::size_t>(y) * y_stride;
    const int chroma_y = y / 2;
    const int adjacent_y = (y & 1) == 0
                               ? std::max(chroma_y - 1, 0)
                               : std::min(chroma_y + 1, chroma_height - 1);
    const auto *cb_row =
        cb_pixels.data() + static_cast<std::size_t>(chroma_y) * cb_stride;
    const auto *cb_adjacent =
        cb_pixels.data() + static_cast<std::size_t>(adjacent_y) * cb_stride;
    const auto *cr_row =
        cr_pixels.data() + static_cast<std::size_t>(chroma_y) * cr_stride;
    const auto *cr_adjacent =
        cr_pixels.data() + static_cast<std::size_t>(adjacent_y) * cr_stride;
    auto *output_row =
        output->data() + static_cast<std::size_t>(y) * output_stride;
    for (int x = 0; x < width; ++x) {
      output_row[static_cast<std::size_t>(x) * 3u] = y_row[x];
    }
    const auto upsample_channel = [chroma_width, width,
                                   output_row](const std::uint8_t *input,
                                               const std::uint8_t *adjacent,
                                               std::size_t channel) {
      int current_sum = input[0] * 3 + adjacent[0];
      int next_sum = input[1] * 3 + adjacent[1];
      output_row[channel] =
          static_cast<std::uint8_t>((current_sum * 4 + 8) >> 4);
      output_row[3u + channel] =
          static_cast<std::uint8_t>((current_sum * 3 + next_sum + 7) >> 4);
      int previous_sum = current_sum;
      current_sum = next_sum;
      for (int x = 1; x < chroma_width - 1; ++x) {
        next_sum = input[x + 1] * 3 + adjacent[x + 1];
        output_row[static_cast<std::size_t>(x * 2) * 3u + channel] =
            static_cast<std::uint8_t>((current_sum * 3 + previous_sum + 8) >>
                                      4);
        output_row[static_cast<std::size_t>(x * 2 + 1) * 3u + channel] =
            static_cast<std::uint8_t>((current_sum * 3 + next_sum + 7) >> 4);
        previous_sum = current_sum;
        current_sum = next_sum;
      }
      output_row[static_cast<std::size_t>(width - 2) * 3u + channel] =
          static_cast<std::uint8_t>((current_sum * 3 + previous_sum + 8) >> 4);
      output_row[static_cast<std::size_t>(width - 1) * 3u + channel] =
          static_cast<std::uint8_t>((current_sum * 4 + 7) >> 4);
    };
    upsample_channel(cb_row, cb_adjacent, 1u);
    upsample_channel(cr_row, cr_adjacent, 2u);
  }
  return true;
}
void jpeg_interleave_ycbcr_planes(const std::vector<std::uint8_t> &y_pixels,
                                  std::size_t y_stride,
                                  const std::vector<std::uint8_t> &cb_pixels,
                                  std::size_t cb_stride,
                                  const std::vector<std::uint8_t> &cr_pixels,
                                  std::size_t cr_stride, int width, int height,
                                  std::vector<std::uint8_t> *output) {
  const std::size_t output_stride = static_cast<std::size_t>(width) * 3u;
  for (int y = 0; y < height; ++y) {
    const auto *y_row =
        y_pixels.data() + static_cast<std::size_t>(y) * y_stride;
    const auto *cb_row =
        cb_pixels.data() + static_cast<std::size_t>(y) * cb_stride;
    const auto *cr_row =
        cr_pixels.data() + static_cast<std::size_t>(y) * cr_stride;
    auto *output_row =
        output->data() + static_cast<std::size_t>(y) * output_stride;
    for (int x = 0; x < width; ++x) {
      const std::size_t output_offset = static_cast<std::size_t>(x) * 3u;
      output_row[output_offset] = y_row[x];
      output_row[output_offset + 1u] = cb_row[x];
      output_row[output_offset + 2u] = cr_row[x];
    }
  }
}
int open_jpeg_image_impl(const char *path, int draft_target_width,
                         int draft_target_height, int draft_mode,
                         int *out_draft_scale, PillowCImage **out_image) {
  if (!path || !out_image) {
    return PILLOW_C_NULL_POINTER;
  }
  *out_image = nullptr;
  if (out_draft_scale) {
    *out_draft_scale = 0;
    if (draft_target_width <= 0 || draft_target_height <= 0) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
  }
  try {
    JpegMetadata metadata;
    if (!read_jpeg_metadata(path, &metadata)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    if (metadata.components != 1 && metadata.components != 3 &&
        metadata.components != 4) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
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
        wide_path.data(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
        decoder.put());
    if (FAILED(hr)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    GUID container = {};
    if (FAILED(decoder->GetContainerFormat(&container)) ||
        !IsEqualGUID(container, GUID_ContainerFormatJpeg)) {
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
    const int original_width = static_cast<int>(width_u);
    const int original_height = static_cast<int>(height_u);
    if (original_width <= 0 || original_height <= 0) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    int draft_scale = 1;
    if (out_draft_scale) {
      const int available_scale =
          std::min(original_width / draft_target_width,
                   original_height / draft_target_height);
      for (const int candidate : {8, 4, 2, 1}) {
        if (available_scale >= candidate) {
          draft_scale = candidate;
          break;
        }
      }
    }
    const int width = (original_width + draft_scale - 1) / draft_scale;
    const int height = (original_height + draft_scale - 1) / draft_scale;
    int mode = PILLOW_C_MODE_RGB;
    int channels = 3;
    WICPixelFormatGUID target_format = GUID_WICPixelFormat24bppRGB;
    if (metadata.components == 1) {
      mode = PILLOW_C_MODE_L;
      channels = 1;
      target_format = GUID_WICPixelFormat8bppGray;
    } else if (metadata.components == 4) {
      mode = PILLOW_C_MODE_CMYK;
      channels = 4;
      target_format = GUID_WICPixelFormat32bppCMYK;
    }
    if (draft_mode != 0 && draft_mode != mode) {
      if (metadata.components == 3 && (draft_mode == PILLOW_C_MODE_L ||
                                       draft_mode == PILLOW_C_MODE_YCBCR)) {
        mode = draft_mode;
        channels = mode == PILLOW_C_MODE_L ? 1 : 3;
        target_format = mode == PILLOW_C_MODE_L ? GUID_WICPixelFormat8bppGray
                                                : GUID_WICPixelFormat24bppRGB;
      } else {
        return PILLOW_C_INVALID_ARGUMENT;
      }
    }
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size(width, height, channels, &stride, &size) ||
        stride > static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
        size > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const bool draft_planar =
        metadata.components == 3 && draft_mode != 0 &&
        (mode == PILLOW_C_MODE_L || mode == PILLOW_C_MODE_YCBCR);
    ComPtr<IWICBitmapSource> source;
    if (draft_scale == 1 && !draft_planar) {
      WICPixelFormatGUID source_format = {};
      hr = frame->GetPixelFormat(&source_format);
      if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      if (IsEqualGUID(source_format, target_format)) {
        source.reset(frame.get());
        source.get()->AddRef();
      } else {
        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(converter.put());
        if (FAILED(hr)) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
        hr = converter->Initialize(frame.get(), target_format,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr)) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
        source.reset(converter.get());
        source.get()->AddRef();
      }
    }
    auto *image = new PillowCImage{
        width, height, mode, channels, stride, std::vector<std::uint8_t>(size)};
    image->exif_orientation = metadata.exif_orientation;
    image->has_dpi = metadata.has_dpi;
    image->dpi_x = metadata.dpi_x;
    image->dpi_y = metadata.dpi_y;
    image->has_jfif = metadata.has_jfif;
    image->jfif_major = metadata.jfif_major;
    image->jfif_minor = metadata.jfif_minor;
    image->jfif_unit = metadata.jfif_unit;
    image->jfif_density_x = metadata.jfif_density_x;
    image->jfif_density_y = metadata.jfif_density_y;
    image->jpeg_comment = std::move(metadata.comment);
    image->has_jpeg_icc_profile = metadata.has_icc_profile;
    image->has_jpeg_icc_profile_none = metadata.has_icc_profile_none;
    image->jpeg_icc_profile = std::move(metadata.icc_profile);
    image->jpeg_photoshop_resources = std::move(metadata.photoshop_resources);
    image->has_jpeg_photoshop_resolution_info =
        metadata.has_photoshop_resolution_info;
    image->jpeg_photoshop_x_resolution = metadata.photoshop_x_resolution;
    image->jpeg_photoshop_displayed_units_x =
        metadata.photoshop_displayed_units_x;
    image->jpeg_photoshop_y_resolution = metadata.photoshop_y_resolution;
    image->jpeg_photoshop_displayed_units_y =
        metadata.photoshop_displayed_units_y;
    image->jpeg_exif = std::move(metadata.exif);
    image->xmp = std::move(metadata.xmp);
    image->jpeg_qtables = std::move(metadata.qtables);
    image->jpeg_qtable_count = metadata.qtable_count;
    image->jpeg_subsampling = metadata.subsampling;
    if (draft_scale == 1 && !draft_planar) {
      hr = source->CopyPixels(nullptr, static_cast<UINT>(stride),
                              static_cast<UINT>(image->pixels.size()),
                              image->pixels.data());
    } else if (draft_planar) {
      ComPtr<IWICPlanarBitmapSourceTransform> transform;
      hr = frame->QueryInterface(__uuidof(IWICPlanarBitmapSourceTransform),
                                 reinterpret_cast<void **>(transform.put()));
      if (FAILED(hr)) {
        delete image;
        return PILLOW_C_INVALID_ARGUMENT;
      }
      UINT planar_width = static_cast<UINT>(width);
      UINT planar_height = static_cast<UINT>(height);
      const WICPixelFormatGUID planar_formats[3] = {GUID_WICPixelFormat8bppY,
                                                    GUID_WICPixelFormat8bppCb,
                                                    GUID_WICPixelFormat8bppCr};
      WICBitmapPlaneDescription descriptions[3] = {};
      BOOL supported = FALSE;
      hr = transform->DoesSupportTransform(
          &planar_width, &planar_height, WICBitmapTransformRotate0,
          WICPlanarOptionsDefault, planar_formats, descriptions, 3, &supported);
      if (FAILED(hr) || !supported ||
          planar_width != static_cast<UINT>(width) ||
          planar_height != static_cast<UINT>(height)) {
        delete image;
        return PILLOW_C_INVALID_ARGUMENT;
      }
      std::size_t plane_strides[3] = {};
      std::size_t plane_sizes[3] = {};
      for (int i = 0; i < 3; ++i) {
        const auto &description = descriptions[i];
        if (!IsEqualGUID(description.Format, planar_formats[i]) ||
            description.Width == 0 || description.Height == 0 ||
            description.Width >
                static_cast<UINT>(std::numeric_limits<int>::max()) ||
            description.Height >
                static_cast<UINT>(std::numeric_limits<int>::max()) ||
            !checked_image_size(static_cast<int>(description.Width),
                                static_cast<int>(description.Height), 1,
                                &plane_strides[i], &plane_sizes[i]) ||
            plane_strides[i] >
                static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
            plane_sizes[i] >
                static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
          delete image;
          return PILLOW_C_INVALID_ARGUMENT;
        }
      }
      if (descriptions[0].Width != static_cast<UINT>(width) ||
          descriptions[0].Height != static_cast<UINT>(height)) {
        delete image;
        return PILLOW_C_INVALID_ARGUMENT;
      }
      if (mode == PILLOW_C_MODE_YCBCR) {
        const bool chroma_planes_match =
            descriptions[1].Width == descriptions[2].Width &&
            descriptions[1].Height == descriptions[2].Height;
        const bool full_size_chroma =
            descriptions[1].Width == static_cast<UINT>(width) &&
            descriptions[1].Height == static_cast<UINT>(height);
        const bool h2v1_chroma =
            descriptions[1].Height == static_cast<UINT>(height) &&
            static_cast<std::uint64_t>(descriptions[1].Width) * 2u ==
                static_cast<std::uint64_t>(width);
        const bool h2v2_chroma =
            static_cast<std::uint64_t>(descriptions[1].Width) * 2u ==
                static_cast<std::uint64_t>(width) &&
            static_cast<std::uint64_t>(descriptions[1].Height) * 2u ==
                static_cast<std::uint64_t>(height);
        if (!chroma_planes_match ||
            (!full_size_chroma && !h2v1_chroma && !h2v2_chroma)) {
          delete image;
          return PILLOW_C_INVALID_ARGUMENT;
        }
      }
      std::vector<std::uint8_t> y_pixels(plane_sizes[0]);
      std::vector<std::uint8_t> cb_pixels(plane_sizes[1]);
      std::vector<std::uint8_t> cr_pixels(plane_sizes[2]);
      const WICBitmapPlane planes[3] = {{planar_formats[0], y_pixels.data(),
                                         static_cast<UINT>(plane_strides[0]),
                                         static_cast<UINT>(plane_sizes[0])},
                                        {planar_formats[1], cb_pixels.data(),
                                         static_cast<UINT>(plane_strides[1]),
                                         static_cast<UINT>(plane_sizes[1])},
                                        {planar_formats[2], cr_pixels.data(),
                                         static_cast<UINT>(plane_strides[2]),
                                         static_cast<UINT>(plane_sizes[2])}};
      hr = transform->CopyPixels(
          nullptr, static_cast<UINT>(width), static_cast<UINT>(height),
          WICBitmapTransformRotate0, WICPlanarOptionsDefault, planes, 3);
      if (SUCCEEDED(hr)) {
        if (mode == PILLOW_C_MODE_L) {
          image->pixels = std::move(y_pixels);
        } else if (descriptions[1].Width == static_cast<UINT>(width)) {
          jpeg_interleave_ycbcr_planes(
              y_pixels, plane_strides[0], cb_pixels, plane_strides[1],
              cr_pixels, plane_strides[2], width, height, &image->pixels);
        } else if (descriptions[1].Height * 2u == static_cast<UINT>(height)) {
          const bool interleaved = jpeg_h2v2_fancy_interleave_ycbcr(
              y_pixels, plane_strides[0], cb_pixels, plane_strides[1],
              cr_pixels, plane_strides[2],
              static_cast<int>(descriptions[1].Width),
              static_cast<int>(descriptions[1].Height), width, height,
              &image->pixels);
          if (!interleaved) {
            hr = E_INVALIDARG;
          }
        } else {
          const bool interleaved =
              draft_scale == 8
                  ? jpeg_h2v1_nearest_interleave_ycbcr(
                        y_pixels, plane_strides[0], cb_pixels, plane_strides[1],
                        cr_pixels, plane_strides[2],
                        static_cast<int>(descriptions[1].Width), width, height,
                        &image->pixels)
                  : jpeg_h2v1_fancy_interleave_ycbcr(
                        y_pixels, plane_strides[0], cb_pixels, plane_strides[1],
                        cr_pixels, plane_strides[2],
                        static_cast<int>(descriptions[1].Width), width, height,
                        &image->pixels);
          if (!interleaved) {
            hr = E_INVALIDARG;
          }
        }
      }
    } else {
      ComPtr<IWICBitmapSourceTransform> transform;
      hr = frame->QueryInterface(__uuidof(IWICBitmapSourceTransform),
                                 reinterpret_cast<void **>(transform.put()));
      if (FAILED(hr)) {
        delete image;
        return PILLOW_C_INVALID_ARGUMENT;
      }
      const bool draft_bgr_to_rgb = mode == PILLOW_C_MODE_RGB;
      WICPixelFormatGUID draft_format =
          draft_bgr_to_rgb ? GUID_WICPixelFormat24bppBGR : target_format;
      const WICPixelFormatGUID requested_draft_format = draft_format;
      hr = transform->CopyPixels(
          nullptr, static_cast<UINT>(width), static_cast<UINT>(height),
          &draft_format, WICBitmapTransformRotate0, static_cast<UINT>(stride),
          static_cast<UINT>(image->pixels.size()), image->pixels.data());
      if (SUCCEEDED(hr) && !IsEqualGUID(draft_format, requested_draft_format)) {
        hr = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
      }
      if (SUCCEEDED(hr) && draft_bgr_to_rgb) {
        for (std::size_t i = 0; i < image->pixels.size(); i += 3) {
          std::swap(image->pixels[i], image->pixels[i + 2]);
        }
      }
    }
    if (FAILED(hr)) {
      delete image;
      return PILLOW_C_INVALID_ARGUMENT;
    }
    if (out_draft_scale) {
      *out_draft_scale = draft_scale;
    }
    *out_image = image;
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int open_jpeg_image(const char *path, PillowCImage **out_image) {
  return open_jpeg_image_impl(path, 0, 0, 0, nullptr, out_image);
}
int open_jpeg_draft_image(const char *path, int target_width, int target_height,
                          PillowCImage **out_image, int *out_scale) {
  if (!out_scale) {
    return PILLOW_C_NULL_POINTER;
  }
  return open_jpeg_image_impl(path, target_width, target_height, 0, out_scale,
                              out_image);
}
int open_jpeg_draft_mode_image(const char *path, int mode, int target_width,
                               int target_height, PillowCImage **out_image,
                               int *out_scale) {
  if (!out_scale) {
    return PILLOW_C_NULL_POINTER;
  }
  return open_jpeg_image_impl(path, target_width, target_height, mode,
                              out_scale, out_image);
}
} // namespace pillow_c_jpeg

using namespace pillow_c_jpeg;
extern "C" __declspec(dllexport) int
pillow_c_image_open_jpeg(const char *path, PillowCImage **out_image) {
  return open_jpeg_image(path, out_image);
}
extern "C" __declspec(dllexport) int
pillow_c_image_open_jpeg_draft(const char *path, int target_width,
                               int target_height, PillowCImage **out_image,
                               int *out_scale) {
  return open_jpeg_draft_image(path, target_width, target_height, out_image,
                               out_scale);
}
extern "C" __declspec(dllexport) int
pillow_c_image_open_jpeg_draft_mode(const char *path, int mode,
                                    int target_width, int target_height,
                                    PillowCImage **out_image, int *out_scale) {
  return open_jpeg_draft_mode_image(path, mode, target_width, target_height,
                                    out_image, out_scale);
}
