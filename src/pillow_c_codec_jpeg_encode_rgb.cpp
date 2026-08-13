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
std::uint8_t jpeg_clamp_sample(double value) {
  const int rounded = static_cast<int>(std::round(value));
  return static_cast<std::uint8_t>(std::max(0, std::min(255, rounded)));
}
int jpeg_rgb_sampling_from_subsampling(int subsampling, int *out_h,
                                       int *out_v) {
  if (!out_h || !out_v) {
    return PILLOW_C_NULL_POINTER;
  }
  switch (subsampling) {
  case -1:
  case 2:
    *out_h = 2;
    *out_v = 2;
    return PILLOW_C_OK;
  case 1:
    *out_h = 2;
    *out_v = 1;
    return PILLOW_C_OK;
  case 0:
    *out_h = 1;
    *out_v = 1;
    return PILLOW_C_OK;
  default:
    return PILLOW_C_INVALID_ARGUMENT;
  }
}
struct JpegRgbPreparedBlocks {
  int h_samp = 0;
  int v_samp = 0;
  int y_blocks_per_mcu = 0;
  int luma_qtable[64] = {};
  int chroma_qtable[64] = {};
  std::vector<int> y_blocks;
  std::vector<int> cb_blocks;
  std::vector<int> cr_blocks;
};
int jpeg_prepare_rgb_sampled_blocks(const PillowCImage *image, int quality,
                                    int subsampling,
                                    const int *custom_luma_qtable,
                                    const int *custom_chroma_qtable,
                                    JpegRgbPreparedBlocks *prepared) {
  if (!image || !prepared) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  int h_samp = 0;
  int v_samp = 0;
  int status =
      jpeg_rgb_sampling_from_subsampling(subsampling, &h_samp, &v_samp);
  if (status != PILLOW_C_OK) {
    return status;
  }
  try {
    *prepared = JpegRgbPreparedBlocks{};
    prepared->h_samp = h_samp;
    prepared->v_samp = v_samp;
    prepared->y_blocks_per_mcu = h_samp * v_samp;
    if (custom_luma_qtable) {
      status = jpeg_scaled_custom_qtable(custom_luma_qtable, quality,
                                         prepared->luma_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      jpeg_scaled_luminance_qtable(quality, prepared->luma_qtable);
    }
    if (custom_chroma_qtable) {
      status = jpeg_scaled_custom_qtable(custom_chroma_qtable, quality,
                                         prepared->chroma_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      jpeg_scaled_chrominance_qtable(quality, prepared->chroma_qtable);
    }
    const std::size_t pixel_count = static_cast<std::size_t>(image->width) *
                                    static_cast<std::size_t>(image->height);
    const int chroma_width = (image->width + h_samp - 1) / h_samp;
    const int chroma_height = (image->height + v_samp - 1) / v_samp;
    const int mcu_cols = (image->width + (8 * h_samp) - 1) / (8 * h_samp);
    const int mcu_rows = (image->height + (8 * v_samp) - 1) / (8 * v_samp);
    const std::size_t mcu_count =
        static_cast<std::size_t>(mcu_cols) * static_cast<std::size_t>(mcu_rows);
    if (mcu_count > std::numeric_limits<std::size_t>::max() / 64u ||
        mcu_count >
            std::numeric_limits<std::size_t>::max() /
                (64u * static_cast<std::size_t>(prepared->y_blocks_per_mcu))) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> y_plane(pixel_count);
    std::vector<std::uint8_t> cb_full(pixel_count);
    std::vector<std::uint8_t> cr_full(pixel_count);
    for (int y = 0; y < image->height; ++y) {
      const std::uint8_t *row =
          image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
      const std::size_t row_offset =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(image->width);
      for (int x = 0; x < image->width; ++x) {
        const std::uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const double r = static_cast<double>(pixel[0]);
        const double g = static_cast<double>(pixel[1]);
        const double b = static_cast<double>(pixel[2]);
        const std::size_t index = row_offset + static_cast<std::size_t>(x);
        y_plane[index] =
            jpeg_clamp_sample((0.299 * r) + (0.587 * g) + (0.114 * b));
        cb_full[index] = jpeg_clamp_sample((-0.168736 * r) - (0.331264 * g) +
                                           (0.5 * b) + 128.0);
        cr_full[index] = jpeg_clamp_sample((0.5 * r) - (0.418688 * g) -
                                           (0.081312 * b) + 128.0);
      }
    }
    const std::size_t chroma_count = static_cast<std::size_t>(chroma_width) *
                                     static_cast<std::size_t>(chroma_height);
    std::vector<std::uint8_t> cb_plane(chroma_count);
    std::vector<std::uint8_t> cr_plane(chroma_count);
    for (int cy = 0; cy < chroma_height; ++cy) {
      for (int cx = 0; cx < chroma_width; ++cx) {
        int cb_sum = 0;
        int cr_sum = 0;
        for (int dy = 0; dy < v_samp; ++dy) {
          const int src_y = std::min((cy * v_samp) + dy, image->height - 1);
          const std::size_t src_row = static_cast<std::size_t>(src_y) *
                                      static_cast<std::size_t>(image->width);
          for (int dx = 0; dx < h_samp; ++dx) {
            const int src_x = std::min((cx * h_samp) + dx, image->width - 1);
            const std::size_t src_index =
                src_row + static_cast<std::size_t>(src_x);
            cb_sum += cb_full[src_index];
            cr_sum += cr_full[src_index];
          }
        }
        const int divisor = h_samp * v_samp;
        const int downsample_bias = h_samp == 2 && v_samp == 1   ? (cx & 1)
                                    : h_samp == 2 && v_samp == 2 ? 1 + (cx & 1)
                                                                 : divisor / 2;
        const std::size_t dst_index =
            static_cast<std::size_t>(cy) *
                static_cast<std::size_t>(chroma_width) +
            static_cast<std::size_t>(cx);
        cb_plane[dst_index] =
            static_cast<std::uint8_t>((cb_sum + downsample_bias) / divisor);
        cr_plane[dst_index] =
            static_cast<std::uint8_t>((cr_sum + downsample_bias) / divisor);
      }
    }
    prepared->y_blocks.reserve(
        mcu_count * static_cast<std::size_t>(prepared->y_blocks_per_mcu) * 64u);
    prepared->cb_blocks.reserve(mcu_count * 64u);
    prepared->cr_blocks.reserve(mcu_count * 64u);
    for (int mcu_y = 0; mcu_y < mcu_rows; ++mcu_y) {
      for (int mcu_x = 0; mcu_x < mcu_cols; ++mcu_x) {
        for (int y_sample = 0; y_sample < v_samp; ++y_sample) {
          for (int x_sample = 0; x_sample < h_samp; ++x_sample) {
            int zz[64] = {};
            jpeg_fdct_quantize_plane_block(y_plane, image->width, image->height,
                                           ((mcu_x * h_samp) + x_sample) * 8,
                                           ((mcu_y * v_samp) + y_sample) * 8,
                                           prepared->luma_qtable, zz);
            prepared->y_blocks.insert(prepared->y_blocks.end(), zz, zz + 64);
          }
        }
        int zz[64] = {};
        jpeg_fdct_quantize_plane_block(cb_plane, chroma_width, chroma_height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->chroma_qtable, zz);
        prepared->cb_blocks.insert(prepared->cb_blocks.end(), zz, zz + 64);
        jpeg_fdct_quantize_plane_block(cr_plane, chroma_width, chroma_height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->chroma_qtable, zz);
        prepared->cr_blocks.insert(prepared->cr_blocks.end(), zz, zz + 64);
      }
    }
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_prepare_keep_rgb_blocks(const PillowCImage *image, int quality,
                                 const int *custom_r_qtable,
                                 const int *custom_gb_qtable,
                                 JpegRgbPreparedBlocks *prepared) {
  if (!image || !prepared) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    *prepared = JpegRgbPreparedBlocks{};
    prepared->h_samp = 1;
    prepared->v_samp = 1;
    prepared->y_blocks_per_mcu = 1;
    int status = PILLOW_C_OK;
    if (custom_r_qtable) {
      status = jpeg_scaled_custom_qtable(custom_r_qtable, quality,
                                         prepared->luma_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      jpeg_scaled_luminance_qtable(quality, prepared->luma_qtable);
    }
    if (custom_gb_qtable) {
      status = jpeg_scaled_custom_qtable(custom_gb_qtable, quality,
                                         prepared->chroma_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      for (int i = 0; i < 64; ++i) {
        prepared->chroma_qtable[i] = prepared->luma_qtable[i];
      }
    }
    const std::size_t pixel_count = static_cast<std::size_t>(image->width) *
                                    static_cast<std::size_t>(image->height);
    const int mcu_cols = (image->width + 7) / 8;
    const int mcu_rows = (image->height + 7) / 8;
    const std::size_t mcu_count =
        static_cast<std::size_t>(mcu_cols) * static_cast<std::size_t>(mcu_rows);
    if (mcu_count > std::numeric_limits<std::size_t>::max() / 64u) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> r_plane(pixel_count);
    std::vector<std::uint8_t> g_plane(pixel_count);
    std::vector<std::uint8_t> b_plane(pixel_count);
    for (int y = 0; y < image->height; ++y) {
      const std::uint8_t *row =
          image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
      const std::size_t row_offset =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(image->width);
      for (int x = 0; x < image->width; ++x) {
        const std::uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const std::size_t index = row_offset + static_cast<std::size_t>(x);
        r_plane[index] = pixel[0];
        g_plane[index] = pixel[1];
        b_plane[index] = pixel[2];
      }
    }
    prepared->y_blocks.reserve(mcu_count * 64u);
    prepared->cb_blocks.reserve(mcu_count * 64u);
    prepared->cr_blocks.reserve(mcu_count * 64u);
    for (int mcu_y = 0; mcu_y < mcu_rows; ++mcu_y) {
      for (int mcu_x = 0; mcu_x < mcu_cols; ++mcu_x) {
        int zz[64] = {};
        jpeg_fdct_quantize_plane_block(r_plane, image->width, image->height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->luma_qtable, zz);
        prepared->y_blocks.insert(prepared->y_blocks.end(), zz, zz + 64);
        jpeg_fdct_quantize_plane_block(g_plane, image->width, image->height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->chroma_qtable, zz);
        prepared->cb_blocks.insert(prepared->cb_blocks.end(), zz, zz + 64);
        jpeg_fdct_quantize_plane_block(b_plane, image->width, image->height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->chroma_qtable, zz);
        prepared->cr_blocks.insert(prepared->cr_blocks.end(), zz, zz + 64);
      }
    }
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_append_adobe_rgb_app14(std::vector<std::uint8_t> &out) {
  const std::uint8_t app14[] = {'A', 'd', 'o', 'b', 'e', 0, 100, 0, 0, 0, 0, 0};
  return append_jpeg_segment(out, 0xeeu, app14, sizeof(app14));
}
int jpeg_append_jfif_app0(std::vector<std::uint8_t> &out, std::uint8_t unit,
                          std::uint16_t x_density, std::uint16_t y_density) {
  const std::uint8_t app0[] = {
      'J',
      'F',
      'I',
      'F',
      0,
      1,
      1,
      unit,
      static_cast<std::uint8_t>((x_density >> 8) & 0xffu),
      static_cast<std::uint8_t>(x_density & 0xffu),
      static_cast<std::uint8_t>((y_density >> 8) & 0xffu),
      static_cast<std::uint8_t>(y_density & 0xffu),
      0,
      0};
  return append_jpeg_segment(out, 0xe0u, app0, sizeof(app0));
}
int jpeg_append_optional_jfif_app0(std::vector<std::uint8_t> &out, bool has_dpi,
                                   double dpi_x, double dpi_y) {
  if (!has_dpi) {
    return PILLOW_C_OK;
  }
  std::uint8_t jfif_unit = 0;
  std::uint16_t jfif_x_density = 0;
  std::uint16_t jfif_y_density = 0;
  int status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit, &jfif_x_density,
                                     &jfif_y_density);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return jpeg_append_jfif_app0(out, jfif_unit, jfif_x_density, jfif_y_density);
}
int jpeg_append_dqt_table(std::vector<std::uint8_t> &out, int table_id,
                          const int qtable[64]) {
  if (table_id < 0 || table_id > 3 || !qtable) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::uint8_t dqt[65] = {};
  dqt[0] = static_cast<std::uint8_t>(table_id);
  for (int i = 0; i < 64; ++i) {
    dqt[i + 1] = static_cast<std::uint8_t>(qtable[JPEG_ZIGZAG[i]]);
  }
  return append_jpeg_segment(out, 0xdbu, dqt, sizeof(dqt));
}
int jpeg_append_luminance_dqt(std::vector<std::uint8_t> &out,
                              const int qtable[64]) {
  return jpeg_append_dqt_table(out, 0, qtable);
}
int jpeg_append_keep_rgb_sof(std::vector<std::uint8_t> &out,
                             const PillowCImage *image, std::uint8_t marker,
                             int r_qtable_id, int g_qtable_id,
                             int b_qtable_id) {
  if (!image) {
    return PILLOW_C_NULL_POINTER;
  }
  if (r_qtable_id < 0 || r_qtable_id > 3 || g_qtable_id < 0 ||
      g_qtable_id > 3 || b_qtable_id < 0 || b_qtable_id > 3) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::vector<std::uint8_t> sof;
  try {
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(3);
    sof.push_back('R');
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(r_qtable_id));
    sof.push_back('G');
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(g_qtable_id));
    sof.push_back('B');
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(b_qtable_id));
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
  return append_jpeg_segment(out, marker, sof.data(), sof.size());
}
int jpeg_append_keep_rgb_interleaved_sos_segment(std::vector<std::uint8_t> &out,
                                                 int r_selector, int g_selector,
                                                 int b_selector, int ss, int se,
                                                 int ah, int al) {
  std::uint8_t sos[] = {3,
                        static_cast<std::uint8_t>('R'),
                        static_cast<std::uint8_t>(r_selector),
                        static_cast<std::uint8_t>('G'),
                        static_cast<std::uint8_t>(g_selector),
                        static_cast<std::uint8_t>('B'),
                        static_cast<std::uint8_t>(b_selector),
                        static_cast<std::uint8_t>(ss),
                        static_cast<std::uint8_t>(se),
                        static_cast<std::uint8_t>((ah << 4) | al)};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int jpeg_append_keep_rgb_single_sos_segment(std::vector<std::uint8_t> &out,
                                            int component_id,
                                            int table_selector, int ss, int se,
                                            int ah, int al) {
  std::uint8_t sos[] = {1,
                        static_cast<std::uint8_t>(component_id),
                        static_cast<std::uint8_t>(table_selector),
                        static_cast<std::uint8_t>(ss),
                        static_cast<std::uint8_t>(se),
                        static_cast<std::uint8_t>((ah << 4) | al)};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int save_jpeg_rgb_progressive_huffman(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, int subsampling, const int *custom_luma_qtable,
    const int *custom_chroma_qtable, int chroma_qtable_id,
    int restart_marker_blocks, int restart_marker_rows);
int save_jpeg_rgb_optimized_huffman(const PillowCImage *image, const char *path,
                                    int quality, bool has_dpi, double dpi_x,
                                    double dpi_y, int subsampling,
                                    bool optimize_huffman,
                                    int restart_marker_blocks) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  int h_samp = 0;
  int v_samp = 0;
  int status =
      jpeg_rgb_sampling_from_subsampling(subsampling, &h_samp, &v_samp);
  if (status != PILLOW_C_OK) {
    return status;
  }
  std::uint8_t jfif_unit = 0;
  std::uint16_t jfif_x_density = 1;
  std::uint16_t jfif_y_density = 1;
  if (has_dpi) {
    status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit, &jfif_x_density,
                                   &jfif_y_density);
    if (status != PILLOW_C_OK) {
      return status;
    }
  }
  try {
    int luma_qtable[64] = {};
    int chroma_qtable[64] = {};
    jpeg_scaled_luminance_qtable(quality, luma_qtable);
    jpeg_scaled_chrominance_qtable(quality, chroma_qtable);
    const std::size_t pixel_count = static_cast<std::size_t>(image->width) *
                                    static_cast<std::size_t>(image->height);
    const int chroma_width = (image->width + h_samp - 1) / h_samp;
    const int chroma_height = (image->height + v_samp - 1) / v_samp;
    const int mcu_cols = (image->width + (8 * h_samp) - 1) / (8 * h_samp);
    const int mcu_rows = (image->height + (8 * v_samp) - 1) / (8 * v_samp);
    const int y_blocks_per_mcu = h_samp * v_samp;
    const std::size_t mcu_count =
        static_cast<std::size_t>(mcu_cols) * static_cast<std::size_t>(mcu_rows);
    if (mcu_count > std::numeric_limits<std::size_t>::max() / 64u ||
        mcu_count > std::numeric_limits<std::size_t>::max() /
                        (64u * static_cast<std::size_t>(y_blocks_per_mcu))) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> y_plane(pixel_count);
    std::vector<std::uint8_t> cb_full(pixel_count);
    std::vector<std::uint8_t> cr_full(pixel_count);
    for (int y = 0; y < image->height; ++y) {
      const std::uint8_t *row =
          image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
      const std::size_t row_offset =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(image->width);
      for (int x = 0; x < image->width; ++x) {
        const std::uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const double r = static_cast<double>(pixel[0]);
        const double g = static_cast<double>(pixel[1]);
        const double b = static_cast<double>(pixel[2]);
        const std::size_t index = row_offset + static_cast<std::size_t>(x);
        y_plane[index] =
            jpeg_clamp_sample((0.299 * r) + (0.587 * g) + (0.114 * b));
        cb_full[index] = jpeg_clamp_sample((-0.168736 * r) - (0.331264 * g) +
                                           (0.5 * b) + 128.0);
        cr_full[index] = jpeg_clamp_sample((0.5 * r) - (0.418688 * g) -
                                           (0.081312 * b) + 128.0);
      }
    }
    const std::size_t chroma_count = static_cast<std::size_t>(chroma_width) *
                                     static_cast<std::size_t>(chroma_height);
    std::vector<std::uint8_t> cb_plane(chroma_count);
    std::vector<std::uint8_t> cr_plane(chroma_count);
    for (int cy = 0; cy < chroma_height; ++cy) {
      for (int cx = 0; cx < chroma_width; ++cx) {
        int cb_sum = 0;
        int cr_sum = 0;
        for (int dy = 0; dy < v_samp; ++dy) {
          const int src_y = std::min((cy * v_samp) + dy, image->height - 1);
          const std::size_t src_row = static_cast<std::size_t>(src_y) *
                                      static_cast<std::size_t>(image->width);
          for (int dx = 0; dx < h_samp; ++dx) {
            const int src_x = std::min((cx * h_samp) + dx, image->width - 1);
            const std::size_t src_index =
                src_row + static_cast<std::size_t>(src_x);
            cb_sum += cb_full[src_index];
            cr_sum += cr_full[src_index];
          }
        }
        const int divisor = h_samp * v_samp;
        const std::size_t dst_index =
            static_cast<std::size_t>(cy) *
                static_cast<std::size_t>(chroma_width) +
            static_cast<std::size_t>(cx);
        cb_plane[dst_index] =
            static_cast<std::uint8_t>((cb_sum + (divisor / 2)) / divisor);
        cr_plane[dst_index] =
            static_cast<std::uint8_t>((cr_sum + (divisor / 2)) / divisor);
      }
    }
    std::vector<int> y_blocks;
    std::vector<int> cb_blocks;
    std::vector<int> cr_blocks;
    y_blocks.reserve(mcu_count * static_cast<std::size_t>(y_blocks_per_mcu) *
                     64u);
    cb_blocks.reserve(mcu_count * 64u);
    cr_blocks.reserve(mcu_count * 64u);
    for (int mcu_y = 0; mcu_y < mcu_rows; ++mcu_y) {
      for (int mcu_x = 0; mcu_x < mcu_cols; ++mcu_x) {
        for (int y_sample = 0; y_sample < v_samp; ++y_sample) {
          for (int x_sample = 0; x_sample < h_samp; ++x_sample) {
            int zz[64] = {};
            jpeg_fdct_quantize_plane_block(y_plane, image->width, image->height,
                                           ((mcu_x * h_samp) + x_sample) * 8,
                                           ((mcu_y * v_samp) + y_sample) * 8,
                                           luma_qtable, zz);
            y_blocks.insert(y_blocks.end(), zz, zz + 64);
          }
        }
        int zz[64] = {};
        jpeg_fdct_quantize_plane_block(cb_plane, chroma_width, chroma_height,
                                       mcu_x * 8, mcu_y * 8, chroma_qtable, zz);
        cb_blocks.insert(cb_blocks.end(), zz, zz + 64);
        jpeg_fdct_quantize_plane_block(cr_plane, chroma_width, chroma_height,
                                       mcu_x * 8, mcu_y * 8, chroma_qtable, zz);
        cr_blocks.insert(cr_blocks.end(), zz, zz + 64);
      }
    }
    JpegHuffmanTable luma_dc_table;
    JpegHuffmanTable luma_ac_table;
    JpegHuffmanTable chroma_dc_table;
    JpegHuffmanTable chroma_ac_table;
    if (optimize_huffman) {
      std::uint64_t luma_dc_freq[256] = {};
      std::uint64_t luma_ac_freq[256] = {};
      std::uint64_t chroma_dc_freq[256] = {};
      std::uint64_t chroma_ac_freq[256] = {};
      const std::size_t restart_interval =
          static_cast<std::size_t>(restart_marker_blocks);
      const std::size_t luma_restart_interval =
          restart_interval * static_cast<std::size_t>(y_blocks_per_mcu);
      jpeg_collect_huffman_frequencies(y_blocks, luma_dc_freq, luma_ac_freq,
                                       luma_restart_interval);
      jpeg_collect_huffman_frequencies(cb_blocks, chroma_dc_freq,
                                       chroma_ac_freq, restart_interval);
      jpeg_collect_huffman_frequencies(cr_blocks, chroma_dc_freq,
                                       chroma_ac_freq, restart_interval);
      status = jpeg_build_optimized_huffman_table(luma_dc_freq, &luma_dc_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_build_optimized_huffman_table(luma_ac_freq, &luma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status =
          jpeg_build_optimized_huffman_table(chroma_dc_freq, &chroma_dc_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status =
          jpeg_build_optimized_huffman_table(chroma_ac_freq, &chroma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      status = jpeg_build_standard_luminance_huffman_tables(&luma_dc_table,
                                                            &luma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_build_standard_chrominance_huffman_tables(&chroma_dc_table,
                                                              &chroma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> out;
    out.reserve(512u + y_blocks.size() + cb_blocks.size() + cr_blocks.size());
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
    std::uint8_t dqt_luma[65] = {};
    dqt_luma[0] = 0;
    for (int i = 0; i < 64; ++i) {
      dqt_luma[i + 1] = static_cast<std::uint8_t>(luma_qtable[JPEG_ZIGZAG[i]]);
    }
    status = append_jpeg_segment(out, 0xdbu, dqt_luma, sizeof(dqt_luma));
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::uint8_t dqt_chroma[65] = {};
    dqt_chroma[0] = 1;
    for (int i = 0; i < 64; ++i) {
      dqt_chroma[i + 1] =
          static_cast<std::uint8_t>(chroma_qtable[JPEG_ZIGZAG[i]]);
    }
    status = append_jpeg_segment(out, 0xdbu, dqt_chroma, sizeof(dqt_chroma));
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> sof;
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(3);
    sof.push_back(1);
    sof.push_back(static_cast<std::uint8_t>((h_samp << 4) | v_samp));
    sof.push_back(0);
    sof.push_back(2);
    sof.push_back(0x11);
    sof.push_back(1);
    sof.push_back(3);
    sof.push_back(0x11);
    sof.push_back(1);
    status = append_jpeg_segment(out, 0xc0u, sof.data(), sof.size());
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 0, luma_dc_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, luma_ac_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 1, chroma_dc_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 1, chroma_ac_table);
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
    std::uint8_t sos[] = {3, 1, 0x00, 2, 0x11, 3, 0x11, 0, 63, 0};
    status = append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_rgb_interleaved_entropy(
        y_blocks, cb_blocks, cr_blocks, y_blocks_per_mcu, luma_dc_table,
        luma_ac_table, chroma_dc_table, chroma_ac_table, &out,
        restart_interval);
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
int save_jpeg_cmyk_baseline(const PillowCImage *image, const char *path,
                            int quality, const int *qtables,
                            std::size_t qtable_count, bool optimize,
                            bool has_dpi, double dpi_x, double dpi_y,
                            int subsampling, std::uint16_t restart_interval);
int save_jpeg_cmyk_progressive(const PillowCImage *image, const char *path,
                               int quality, bool has_dpi, double dpi_x,
                               double dpi_y, const int *qtables,
                               std::size_t qtable_count, int subsampling,
                               std::uint16_t restart_interval,
                               std::uint16_t c_ac_restart_interval);
int save_jpeg_rgb_qtables_optimized_huffman(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize,
    bool allow_cmyk_progressive_subsampling, int restart_marker_blocks,
    int restart_marker_rows) {
  if (!image || !path || !qtables) {
    return PILLOW_C_NULL_POINTER;
  }
  const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
  if (refresh_status != PILLOW_C_OK) {
    return refresh_status;
  }
  if (qtable_count < 1u || qtable_count > 2u || progressive < -1 ||
      progressive > 1 || optimize < -1 || optimize > 1 ||
      restart_marker_blocks < 0 ||
      restart_marker_blocks > std::numeric_limits<std::uint16_t>::max() ||
      restart_marker_rows < 0 ||
      (restart_marker_blocks != 0 && restart_marker_rows != 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
    if (!(subsampling == -1 || subsampling == 0 || subsampling == 1 ||
          subsampling == 2)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const int c_h_samp = (subsampling == 1 || subsampling == 2) ? 2 : 1;
    std::uint64_t restart_interval_value =
        static_cast<std::uint64_t>(restart_marker_blocks);
    std::uint64_t c_ac_restart_interval_value = restart_interval_value;
    if (restart_marker_rows != 0) {
      if (image->width <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      const std::uint64_t mcu_width = static_cast<std::uint64_t>(8 * c_h_samp);
      const std::uint64_t mcu_columns =
          (static_cast<std::uint64_t>(image->width) + mcu_width - 1u) /
          mcu_width;
      restart_interval_value =
          mcu_columns * static_cast<std::uint64_t>(restart_marker_rows);
      const std::uint64_t c_block_columns =
          (static_cast<std::uint64_t>(image->width) + 7u) / 8u;
      c_ac_restart_interval_value =
          c_block_columns * static_cast<std::uint64_t>(restart_marker_rows);
    }
    if (restart_interval_value > std::numeric_limits<std::uint16_t>::max() ||
        c_ac_restart_interval_value >
            std::numeric_limits<std::uint16_t>::max()) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint16_t restart_interval =
        static_cast<std::uint16_t>(restart_interval_value);
    const std::uint16_t c_ac_restart_interval =
        static_cast<std::uint16_t>(c_ac_restart_interval_value);
    if (progressive == 1) {
      if (!(subsampling == -1 || subsampling == 0)) {
        if (!allow_cmyk_progressive_subsampling) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
      }
      return save_jpeg_cmyk_progressive(
          image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
          subsampling, restart_interval, c_ac_restart_interval);
    }
    return save_jpeg_cmyk_baseline(image, path, quality, qtables, qtable_count,
                                   optimize == 1, has_dpi, dpi_x, dpi_y,
                                   subsampling, restart_interval);
  }
  if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
    if (subsampling != -1) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::uint64_t restart_interval_value =
        static_cast<std::uint64_t>(restart_marker_blocks);
    if (restart_marker_rows != 0) {
      if (image->width <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      const std::uint64_t mcu_columns =
          static_cast<std::uint64_t>((image->width + 7) / 8);
      restart_interval_value =
          mcu_columns * static_cast<std::uint64_t>(restart_marker_rows);
    }
    if (restart_interval_value > std::numeric_limits<std::uint16_t>::max()) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const int restart_interval = static_cast<int>(restart_interval_value);
    if (progressive == 1) {
      return save_jpeg_l_progressive_huffman(image, path, quality, has_dpi,
                                             dpi_x, dpi_y, qtables,
                                             restart_interval);
    }
    return save_jpeg_l_optimized_huffman(image, path, quality, has_dpi, dpi_x,
                                         dpi_y, qtables, optimize == 1,
                                         restart_interval);
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const int *chroma_qtable = qtable_count > 1u ? qtables + 64 : qtables;
  const int chroma_qtable_id = qtable_count > 1u ? 1 : 0;
  if (progressive == 1) {
    return save_jpeg_rgb_progressive_huffman(
        image, path, quality, has_dpi, dpi_x, dpi_y, subsampling, qtables,
        chroma_qtable, chroma_qtable_id, restart_marker_blocks,
        restart_marker_rows);
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
    JpegRgbPreparedBlocks prepared;
    int status = jpeg_prepare_rgb_sampled_blocks(
        image, quality, subsampling, qtables, chroma_qtable, &prepared);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::uint64_t restart_interval_value =
        static_cast<std::uint64_t>(restart_marker_blocks);
    if (restart_marker_rows != 0) {
      const std::uint64_t mcu_columns = static_cast<std::uint64_t>(
          (image->width + (8 * prepared.h_samp) - 1) / (8 * prepared.h_samp));
      restart_interval_value =
          mcu_columns * static_cast<std::uint64_t>(restart_marker_rows);
    }
    if (restart_interval_value > std::numeric_limits<std::uint16_t>::max()) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint16_t restart_interval =
        static_cast<std::uint16_t>(restart_interval_value);
    JpegHuffmanTable luma_dc_table;
    JpegHuffmanTable luma_ac_table;
    JpegHuffmanTable chroma_dc_table;
    JpegHuffmanTable chroma_ac_table;
    if (optimize == 1) {
      std::uint64_t luma_dc_freq[256] = {};
      std::uint64_t luma_ac_freq[256] = {};
      std::uint64_t chroma_dc_freq[256] = {};
      std::uint64_t chroma_ac_freq[256] = {};
      const std::size_t luma_restart_interval =
          static_cast<std::size_t>(restart_interval) *
          static_cast<std::size_t>(prepared.y_blocks_per_mcu);
      jpeg_collect_huffman_frequencies(prepared.y_blocks, luma_dc_freq,
                                       luma_ac_freq, luma_restart_interval);
      jpeg_collect_huffman_frequencies(prepared.cb_blocks, chroma_dc_freq,
                                       chroma_ac_freq, restart_interval);
      jpeg_collect_huffman_frequencies(prepared.cr_blocks, chroma_dc_freq,
                                       chroma_ac_freq, restart_interval);
      status = jpeg_build_optimized_huffman_table(luma_dc_freq, &luma_dc_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_build_optimized_huffman_table(luma_ac_freq, &luma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status =
          jpeg_build_optimized_huffman_table(chroma_dc_freq, &chroma_dc_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status =
          jpeg_build_optimized_huffman_table(chroma_ac_freq, &chroma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    } else {
      status = jpeg_build_standard_luminance_huffman_tables(&luma_dc_table,
                                                            &luma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_build_standard_chrominance_huffman_tables(&chroma_dc_table,
                                                              &chroma_ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> out;
    out.reserve(512u + prepared.y_blocks.size() + prepared.cb_blocks.size() +
                prepared.cr_blocks.size());
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
    std::uint8_t dqt_luma[65] = {};
    dqt_luma[0] = 0;
    for (int i = 0; i < 64; ++i) {
      dqt_luma[i + 1] =
          static_cast<std::uint8_t>(prepared.luma_qtable[JPEG_ZIGZAG[i]]);
    }
    status = append_jpeg_segment(out, 0xdbu, dqt_luma, sizeof(dqt_luma));
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (chroma_qtable_id == 1) {
      std::uint8_t dqt_chroma[65] = {};
      dqt_chroma[0] = 1;
      for (int i = 0; i < 64; ++i) {
        dqt_chroma[i + 1] =
            static_cast<std::uint8_t>(prepared.chroma_qtable[JPEG_ZIGZAG[i]]);
      }
      status = append_jpeg_segment(out, 0xdbu, dqt_chroma, sizeof(dqt_chroma));
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> sof;
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(3);
    sof.push_back(1);
    sof.push_back(
        static_cast<std::uint8_t>((prepared.h_samp << 4) | prepared.v_samp));
    sof.push_back(0);
    sof.push_back(2);
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(chroma_qtable_id));
    sof.push_back(3);
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(chroma_qtable_id));
    status = append_jpeg_segment(out, 0xc0u, sof.data(), sof.size());
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 0, luma_dc_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, luma_ac_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 1, chroma_dc_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 1, chroma_ac_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (restart_interval != 0u) {
      const std::uint8_t dri[] = {
          static_cast<std::uint8_t>((restart_interval >> 8) & 0xffu),
          static_cast<std::uint8_t>(restart_interval & 0xffu)};
      status = append_jpeg_segment(out, 0xddu, dri, sizeof(dri));
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::uint8_t sos[] = {3, 1, 0x00, 2, 0x11, 3, 0x11, 0, 63, 0};
    status = append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_rgb_interleaved_entropy(
        prepared.y_blocks, prepared.cb_blocks, prepared.cr_blocks,
        prepared.y_blocks_per_mcu, luma_dc_table, luma_ac_table,
        chroma_dc_table, chroma_ac_table, &out, restart_interval);
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
int jpeg_append_rgb_interleaved_sos_segment(std::vector<std::uint8_t> &out,
                                            int y_selector, int cb_selector,
                                            int cr_selector, int ss, int se,
                                            int ah, int al) {
  std::uint8_t sos[] = {3,
                        1,
                        static_cast<std::uint8_t>(y_selector),
                        2,
                        static_cast<std::uint8_t>(cb_selector),
                        3,
                        static_cast<std::uint8_t>(cr_selector),
                        static_cast<std::uint8_t>(ss),
                        static_cast<std::uint8_t>(se),
                        static_cast<std::uint8_t>((ah << 4) | al)};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int jpeg_append_rgb_single_sos_segment(std::vector<std::uint8_t> &out,
                                       int component_id, int table_selector,
                                       int ss, int se, int ah, int al) {
  std::uint8_t sos[] = {1,
                        static_cast<std::uint8_t>(component_id),
                        static_cast<std::uint8_t>(table_selector),
                        static_cast<std::uint8_t>(ss),
                        static_cast<std::uint8_t>(se),
                        static_cast<std::uint8_t>((ah << 4) | al)};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int jpeg_encode_progressive_rgb_dc_first_scan(
    const std::vector<int> &y_blocks, const std::vector<int> &cb_blocks,
    const std::vector<int> &cr_blocks, int y_blocks_per_mcu, int al,
    const JpegHuffmanTable &luma_dc_table,
    const JpegHuffmanTable &chroma_dc_table, std::vector<std::uint8_t> *out,
    std::uint16_t restart_interval = 0) {
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
        jpeg_write_restart_marker(&writer, &restart_index);
        previous_y_dc = 0;
        previous_cb_dc = 0;
        previous_cr_dc = 0;
      }
      for (int y_block = 0; y_block < y_blocks_per_mcu; ++y_block) {
        const int *coeffs = y_blocks.data() + y_block_index * 64u;
        ++y_block_index;
        const int dc = jpeg_successive_dc_value(coeffs[0], al);
        const int diff = dc - previous_y_dc;
        previous_y_dc = dc;
        const int category = jpeg_value_category(diff);
        int status =
            jpeg_write_huffman_symbol(&writer, luma_dc_table, category);
        if (status != PILLOW_C_OK) {
          return status;
        }
        jpeg_bit_writer_write(&writer, jpeg_value_bits(diff, category),
                              category);
      }
      const int *cb_coeffs = cb_blocks.data() + mcu * 64u;
      int cb_dc = jpeg_successive_dc_value(cb_coeffs[0], al);
      int diff = cb_dc - previous_cb_dc;
      previous_cb_dc = cb_dc;
      int category = jpeg_value_category(diff);
      int status =
          jpeg_write_huffman_symbol(&writer, chroma_dc_table, category);
      if (status != PILLOW_C_OK) {
        return status;
      }
      jpeg_bit_writer_write(&writer, jpeg_value_bits(diff, category), category);
      const int *cr_coeffs = cr_blocks.data() + mcu * 64u;
      const int cr_dc = jpeg_successive_dc_value(cr_coeffs[0], al);
      diff = cr_dc - previous_cr_dc;
      previous_cr_dc = cr_dc;
      category = jpeg_value_category(diff);
      status = jpeg_write_huffman_symbol(&writer, chroma_dc_table, category);
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
int jpeg_encode_progressive_rgb_dc_refine_scan(
    const std::vector<int> &y_blocks, const std::vector<int> &cb_blocks,
    const std::vector<int> &cr_blocks, int y_blocks_per_mcu, int al,
    std::vector<std::uint8_t> *out, std::uint16_t restart_interval = 0) {
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
    int restart_index = 0;
    std::size_t y_block_index = 0;
    for (std::size_t mcu = 0; mcu < mcu_count; ++mcu) {
      if (restart_interval != 0u && mcu != 0u && mcu % restart_interval == 0u) {
        jpeg_write_restart_marker(&writer, &restart_index);
      }
      for (int y_block = 0; y_block < y_blocks_per_mcu; ++y_block) {
        const int *coeffs = y_blocks.data() + y_block_index * 64u;
        ++y_block_index;
        jpeg_bit_writer_write(
            &writer,
            static_cast<std::uint16_t>(jpeg_successive_bit(coeffs[0], al)), 1);
      }
      const int *cb_coeffs = cb_blocks.data() + mcu * 64u;
      const int *cr_coeffs = cr_blocks.data() + mcu * 64u;
      jpeg_bit_writer_write(
          &writer,
          static_cast<std::uint16_t>(jpeg_successive_bit(cb_coeffs[0], al)), 1);
      jpeg_bit_writer_write(
          &writer,
          static_cast<std::uint16_t>(jpeg_successive_bit(cr_coeffs[0], al)), 1);
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int save_jpeg_rgb_progressive_huffman(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, int subsampling, const int *custom_luma_qtable,
    const int *custom_chroma_qtable, int chroma_qtable_id,
    int restart_marker_blocks, int restart_marker_rows) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if ((chroma_qtable_id != 0 && chroma_qtable_id != 1) ||
      restart_marker_blocks < 0 || restart_marker_rows < 0 ||
      (restart_marker_blocks != 0 && restart_marker_rows != 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::uint8_t jfif_unit = 0;
  std::uint16_t jfif_x_density = 1;
  std::uint16_t jfif_y_density = 1;
  int status = PILLOW_C_OK;
  if (has_dpi) {
    status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit, &jfif_x_density,
                                   &jfif_y_density);
    if (status != PILLOW_C_OK) {
      return status;
    }
  }
  try {
    JpegRgbPreparedBlocks prepared;
    status = jpeg_prepare_rgb_sampled_blocks(image, quality, subsampling,
                                             custom_luma_qtable,
                                             custom_chroma_qtable, &prepared);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const int y_block_columns = (image->width + 7) / 8;
    const int y_block_rows = (image->height + 7) / 8;
    const int mcu_columns =
        (image->width + (8 * prepared.h_samp) - 1) / (8 * prepared.h_samp);
    const std::size_t y_scan_block_count =
        static_cast<std::size_t>(y_block_columns) *
        static_cast<std::size_t>(y_block_rows);
    if (y_scan_block_count > prepared.y_blocks.size() / 64u) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<int> y_scan_blocks;
    y_scan_blocks.reserve(y_scan_block_count * 64u);
    for (int block_y = 0; block_y < y_block_rows; ++block_y) {
      const int mcu_y = block_y / prepared.v_samp;
      const int block_y_in_mcu = block_y % prepared.v_samp;
      for (int block_x = 0; block_x < y_block_columns; ++block_x) {
        const int mcu_x = block_x / prepared.h_samp;
        const int block_x_in_mcu = block_x % prepared.h_samp;
        const std::size_t mcu_index =
            static_cast<std::size_t>(mcu_y) *
                static_cast<std::size_t>(mcu_columns) +
            static_cast<std::size_t>(mcu_x);
        const std::size_t source_block =
            mcu_index * static_cast<std::size_t>(prepared.y_blocks_per_mcu) +
            static_cast<std::size_t>(block_y_in_mcu * prepared.h_samp +
                                     block_x_in_mcu);
        if (source_block >= prepared.y_blocks.size() / 64u) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
        const int *coefficients = prepared.y_blocks.data() + source_block * 64u;
        y_scan_blocks.insert(y_scan_blocks.end(), coefficients,
                             coefficients + 64);
      }
    }
    std::uint64_t interleaved_restart_interval_value = 0u;
    std::uint64_t luma_restart_interval_value = 0u;
    if (restart_marker_blocks != 0) {
      interleaved_restart_interval_value =
          static_cast<std::uint64_t>(restart_marker_blocks);
      luma_restart_interval_value = interleaved_restart_interval_value;
    } else if (restart_marker_rows != 0) {
      const std::uint64_t sampled_mcu_columns = static_cast<std::uint64_t>(
          (image->width + (8 * prepared.h_samp) - 1) / (8 * prepared.h_samp));
      interleaved_restart_interval_value =
          sampled_mcu_columns * static_cast<std::uint64_t>(restart_marker_rows);
      const std::uint64_t luma_block_columns =
          (static_cast<std::uint64_t>(image->width) + 7u) / 8u;
      luma_restart_interval_value =
          luma_block_columns * static_cast<std::uint64_t>(restart_marker_rows);
    }
    if (interleaved_restart_interval_value >
            std::numeric_limits<std::uint16_t>::max() ||
        luma_restart_interval_value >
            std::numeric_limits<std::uint16_t>::max()) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint16_t interleaved_restart_interval =
        static_cast<std::uint16_t>(interleaved_restart_interval_value);
    const std::uint16_t luma_restart_interval =
        static_cast<std::uint16_t>(luma_restart_interval_value);
    const std::size_t luma_dc_restart_interval =
        static_cast<std::size_t>(interleaved_restart_interval) *
        static_cast<std::size_t>(prepared.y_blocks_per_mcu);
    std::uint64_t luma_dc_first_freq[256] = {};
    std::uint64_t chroma_dc_first_freq[256] = {};
    std::uint64_t luma_ac_first_low_freq[256] = {};
    std::uint64_t luma_ac_first_high_freq[256] = {};
    std::uint64_t cb_ac_first_freq[256] = {};
    std::uint64_t cr_ac_first_freq[256] = {};
    std::uint64_t luma_ac_refine_mid_freq[256] = {};
    std::uint64_t luma_ac_refine_final_freq[256] = {};
    std::uint64_t cb_ac_refine_final_freq[256] = {};
    std::uint64_t cr_ac_refine_final_freq[256] = {};
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.y_blocks, 1, luma_dc_first_freq, luma_dc_restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(prepared.cb_blocks, 1,
                                                  chroma_dc_first_freq,
                                                  interleaved_restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(prepared.cr_blocks, 1,
                                                  chroma_dc_first_freq,
                                                  interleaved_restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        y_scan_blocks, 1, 5, 2, luma_ac_first_low_freq, luma_restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(y_scan_blocks, 6, 63, 2,
                                                  luma_ac_first_high_freq,
                                                  luma_restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(prepared.cb_blocks, 1, 63, 1,
                                                  cb_ac_first_freq,
                                                  interleaved_restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(prepared.cr_blocks, 1, 63, 1,
                                                  cr_ac_first_freq,
                                                  interleaved_restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(y_scan_blocks, 1, 63, 2, 1,
                                                   luma_ac_refine_mid_freq,
                                                   luma_restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(y_scan_blocks, 1, 63, 1, 0,
                                                   luma_ac_refine_final_freq,
                                                   luma_restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(
        prepared.cb_blocks, 1, 63, 1, 0, cb_ac_refine_final_freq,
        interleaved_restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(
        prepared.cr_blocks, 1, 63, 1, 0, cr_ac_refine_final_freq,
        interleaved_restart_interval);
    JpegHuffmanTable luma_dc_first_table;
    JpegHuffmanTable chroma_dc_first_table;
    JpegHuffmanTable luma_ac_first_low_table;
    JpegHuffmanTable luma_ac_first_high_table;
    JpegHuffmanTable cb_ac_first_table;
    JpegHuffmanTable cr_ac_first_table;
    JpegHuffmanTable luma_ac_refine_mid_table;
    JpegHuffmanTable luma_ac_refine_final_table;
    JpegHuffmanTable cb_ac_refine_final_table;
    JpegHuffmanTable cr_ac_refine_final_table;
    status = jpeg_build_optimized_huffman_table(luma_dc_first_freq,
                                                &luma_dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(chroma_dc_first_freq,
                                                &chroma_dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(luma_ac_first_low_freq,
                                                &luma_ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(luma_ac_first_high_freq,
                                                &luma_ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(cb_ac_first_freq,
                                                &cb_ac_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(cr_ac_first_freq,
                                                &cr_ac_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(luma_ac_refine_mid_freq,
                                                &luma_ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(luma_ac_refine_final_freq,
                                                &luma_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(cb_ac_refine_final_freq,
                                                &cb_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(cr_ac_refine_final_freq,
                                                &cr_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> out;
    out.reserve(768u + prepared.y_blocks.size() + prepared.cb_blocks.size() +
                prepared.cr_blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    std::uint16_t active_restart_interval = 0u;
    auto append_restart_interval = [&](std::uint16_t interval) -> int {
      if (interval == 0u || interval == active_restart_interval) {
        return PILLOW_C_OK;
      }
      const int append_status = jpeg_append_dri_segment(out, interval);
      if (append_status == PILLOW_C_OK) {
        active_restart_interval = interval;
      }
      return append_status;
    };
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
    std::uint8_t dqt_luma[65] = {};
    dqt_luma[0] = 0;
    for (int i = 0; i < 64; ++i) {
      dqt_luma[i + 1] =
          static_cast<std::uint8_t>(prepared.luma_qtable[JPEG_ZIGZAG[i]]);
    }
    status = append_jpeg_segment(out, 0xdbu, dqt_luma, sizeof(dqt_luma));
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (chroma_qtable_id == 1) {
      std::uint8_t dqt_chroma[65] = {};
      dqt_chroma[0] = 1;
      for (int i = 0; i < 64; ++i) {
        dqt_chroma[i + 1] =
            static_cast<std::uint8_t>(prepared.chroma_qtable[JPEG_ZIGZAG[i]]);
      }
      status = append_jpeg_segment(out, 0xdbu, dqt_chroma, sizeof(dqt_chroma));
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> sof;
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(3);
    sof.push_back(1);
    sof.push_back(
        static_cast<std::uint8_t>((prepared.h_samp << 4) | prepared.v_samp));
    sof.push_back(0);
    sof.push_back(2);
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(chroma_qtable_id));
    sof.push_back(3);
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(chroma_qtable_id));
    status = append_jpeg_segment(out, 0xc2u, sof.data(), sof.size());
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 0, luma_dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 1, chroma_dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_interleaved_sos_segment(out, 0x00, 0x10, 0x10, 0,
                                                     0, 0, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_rgb_dc_first_scan(
        prepared.y_blocks, prepared.cb_blocks, prepared.cr_blocks,
        prepared.y_blocks_per_mcu, 1, luma_dc_first_table,
        chroma_dc_first_table, &out, interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, luma_ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 1, 0x00, 1, 5, 0, 2);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_first_scan(y_scan_blocks, 1, 5, 2,
                                                   luma_ac_first_low_table,
                                                   &out, luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 1, cr_ac_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 3, 0x01, 1, 63, 0, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_first_scan(
        prepared.cr_blocks, 1, 63, 1, cr_ac_first_table, &out,
        interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 1, cb_ac_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 2, 0x01, 1, 63, 0, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_first_scan(
        prepared.cb_blocks, 1, 63, 1, cb_ac_first_table, &out,
        interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, luma_ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 1, 0x00, 6, 63, 0, 2);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_first_scan(y_scan_blocks, 6, 63, 2,
                                                   luma_ac_first_high_table,
                                                   &out, luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, luma_ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 1, 0x00, 1, 63, 2, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_refine_scan(
        y_scan_blocks, 1, 63, 2, 1, luma_ac_refine_mid_table, &out,
        luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_interleaved_sos_segment(out, 0x00, 0x00, 0x00, 0,
                                                     0, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_rgb_dc_refine_scan(
        prepared.y_blocks, prepared.cb_blocks, prepared.cr_blocks,
        prepared.y_blocks_per_mcu, 0, &out, interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 1, cr_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 3, 0x01, 1, 63, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_refine_scan(
        prepared.cr_blocks, 1, 63, 1, 0, cr_ac_refine_final_table, &out,
        interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 1, cb_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 2, 0x01, 1, 63, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_refine_scan(
        prepared.cb_blocks, 1, 63, 1, 0, cb_ac_refine_final_table, &out,
        interleaved_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 1, 0, luma_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = append_restart_interval(luma_restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_rgb_single_sos_segment(out, 1, 0x00, 1, 63, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_ac_refine_scan(
        y_scan_blocks, 1, 63, 1, 0, luma_ac_refine_final_table, &out,
        luma_restart_interval);
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
int save_jpeg_rgb_keep_rgb_baseline(const PillowCImage *image, const char *path,
                                    int quality, bool has_dpi, double dpi_x,
                                    double dpi_y, bool optimize,
                                    const int *custom_r_qtable,
                                    const int *custom_gb_qtable,
                                    int gb_qtable_id,
                                    std::uint16_t restart_interval) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (gb_qtable_id != 0 && gb_qtable_id != 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegRgbPreparedBlocks prepared;
    int status = jpeg_prepare_keep_rgb_blocks(image, quality, custom_r_qtable,
                                              custom_gb_qtable, &prepared);
    if (status != PILLOW_C_OK) {
      return status;
    }
    JpegHuffmanTable dc_table;
    JpegHuffmanTable ac_table;
    if (optimize) {
      std::uint64_t dc_freq[256] = {};
      std::uint64_t ac_freq[256] = {};
      jpeg_collect_huffman_frequencies(prepared.y_blocks, dc_freq, ac_freq,
                                       restart_interval);
      jpeg_collect_huffman_frequencies(prepared.cb_blocks, dc_freq, ac_freq,
                                       restart_interval);
      jpeg_collect_huffman_frequencies(prepared.cr_blocks, dc_freq, ac_freq,
                                       restart_interval);
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
    out.reserve(512u + prepared.y_blocks.size() + prepared.cb_blocks.size() +
                prepared.cr_blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    status = jpeg_append_optional_jfif_app0(out, has_dpi, dpi_x, dpi_y);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_adobe_rgb_app14(out);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_dqt(out, prepared.luma_qtable);
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (gb_qtable_id == 1) {
      status = jpeg_append_dqt_table(out, 1, prepared.chroma_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_keep_rgb_sof(out, image, 0xc0u, 0, gb_qtable_id,
                                      gb_qtable_id);
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
    if (restart_interval != 0u) {
      status = jpeg_append_dri_segment(out, restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_keep_rgb_interleaved_sos_segment(out, 0x00, 0x00, 0x00,
                                                          0, 63, 0, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_rgb_interleaved_entropy(
        prepared.y_blocks, prepared.cb_blocks, prepared.cr_blocks, 1, dc_table,
        ac_table, dc_table, ac_table, &out, restart_interval);
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
int save_jpeg_rgb_keep_rgb_progressive(const PillowCImage *image,
                                       const char *path, int quality,
                                       bool has_dpi, double dpi_x, double dpi_y,
                                       const int *custom_r_qtable,
                                       const int *custom_gb_qtable,
                                       int gb_qtable_id,
                                       std::uint16_t restart_interval) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (gb_qtable_id != 0 && gb_qtable_id != 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegRgbPreparedBlocks prepared;
    int status = jpeg_prepare_keep_rgb_blocks(image, quality, custom_r_qtable,
                                              custom_gb_qtable, &prepared);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::uint64_t dc_first_freq[256] = {};
    std::uint64_t r_ac_first_low_freq[256] = {};
    std::uint64_t g_ac_first_low_freq[256] = {};
    std::uint64_t b_ac_first_low_freq[256] = {};
    std::uint64_t r_ac_first_high_freq[256] = {};
    std::uint64_t g_ac_first_high_freq[256] = {};
    std::uint64_t b_ac_first_high_freq[256] = {};
    std::uint64_t r_ac_refine_mid_freq[256] = {};
    std::uint64_t g_ac_refine_mid_freq[256] = {};
    std::uint64_t b_ac_refine_mid_freq[256] = {};
    std::uint64_t r_ac_refine_final_freq[256] = {};
    std::uint64_t g_ac_refine_final_freq[256] = {};
    std::uint64_t b_ac_refine_final_freq[256] = {};
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.y_blocks, 1, dc_first_freq, restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.cb_blocks, 1, dc_first_freq, restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.cr_blocks, 1, dc_first_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.y_blocks, 1, 5, 2, r_ac_first_low_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.cb_blocks, 1, 5, 2, g_ac_first_low_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.cr_blocks, 1, 5, 2, b_ac_first_low_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.y_blocks, 6, 63, 2, r_ac_first_high_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.cb_blocks, 6, 63, 2, g_ac_first_high_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.cr_blocks, 6, 63, 2, b_ac_first_high_freq, restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(
        prepared.y_blocks, 1, 63, 2, 1, r_ac_refine_mid_freq, restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.cb_blocks, 1, 63, 2,
                                                   1, g_ac_refine_mid_freq,
                                                   restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.cr_blocks, 1, 63, 2,
                                                   1, b_ac_refine_mid_freq,
                                                   restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.y_blocks, 1, 63, 1,
                                                   0, r_ac_refine_final_freq,
                                                   restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.cb_blocks, 1, 63, 1,
                                                   0, g_ac_refine_final_freq,
                                                   restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.cr_blocks, 1, 63, 1,
                                                   0, b_ac_refine_final_freq,
                                                   restart_interval);
    JpegHuffmanTable dc_first_table;
    JpegHuffmanTable r_ac_first_low_table;
    JpegHuffmanTable g_ac_first_low_table;
    JpegHuffmanTable b_ac_first_low_table;
    JpegHuffmanTable r_ac_first_high_table;
    JpegHuffmanTable g_ac_first_high_table;
    JpegHuffmanTable b_ac_first_high_table;
    JpegHuffmanTable r_ac_refine_mid_table;
    JpegHuffmanTable g_ac_refine_mid_table;
    JpegHuffmanTable b_ac_refine_mid_table;
    JpegHuffmanTable r_ac_refine_final_table;
    JpegHuffmanTable g_ac_refine_final_table;
    JpegHuffmanTable b_ac_refine_final_table;
    status = jpeg_build_optimized_huffman_table(dc_first_freq, &dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(r_ac_first_low_freq,
                                                &r_ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(g_ac_first_low_freq,
                                                &g_ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(b_ac_first_low_freq,
                                                &b_ac_first_low_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(r_ac_first_high_freq,
                                                &r_ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(g_ac_first_high_freq,
                                                &g_ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(b_ac_first_high_freq,
                                                &b_ac_first_high_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(r_ac_refine_mid_freq,
                                                &r_ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(g_ac_refine_mid_freq,
                                                &g_ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(b_ac_refine_mid_freq,
                                                &b_ac_refine_mid_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(r_ac_refine_final_freq,
                                                &r_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(g_ac_refine_final_freq,
                                                &g_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_build_optimized_huffman_table(b_ac_refine_final_freq,
                                                &b_ac_refine_final_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::vector<std::uint8_t> out;
    out.reserve(1024u + prepared.y_blocks.size() + prepared.cb_blocks.size() +
                prepared.cr_blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    status = jpeg_append_optional_jfif_app0(out, has_dpi, dpi_x, dpi_y);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_adobe_rgb_app14(out);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_dqt(out, prepared.luma_qtable);
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (gb_qtable_id == 1) {
      status = jpeg_append_dqt_table(out, 1, prepared.chroma_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_keep_rgb_sof(out, image, 0xc2u, 0, gb_qtable_id,
                                      gb_qtable_id);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_dht_segment(out, 0, 0, dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (restart_interval != 0u) {
      status = jpeg_append_dri_segment(out, restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_keep_rgb_interleaved_sos_segment(out, 0x00, 0x00, 0x00,
                                                          0, 0, 0, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_rgb_dc_first_scan(
        prepared.y_blocks, prepared.cb_blocks, prepared.cr_blocks, 1, 1,
        dc_first_table, dc_first_table, &out, restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const struct AcFirstScan {
      const std::vector<int> *blocks;
      const JpegHuffmanTable *table;
      int component;
      int ss;
      int se;
      int al;
    } ac_first_scans[] = {
        {&prepared.y_blocks, &r_ac_first_low_table, 'R', 1, 5, 2},
        {&prepared.cb_blocks, &g_ac_first_low_table, 'G', 1, 5, 2},
        {&prepared.cr_blocks, &b_ac_first_low_table, 'B', 1, 5, 2},
        {&prepared.y_blocks, &r_ac_first_high_table, 'R', 6, 63, 2},
        {&prepared.cb_blocks, &g_ac_first_high_table, 'G', 6, 63, 2},
        {&prepared.cr_blocks, &b_ac_first_high_table, 'B', 6, 63, 2},
    };
    for (const AcFirstScan &scan : ac_first_scans) {
      status = jpeg_append_dht_segment(out, 1, 0, *scan.table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_append_keep_rgb_single_sos_segment(
          out, scan.component, 0x00, scan.ss, scan.se, 0, scan.al);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_progressive_ac_first_scan(
          *scan.blocks, scan.ss, scan.se, scan.al, *scan.table, &out,
          restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    const struct AcRefineScan {
      const std::vector<int> *blocks;
      const JpegHuffmanTable *table;
      int component;
      int ah;
      int al;
    } ac_refine_mid_scans[] = {
        {&prepared.y_blocks, &r_ac_refine_mid_table, 'R', 2, 1},
        {&prepared.cb_blocks, &g_ac_refine_mid_table, 'G', 2, 1},
        {&prepared.cr_blocks, &b_ac_refine_mid_table, 'B', 2, 1},
    };
    for (const AcRefineScan &scan : ac_refine_mid_scans) {
      status = jpeg_append_dht_segment(out, 1, 0, *scan.table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_append_keep_rgb_single_sos_segment(
          out, scan.component, 0x00, 1, 63, scan.ah, scan.al);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_progressive_ac_refine_scan(
          *scan.blocks, 1, 63, scan.ah, scan.al, *scan.table, &out,
          restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_keep_rgb_interleaved_sos_segment(out, 0x00, 0x00, 0x00,
                                                          0, 0, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_rgb_dc_refine_scan(
        prepared.y_blocks, prepared.cb_blocks, prepared.cr_blocks, 1, 0, &out,
        restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const AcRefineScan ac_refine_final_scans[] = {
        {&prepared.y_blocks, &r_ac_refine_final_table, 'R', 1, 0},
        {&prepared.cb_blocks, &g_ac_refine_final_table, 'G', 1, 0},
        {&prepared.cr_blocks, &b_ac_refine_final_table, 'B', 1, 0},
    };
    for (const AcRefineScan &scan : ac_refine_final_scans) {
      status = jpeg_append_dht_segment(out, 1, 0, *scan.table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_append_keep_rgb_single_sos_segment(
          out, scan.component, 0x00, 1, 63, scan.ah, scan.al);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_progressive_ac_refine_scan(
          *scan.blocks, 1, 63, scan.ah, scan.al, *scan.table, &out,
          restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    out.push_back(0xffu);
    out.push_back(0xd9u);
    return write_binary_file(path, out) ? PILLOW_C_OK
                                        : PILLOW_C_INVALID_ARGUMENT;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int save_jpeg_rgb_keep_rgb_options(const PillowCImage *image, const char *path,
                                   int quality, bool has_dpi, double dpi_x,
                                   double dpi_y, int subsampling,
                                   int progressive, int optimize, int keep_rgb,
                                   std::uint16_t restart_interval) {
  if (keep_rgb < -1 || keep_rgb > 1 || progressive < -1 || progressive > 1 ||
      optimize < -1 || optimize > 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (keep_rgb != 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (!(subsampling == -1 || subsampling == 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (!image || image->mode != PILLOW_C_MODE_RGB || image->channels != 3) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
  if (refresh_status != PILLOW_C_OK) {
    return refresh_status;
  }
  if (progressive == 1) {
    return save_jpeg_rgb_keep_rgb_progressive(image, path, quality, has_dpi,
                                              dpi_x, dpi_y, nullptr, nullptr, 0,
                                              restart_interval);
  }
  return save_jpeg_rgb_keep_rgb_baseline(image, path, quality, has_dpi, dpi_x,
                                         dpi_y, optimize == 1, nullptr, nullptr,
                                         0,
                                         restart_interval);
}
int save_jpeg_rgb_qtables_keep_rgb_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize, int keep_rgb,
    std::uint16_t restart_interval) {
  static_cast<void>(dpi_x);
  static_cast<void>(dpi_y);
  if (!image || !path || !qtables) {
    return PILLOW_C_NULL_POINTER;
  }
  const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
  if (refresh_status != PILLOW_C_OK) {
    return refresh_status;
  }
  if (qtable_count < 1u || qtable_count > 2u || keep_rgb < -1 || keep_rgb > 1 ||
      progressive < -1 || progressive > 1 || optimize < -1 || optimize > 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (keep_rgb != 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
    if (restart_interval != 0u) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    if (!(subsampling == -1 || subsampling == 0 || subsampling == 1 ||
          subsampling == 2)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_jpeg_rgb_qtables_optimized_huffman(
        image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
        subsampling, progressive, optimize, true);
  }
  if (has_dpi) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (!(subsampling == -1 || subsampling == 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const int *gb_qtable = qtable_count > 1u ? qtables + 64 : qtables;
  const int gb_qtable_id = qtable_count > 1u ? 1 : 0;
  if (progressive == 1) {
    return save_jpeg_rgb_keep_rgb_progressive(image, path, quality, false, 0.0,
                                              0.0, qtables, gb_qtable,
                                              gb_qtable_id, restart_interval);
  }
  return save_jpeg_rgb_keep_rgb_baseline(image, path, quality, false, 0.0, 0.0,
                                         optimize == 1, qtables, gb_qtable,
                                         gb_qtable_id, restart_interval);
}
int jpeg_keep_rgb_restart_interval_from_options(
    const PillowCImage *image, int restart_marker_blocks,
    int restart_marker_rows, std::uint16_t *out_restart_interval) {
  if (!image || !out_restart_interval) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      image->width <= 0 || restart_marker_blocks < 0 ||
      restart_marker_rows < 0 ||
      (restart_marker_blocks != 0 && restart_marker_rows != 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::uint64_t restart_interval =
      static_cast<std::uint64_t>(restart_marker_blocks);
  if (restart_marker_rows != 0) {
    const std::uint64_t mcu_columns =
        (static_cast<std::uint64_t>(image->width) + 7u) / 8u;
    restart_interval =
        mcu_columns * static_cast<std::uint64_t>(restart_marker_rows);
  }
  if (restart_interval > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  *out_restart_interval = static_cast<std::uint16_t>(restart_interval);
  return PILLOW_C_OK;
}
} // namespace pillow_c_jpeg
