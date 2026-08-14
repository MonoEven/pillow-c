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
int save_jpeg_l_optimized_huffman(const PillowCImage *image, const char *path,
                                  int quality, bool has_dpi, double dpi_x,
                                  double dpi_y, const int *custom_qtable,
                                  bool optimize_huffman,
                                  int restart_marker_blocks,
                                  int smoothing_factor) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_L || image->channels != 1 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::uint8_t jfif_unit = 0;
  std::uint16_t jfif_x_density = 1;
  std::uint16_t jfif_y_density = 1;
  if (has_dpi) {
    int status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit,
                                       &jfif_x_density, &jfif_y_density);
    if (status != PILLOW_C_OK) {
      return status;
    }
  }
  try {
    int qtable[64] = {};
    int status = PILLOW_C_OK;
    if (custom_qtable) {
      status = jpeg_scaled_custom_qtable(custom_qtable, quality, qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      jpeg_scaled_luminance_qtable(quality, qtable);
    }
    const int blocks_x = (image->width + 7) / 8;
    const int blocks_y = (image->height + 7) / 8;
    std::vector<std::uint8_t> smoothed_plane;
    const bool has_smoothed_plane = smoothing_factor != 0;
    if (has_smoothed_plane) {
      std::vector<std::uint8_t> l_plane;
      l_plane.reserve(static_cast<std::size_t>(image->width) *
                      static_cast<std::size_t>(image->height));
      for (int y = 0; y < image->height; ++y) {
        const std::uint8_t *row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        l_plane.insert(l_plane.end(), row, row + image->width);
      }
      status = jpeg_smooth_fullsize_plane(l_plane, image->width, image->height,
                                          blocks_x * 8, smoothing_factor,
                                          &smoothed_plane);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<int> blocks;
    blocks.reserve(static_cast<std::size_t>(blocks_x) *
                   static_cast<std::size_t>(blocks_y) * 64u);
    for (int by = 0; by < blocks_y; ++by) {
      for (int bx = 0; bx < blocks_x; ++bx) {
        int zz[64] = {};
        if (has_smoothed_plane) {
          jpeg_fdct_quantize_plane_block(smoothed_plane, image->width,
                                         image->height, bx * 8, by * 8, qtable,
                                         zz);
        } else {
          jpeg_fdct_quantize_luma_block(image, bx * 8, by * 8, qtable, zz);
        }
        blocks.insert(blocks.end(), zz, zz + 64);
      }
    }
    JpegHuffmanTable dc_table;
    JpegHuffmanTable ac_table;
    if (optimize_huffman) {
      std::uint64_t dc_freq[256] = {};
      std::uint64_t ac_freq[256] = {};
      jpeg_collect_huffman_frequencies(
          blocks, dc_freq, ac_freq,
          static_cast<std::size_t>(restart_marker_blocks));
      status = jpeg_build_optimized_huffman_table(dc_freq, &dc_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_build_optimized_huffman_table(ac_freq, &ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      status =
          jpeg_build_standard_luminance_huffman_tables(&dc_table, &ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> out;
    out.reserve(256u + blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    std::uint8_t app0[] = {
        'J',
        'F',
        'I',
        'F',
        0,
        1,
        1,
        jfif_unit,
        static_cast<std::uint8_t>((jfif_x_density >> 8) & 0xffu),
        static_cast<std::uint8_t>(jfif_x_density & 0xffu),
        static_cast<std::uint8_t>((jfif_y_density >> 8) & 0xffu),
        static_cast<std::uint8_t>(jfif_y_density & 0xffu),
        0,
        0};
    status = append_jpeg_segment(out, 0xe0u, app0, sizeof(app0));
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::uint8_t dqt[65] = {};
    dqt[0] = 0;
    for (int i = 0; i < 64; ++i) {
      dqt[i + 1] = static_cast<std::uint8_t>(qtable[JPEG_ZIGZAG[i]]);
    }
    status = append_jpeg_segment(out, 0xdbu, dqt, sizeof(dqt));
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> sof;
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(1);
    sof.push_back(1);
    sof.push_back(0x11);
    sof.push_back(0);
    status = append_jpeg_segment(out, 0xc0u, sof.data(), sof.size());
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 0, dc_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, ac_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const bool has_restart_interval = restart_marker_blocks != 0;
    const std::uint16_t restart_interval =
        static_cast<std::uint16_t>(restart_marker_blocks);
    if (has_restart_interval) {
      const std::uint8_t dri[] = {
          static_cast<std::uint8_t>((restart_interval >> 8) & 0xffu),
          static_cast<std::uint8_t>(restart_interval & 0xffu)};
      status = append_jpeg_segment(out, 0xddu, dri, sizeof(dri));
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::uint8_t sos[] = {1, 1, 0, 0, 63, 0};
    status = append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_luma_entropy(blocks, dc_table, ac_table,
                                      restart_interval, &out);
    if (status != PILLOW_C_OK) {
      return status;
    }
    out.push_back(0xffu);
    out.push_back(0xd9u);
    return write_binary_file(path, out) ? PILLOW_C_OK
                                        : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int save_jpeg_l_progressive_huffman(const PillowCImage *image, const char *path,
                                    int quality, bool has_dpi, double dpi_x,
                                    double dpi_y, const int *custom_qtable,
                                    int restart_marker_blocks,
                                    int smoothing_factor) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_L || image->channels != 1 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max() ||
      restart_marker_blocks < 0 ||
      restart_marker_blocks > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::uint8_t jfif_unit = 0;
  std::uint16_t jfif_x_density = 1;
  std::uint16_t jfif_y_density = 1;
  if (has_dpi) {
    int status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit,
                                       &jfif_x_density, &jfif_y_density);
    if (status != PILLOW_C_OK) {
      return status;
    }
  }
  try {
    int qtable[64] = {};
    int status = PILLOW_C_OK;
    if (custom_qtable) {
      status = jpeg_scaled_custom_qtable(custom_qtable, quality, qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      jpeg_scaled_luminance_qtable(quality, qtable);
    }
    const int blocks_x = (image->width + 7) / 8;
    const int blocks_y = (image->height + 7) / 8;
    std::vector<std::uint8_t> smoothed_plane;
    const bool has_smoothed_plane = smoothing_factor != 0;
    if (has_smoothed_plane) {
      std::vector<std::uint8_t> l_plane;
      l_plane.reserve(static_cast<std::size_t>(image->width) *
                      static_cast<std::size_t>(image->height));
      for (int y = 0; y < image->height; ++y) {
        const std::uint8_t *row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        l_plane.insert(l_plane.end(), row, row + image->width);
      }
      status = jpeg_smooth_fullsize_plane(l_plane, image->width, image->height,
                                          blocks_x * 8, smoothing_factor,
                                          &smoothed_plane);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<int> blocks;
    blocks.reserve(static_cast<std::size_t>(blocks_x) *
                   static_cast<std::size_t>(blocks_y) * 64u);
    for (int by = 0; by < blocks_y; ++by) {
      for (int bx = 0; bx < blocks_x; ++bx) {
        int zz[64] = {};
        if (has_smoothed_plane) {
          jpeg_fdct_quantize_plane_block(smoothed_plane, image->width,
                                         image->height, bx * 8, by * 8, qtable,
                                         zz);
        } else {
          jpeg_fdct_quantize_luma_block(image, bx * 8, by * 8, qtable, zz);
        }
        blocks.insert(blocks.end(), zz, zz + 64);
      }
    }
    std::uint64_t dc_first_freq[256] = {};
    std::uint64_t ac_first_low_freq[256] = {};
    std::uint64_t ac_first_high_freq[256] = {};
    std::uint64_t ac_refine_mid_freq[256] = {};
    std::uint64_t ac_refine_final_freq[256] = {};
    jpeg_collect_progressive_dc_first_frequencies(
        blocks, 1, dc_first_freq,
        static_cast<std::size_t>(restart_marker_blocks));
    jpeg_collect_progressive_ac_first_frequencies(
        blocks, 1, 5, 2, ac_first_low_freq,
        static_cast<std::size_t>(restart_marker_blocks));
    jpeg_collect_progressive_ac_first_frequencies(
        blocks, 6, 63, 2, ac_first_high_freq,
        static_cast<std::size_t>(restart_marker_blocks));
    jpeg_collect_progressive_ac_refine_frequencies(
        blocks, 1, 63, 2, 1, ac_refine_mid_freq,
        static_cast<std::size_t>(restart_marker_blocks));
    jpeg_collect_progressive_ac_refine_frequencies(
        blocks, 1, 63, 1, 0, ac_refine_final_freq,
        static_cast<std::size_t>(restart_marker_blocks));
    JpegHuffmanTable dc_first_table;
    JpegHuffmanTable ac_first_low_table;
    JpegHuffmanTable ac_first_high_table;
    JpegHuffmanTable ac_refine_mid_table;
    JpegHuffmanTable ac_refine_final_table;
    status = jpeg_build_optimized_huffman_table(dc_first_freq, &dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(ac_first_low_freq,
                                                &ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(ac_first_high_freq,
                                                &ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(ac_refine_mid_freq,
                                                &ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(ac_refine_final_freq,
                                                &ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> out;
    out.reserve(384u + blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    std::uint8_t app0[] = {
        'J',
        'F',
        'I',
        'F',
        0,
        1,
        1,
        jfif_unit,
        static_cast<std::uint8_t>((jfif_x_density >> 8) & 0xffu),
        static_cast<std::uint8_t>(jfif_x_density & 0xffu),
        static_cast<std::uint8_t>((jfif_y_density >> 8) & 0xffu),
        static_cast<std::uint8_t>(jfif_y_density & 0xffu),
        0,
        0};
    status = append_jpeg_segment(out, 0xe0u, app0, sizeof(app0));
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::uint8_t dqt[65] = {};
    dqt[0] = 0;
    for (int i = 0; i < 64; ++i) {
      dqt[i + 1] = static_cast<std::uint8_t>(qtable[JPEG_ZIGZAG[i]]);
    }
    status = append_jpeg_segment(out, 0xdbu, dqt, sizeof(dqt));
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> sof;
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(1);
    sof.push_back(1);
    sof.push_back(0x11);
    sof.push_back(0);
    status = append_jpeg_segment(out, 0xc2u, sof.data(), sof.size());
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 0, dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const std::uint16_t restart_interval =
        static_cast<std::uint16_t>(restart_marker_blocks);
    if (restart_interval != 0u) {
      status = jpeg_append_dri_segment(out, restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_luminance_sos_segment(out, 0, 0, 0, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_dc_first_scan(blocks, 1, dc_first_table,
                                                   &out, restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_sos_segment(out, 1, 5, 0, 2);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_first_scan(
        blocks, 1, 5, 2, ac_first_low_table, &out, restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_sos_segment(out, 6, 63, 0, 2);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_first_scan(
        blocks, 6, 63, 2, ac_first_high_table, &out, restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_sos_segment(out, 1, 63, 2, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_refine_scan(
        blocks, 1, 63, 2, 1, ac_refine_mid_table, &out, restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_sos_segment(out, 0, 0, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_dc_refine_scan(blocks, 0, &out,
                                                    restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_sos_segment(out, 1, 63, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_refine_scan(
        blocks, 1, 63, 1, 0, ac_refine_final_table, &out, restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    out.push_back(0xffu);
    out.push_back(0xd9u);
    return write_binary_file(path, out) ? PILLOW_C_OK
                                        : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
} // namespace pillow_c_jpeg
