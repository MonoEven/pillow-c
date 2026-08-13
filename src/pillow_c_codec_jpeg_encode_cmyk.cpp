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
struct JpegCmykPreparedBlocks {
  int h_samp = 1;
  int v_samp = 1;
  int c_blocks_per_mcu = 1;
  int qtable[64] = {};
  int myk_qtable[64] = {};
  bool has_myk_qtable = false;
  std::vector<int> c_blocks;
  std::vector<int> m_blocks;
  std::vector<int> y_blocks;
  std::vector<int> k_blocks;
};
int jpeg_cmyk_sampling_from_subsampling(int subsampling, int *out_h,
                                        int *out_v) {
  if (!out_h || !out_v) {
    return PILLOW_C_NULL_POINTER;
  }
  switch (subsampling) {
  case -1:
  case 0:
    *out_h = 1;
    *out_v = 1;
    return PILLOW_C_OK;
  case 1:
    *out_h = 2;
    *out_v = 1;
    return PILLOW_C_OK;
  case 2:
    *out_h = 2;
    *out_v = 2;
    return PILLOW_C_OK;
  default:
    return PILLOW_C_INVALID_ARGUMENT;
  }
}
int jpeg_prepare_cmyk_blocks(const PillowCImage *image, int quality,
                             const int *qtables, std::size_t qtable_count,
                             JpegCmykPreparedBlocks *prepared,
                             int subsampling) {
  if (!image || !prepared) {
    return PILLOW_C_NULL_POINTER;
  }
  if (qtable_count > 0u && !qtables) {
    return PILLOW_C_NULL_POINTER;
  }
  if (qtable_count > 2u) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (image->mode != PILLOW_C_MODE_CMYK || image->channels != 4 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  int h_samp = 1;
  int v_samp = 1;
  int status =
      jpeg_cmyk_sampling_from_subsampling(subsampling, &h_samp, &v_samp);
  if (status != PILLOW_C_OK) {
    return status;
  }
  try {
    *prepared = JpegCmykPreparedBlocks{};
    prepared->h_samp = h_samp;
    prepared->v_samp = v_samp;
    prepared->c_blocks_per_mcu = h_samp * v_samp;
    if (qtable_count == 0u) {
      jpeg_scaled_luminance_qtable(quality, prepared->qtable);
      std::copy(prepared->qtable, prepared->qtable + 64, prepared->myk_qtable);
    } else {
      status = jpeg_scaled_custom_qtable(qtables, quality, prepared->qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
      if (qtable_count > 1u) {
        status = jpeg_scaled_custom_qtable(qtables + 64, quality,
                                           prepared->myk_qtable);
        if (status != PILLOW_C_OK) {
          return status;
        }
        prepared->has_myk_qtable = true;
      } else {
        std::copy(prepared->qtable, prepared->qtable + 64,
                  prepared->myk_qtable);
      }
    }
    const std::size_t pixel_count = static_cast<std::size_t>(image->width) *
                                    static_cast<std::size_t>(image->height);
    const int myk_width = (image->width + h_samp - 1) / h_samp;
    const int myk_height = (image->height + v_samp - 1) / v_samp;
    const int mcu_cols = (image->width + (8 * h_samp) - 1) / (8 * h_samp);
    const int mcu_rows = (image->height + (8 * v_samp) - 1) / (8 * v_samp);
    const std::size_t mcu_count =
        static_cast<std::size_t>(mcu_cols) * static_cast<std::size_t>(mcu_rows);
    if (mcu_count > std::numeric_limits<std::size_t>::max() / 64u ||
        mcu_count >
            std::numeric_limits<std::size_t>::max() /
                (64u * static_cast<std::size_t>(prepared->c_blocks_per_mcu))) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> c_plane(pixel_count);
    std::vector<std::uint8_t> m_full(pixel_count);
    std::vector<std::uint8_t> y_full(pixel_count);
    std::vector<std::uint8_t> k_full(pixel_count);
    for (int y = 0; y < image->height; ++y) {
      const std::uint8_t *row =
          image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
      const std::size_t row_offset =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(image->width);
      for (int x = 0; x < image->width; ++x) {
        const std::uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        const std::size_t index = row_offset + static_cast<std::size_t>(x);
        c_plane[index] = static_cast<std::uint8_t>(255u - pixel[0]);
        m_full[index] = static_cast<std::uint8_t>(255u - pixel[1]);
        y_full[index] = static_cast<std::uint8_t>(255u - pixel[2]);
        k_full[index] = static_cast<std::uint8_t>(255u - pixel[3]);
      }
    }
    const std::size_t myk_count = static_cast<std::size_t>(myk_width) *
                                  static_cast<std::size_t>(myk_height);
    std::vector<std::uint8_t> m_plane(myk_count);
    std::vector<std::uint8_t> y_plane(myk_count);
    std::vector<std::uint8_t> k_plane(myk_count);
    for (int cy = 0; cy < myk_height; ++cy) {
      for (int cx = 0; cx < myk_width; ++cx) {
        int m_sum = 0;
        int y_sum = 0;
        int k_sum = 0;
        for (int dy = 0; dy < v_samp; ++dy) {
          const int src_y = std::min((cy * v_samp) + dy, image->height - 1);
          const std::size_t src_row = static_cast<std::size_t>(src_y) *
                                      static_cast<std::size_t>(image->width);
          for (int dx = 0; dx < h_samp; ++dx) {
            const int src_x = std::min((cx * h_samp) + dx, image->width - 1);
            const std::size_t src_index =
                src_row + static_cast<std::size_t>(src_x);
            m_sum += m_full[src_index];
            y_sum += y_full[src_index];
            k_sum += k_full[src_index];
          }
        }
        const int divisor = h_samp * v_samp;
        const int downsample_bias =
            h_samp == 2 && v_samp == 2 ? 1 + (cx & 1) : 0;
        const std::size_t dst_index =
            static_cast<std::size_t>(cy) * static_cast<std::size_t>(myk_width) +
            static_cast<std::size_t>(cx);
        m_plane[dst_index] =
            static_cast<std::uint8_t>((m_sum + downsample_bias) / divisor);
        y_plane[dst_index] =
            static_cast<std::uint8_t>((y_sum + downsample_bias) / divisor);
        k_plane[dst_index] =
            static_cast<std::uint8_t>((k_sum + downsample_bias) / divisor);
      }
    }
    prepared->c_blocks.reserve(
        mcu_count * static_cast<std::size_t>(prepared->c_blocks_per_mcu) * 64u);
    prepared->m_blocks.reserve(mcu_count * 64u);
    prepared->y_blocks.reserve(mcu_count * 64u);
    prepared->k_blocks.reserve(mcu_count * 64u);
    for (int mcu_y = 0; mcu_y < mcu_rows; ++mcu_y) {
      for (int mcu_x = 0; mcu_x < mcu_cols; ++mcu_x) {
        for (int c_y = 0; c_y < v_samp; ++c_y) {
          for (int c_x = 0; c_x < h_samp; ++c_x) {
            int zz[64] = {};
            jpeg_fdct_quantize_plane_block(c_plane, image->width, image->height,
                                           ((mcu_x * h_samp) + c_x) * 8,
                                           ((mcu_y * v_samp) + c_y) * 8,
                                           prepared->qtable, zz);
            prepared->c_blocks.insert(prepared->c_blocks.end(), zz, zz + 64);
          }
        }
        int zz[64] = {};
        jpeg_fdct_quantize_plane_block(m_plane, myk_width, myk_height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->myk_qtable, zz);
        prepared->m_blocks.insert(prepared->m_blocks.end(), zz, zz + 64);
        jpeg_fdct_quantize_plane_block(y_plane, myk_width, myk_height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->myk_qtable, zz);
        prepared->y_blocks.insert(prepared->y_blocks.end(), zz, zz + 64);
        jpeg_fdct_quantize_plane_block(k_plane, myk_width, myk_height,
                                       mcu_x * 8, mcu_y * 8,
                                       prepared->myk_qtable, zz);
        prepared->k_blocks.insert(prepared->k_blocks.end(), zz, zz + 64);
      }
    }
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int jpeg_append_cmyk_sof(std::vector<std::uint8_t> &out,
                         const PillowCImage *image, int myk_qtable_id,
                         std::uint8_t marker, int h_samp, int v_samp) {
  if (!image) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((myk_qtable_id != 0 && myk_qtable_id != 1) || h_samp < 1 || h_samp > 2 ||
      v_samp < 1 || v_samp > 2) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::vector<std::uint8_t> sof;
  try {
    sof.push_back(8);
    append_be16(sof, static_cast<std::uint16_t>(image->height));
    append_be16(sof, static_cast<std::uint16_t>(image->width));
    sof.push_back(4);
    sof.push_back('C');
    sof.push_back(static_cast<std::uint8_t>((h_samp << 4) | v_samp));
    sof.push_back(0);
    sof.push_back('M');
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(myk_qtable_id));
    sof.push_back('Y');
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(myk_qtable_id));
    sof.push_back('K');
    sof.push_back(0x11);
    sof.push_back(static_cast<std::uint8_t>(myk_qtable_id));
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
  return append_jpeg_segment(out, marker, sof.data(), sof.size());
}
int jpeg_append_cmyk_sos_segment(std::vector<std::uint8_t> &out) {
  const std::uint8_t sos[] = {4,    static_cast<std::uint8_t>('C'),
                              0x00, static_cast<std::uint8_t>('M'),
                              0x00, static_cast<std::uint8_t>('Y'),
                              0x00, static_cast<std::uint8_t>('K'),
                              0x00, 0,
                              63,   0};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int jpeg_append_cmyk_interleaved_sos_segment(std::vector<std::uint8_t> &out,
                                             int c_selector, int m_selector,
                                             int y_selector, int k_selector,
                                             int ss, int se, int ah, int al) {
  std::uint8_t sos[] = {4,
                        static_cast<std::uint8_t>('C'),
                        static_cast<std::uint8_t>(c_selector),
                        static_cast<std::uint8_t>('M'),
                        static_cast<std::uint8_t>(m_selector),
                        static_cast<std::uint8_t>('Y'),
                        static_cast<std::uint8_t>(y_selector),
                        static_cast<std::uint8_t>('K'),
                        static_cast<std::uint8_t>(k_selector),
                        static_cast<std::uint8_t>(ss),
                        static_cast<std::uint8_t>(se),
                        static_cast<std::uint8_t>((ah << 4) | al)};
  return append_jpeg_segment(out, 0xdau, sos, sizeof(sos));
}
int jpeg_append_cmyk_single_sos_segment(std::vector<std::uint8_t> &out,
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
int jpeg_encode_cmyk_interleaved_entropy(
    const std::vector<int> &c_blocks, const std::vector<int> &m_blocks,
    const std::vector<int> &y_blocks, const std::vector<int> &k_blocks,
    int c_blocks_per_mcu, const JpegHuffmanTable &dc_table,
    const JpegHuffmanTable &ac_table, std::vector<std::uint8_t> *out,
    std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  if (c_blocks_per_mcu <= 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const std::size_t mcu_count = m_blocks.size() / 64u;
  if (y_blocks.size() / 64u != mcu_count ||
      k_blocks.size() / 64u != mcu_count ||
      c_blocks.size() / 64u !=
          mcu_count * static_cast<std::size_t>(c_blocks_per_mcu)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int previous_c_dc = 0;
    int previous_m_dc = 0;
    int previous_y_dc = 0;
    int previous_k_dc = 0;
    int restart_index = 0;
    std::size_t c_block_index = 0;
    for (std::size_t mcu = 0; mcu < mcu_count; ++mcu) {
      if (restart_interval != 0u && mcu != 0u && mcu % restart_interval == 0u) {
        jpeg_bit_writer_flush(&writer);
        out->push_back(0xffu);
        out->push_back(static_cast<std::uint8_t>(0xd0u + (restart_index & 7)));
        ++restart_index;
        previous_c_dc = 0;
        previous_m_dc = 0;
        previous_y_dc = 0;
        previous_k_dc = 0;
      }
      for (int c_block = 0; c_block < c_blocks_per_mcu; ++c_block) {
        const int *c_coeffs = c_blocks.data() + c_block_index * 64u;
        ++c_block_index;
        const int status = jpeg_encode_block_entropy(
            &writer, c_coeffs, &previous_c_dc, dc_table, ac_table);
        if (status != PILLOW_C_OK) {
          return status;
        }
      }
      const int *m_coeffs = m_blocks.data() + mcu * 64u;
      int status = jpeg_encode_block_entropy(&writer, m_coeffs, &previous_m_dc,
                                             dc_table, ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      const int *y_coeffs = y_blocks.data() + mcu * 64u;
      status = jpeg_encode_block_entropy(&writer, y_coeffs, &previous_y_dc,
                                         dc_table, ac_table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      const int *k_coeffs = k_blocks.data() + mcu * 64u;
      status = jpeg_encode_block_entropy(&writer, k_coeffs, &previous_k_dc,
                                         dc_table, ac_table);
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
int jpeg_encode_progressive_cmyk_dc_first_scan(
    const std::vector<int> &c_blocks, const std::vector<int> &m_blocks,
    const std::vector<int> &y_blocks, const std::vector<int> &k_blocks,
    int c_blocks_per_mcu, int al, const JpegHuffmanTable &dc_table,
    std::vector<std::uint8_t> *out, std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  if (c_blocks_per_mcu <= 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const std::size_t mcu_count = m_blocks.size() / 64u;
  if (y_blocks.size() / 64u != mcu_count ||
      k_blocks.size() / 64u != mcu_count ||
      c_blocks.size() / 64u !=
          mcu_count * static_cast<std::size_t>(c_blocks_per_mcu)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int previous_c_dc = 0;
    int previous_m_dc = 0;
    int previous_y_dc = 0;
    int previous_k_dc = 0;
    int restart_index = 0;
    std::size_t c_block_index = 0;
    for (std::size_t mcu = 0; mcu < mcu_count; ++mcu) {
      if (restart_interval != 0u && mcu != 0u && mcu % restart_interval == 0u) {
        jpeg_write_restart_marker(&writer, &restart_index);
        previous_c_dc = 0;
        previous_m_dc = 0;
        previous_y_dc = 0;
        previous_k_dc = 0;
      }
      for (int c_block = 0; c_block < c_blocks_per_mcu; ++c_block) {
        const int *c_coeffs = c_blocks.data() + c_block_index * 64u;
        ++c_block_index;
        int dc = jpeg_successive_dc_value(c_coeffs[0], al);
        int diff = dc - previous_c_dc;
        previous_c_dc = dc;
        int category = jpeg_value_category(diff);
        int status = jpeg_write_huffman_symbol(&writer, dc_table, category);
        if (status != PILLOW_C_OK) {
          return status;
        }
        jpeg_bit_writer_write(&writer, jpeg_value_bits(diff, category),
                              category);
      }
      const int *m_coeffs = m_blocks.data() + mcu * 64u;
      int dc = jpeg_successive_dc_value(m_coeffs[0], al);
      int diff = dc - previous_m_dc;
      previous_m_dc = dc;
      int category = jpeg_value_category(diff);
      int status = jpeg_write_huffman_symbol(&writer, dc_table, category);
      if (status != PILLOW_C_OK) {
        return status;
      }
      jpeg_bit_writer_write(&writer, jpeg_value_bits(diff, category), category);
      const int *y_coeffs = y_blocks.data() + mcu * 64u;
      dc = jpeg_successive_dc_value(y_coeffs[0], al);
      diff = dc - previous_y_dc;
      previous_y_dc = dc;
      category = jpeg_value_category(diff);
      status = jpeg_write_huffman_symbol(&writer, dc_table, category);
      if (status != PILLOW_C_OK) {
        return status;
      }
      jpeg_bit_writer_write(&writer, jpeg_value_bits(diff, category), category);
      const int *k_coeffs = k_blocks.data() + mcu * 64u;
      dc = jpeg_successive_dc_value(k_coeffs[0], al);
      diff = dc - previous_k_dc;
      previous_k_dc = dc;
      category = jpeg_value_category(diff);
      status = jpeg_write_huffman_symbol(&writer, dc_table, category);
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
int jpeg_encode_progressive_cmyk_dc_refine_scan(
    const std::vector<int> &c_blocks, const std::vector<int> &m_blocks,
    const std::vector<int> &y_blocks, const std::vector<int> &k_blocks,
    int c_blocks_per_mcu, int al, std::vector<std::uint8_t> *out,
    std::uint16_t restart_interval) {
  if (!out) {
    return PILLOW_C_NULL_POINTER;
  }
  if (c_blocks_per_mcu <= 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const std::size_t mcu_count = m_blocks.size() / 64u;
  if (y_blocks.size() / 64u != mcu_count ||
      k_blocks.size() / 64u != mcu_count ||
      c_blocks.size() / 64u !=
          mcu_count * static_cast<std::size_t>(c_blocks_per_mcu)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegBitWriter writer;
    writer.out = out;
    int restart_index = 0;
    std::size_t c_block_index = 0;
    for (std::size_t mcu = 0; mcu < mcu_count; ++mcu) {
      if (restart_interval != 0u && mcu != 0u && mcu % restart_interval == 0u) {
        jpeg_write_restart_marker(&writer, &restart_index);
      }
      for (int c_block = 0; c_block < c_blocks_per_mcu; ++c_block) {
        const int *c_coeffs = c_blocks.data() + c_block_index * 64u;
        ++c_block_index;
        jpeg_bit_writer_write(
            &writer,
            static_cast<std::uint16_t>(jpeg_successive_bit(c_coeffs[0], al)),
            1);
      }
      const int *m_coeffs = m_blocks.data() + mcu * 64u;
      const int *y_coeffs = y_blocks.data() + mcu * 64u;
      const int *k_coeffs = k_blocks.data() + mcu * 64u;
      jpeg_bit_writer_write(
          &writer,
          static_cast<std::uint16_t>(jpeg_successive_bit(m_coeffs[0], al)), 1);
      jpeg_bit_writer_write(
          &writer,
          static_cast<std::uint16_t>(jpeg_successive_bit(y_coeffs[0], al)), 1);
      jpeg_bit_writer_write(
          &writer,
          static_cast<std::uint16_t>(jpeg_successive_bit(k_coeffs[0], al)), 1);
    }
    jpeg_bit_writer_flush(&writer);
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int save_jpeg_cmyk_baseline(const PillowCImage *image, const char *path,
                            int quality, const int *qtables,
                            std::size_t qtable_count, bool optimize,
                            bool has_dpi, double dpi_x, double dpi_y,
                            int subsampling, std::uint16_t restart_interval) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_CMYK || image->channels != 4 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegCmykPreparedBlocks prepared;
    int status = jpeg_prepare_cmyk_blocks(image, quality, qtables, qtable_count,
                                          &prepared, subsampling);
    if (status != PILLOW_C_OK) {
      return status;
    }
    JpegHuffmanTable dc_table;
    JpegHuffmanTable ac_table;
    if (optimize) {
      std::uint64_t dc_freq[256] = {};
      std::uint64_t ac_freq[256] = {};
      const std::size_t c_restart_interval =
          static_cast<std::size_t>(restart_interval) *
          static_cast<std::size_t>(prepared.c_blocks_per_mcu);
      jpeg_collect_huffman_frequencies(prepared.c_blocks, dc_freq, ac_freq,
                                       c_restart_interval);
      jpeg_collect_huffman_frequencies(prepared.m_blocks, dc_freq, ac_freq,
                                       restart_interval);
      jpeg_collect_huffman_frequencies(prepared.y_blocks, dc_freq, ac_freq,
                                       restart_interval);
      jpeg_collect_huffman_frequencies(prepared.k_blocks, dc_freq, ac_freq,
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
    out.reserve(512u + prepared.c_blocks.size() + prepared.m_blocks.size() +
                prepared.y_blocks.size() + prepared.k_blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    if (has_dpi) {
      std::uint8_t jfif_unit = 0;
      std::uint16_t jfif_x_density = 0;
      std::uint16_t jfif_y_density = 0;
      status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit, &jfif_x_density,
                                     &jfif_y_density);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status =
          jpeg_append_jfif_app0(out, jfif_unit, jfif_x_density, jfif_y_density);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_adobe_rgb_app14(out);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_dqt(out, prepared.qtable);
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (prepared.has_myk_qtable) {
      status = jpeg_append_dqt_table(out, 1, prepared.myk_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_cmyk_sof(out, image, prepared.has_myk_qtable ? 1 : 0,
                                  0xc0u, prepared.h_samp, prepared.v_samp);
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
    status = jpeg_append_cmyk_sos_segment(out);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_cmyk_interleaved_entropy(
        prepared.c_blocks, prepared.m_blocks, prepared.y_blocks,
        prepared.k_blocks, prepared.c_blocks_per_mcu, dc_table, ac_table, &out,
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
int save_jpeg_cmyk_progressive(const PillowCImage *image, const char *path,
                               int quality, bool has_dpi, double dpi_x,
                               double dpi_y, const int *qtables,
                               std::size_t qtable_count, int subsampling,
                               std::uint16_t restart_interval,
                               std::uint16_t c_ac_restart_interval) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_CMYK || image->channels != 4 ||
      image->width <= 0 || image->height <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max() ||
      image->height > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  try {
    JpegCmykPreparedBlocks prepared;
    int status = jpeg_prepare_cmyk_blocks(image, quality, qtables, qtable_count,
                                          &prepared, subsampling);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const int c_block_columns = (image->width + 7) / 8;
    const int c_block_rows = (image->height + 7) / 8;
    const int mcu_columns =
        (image->width + (8 * prepared.h_samp) - 1) / (8 * prepared.h_samp);
    const std::size_t c_scan_block_count =
        static_cast<std::size_t>(c_block_columns) *
        static_cast<std::size_t>(c_block_rows);
    if (c_scan_block_count > prepared.c_blocks.size() / 64u) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<int> c_scan_blocks;
    c_scan_blocks.reserve(c_scan_block_count * 64u);
    for (int block_y = 0; block_y < c_block_rows; ++block_y) {
      const int mcu_y = block_y / prepared.v_samp;
      const int block_y_in_mcu = block_y % prepared.v_samp;
      for (int block_x = 0; block_x < c_block_columns; ++block_x) {
        const int mcu_x = block_x / prepared.h_samp;
        const int block_x_in_mcu = block_x % prepared.h_samp;
        const std::size_t mcu_index =
            static_cast<std::size_t>(mcu_y) *
                static_cast<std::size_t>(mcu_columns) +
            static_cast<std::size_t>(mcu_x);
        const std::size_t source_block =
            mcu_index * static_cast<std::size_t>(prepared.c_blocks_per_mcu) +
            static_cast<std::size_t>(block_y_in_mcu * prepared.h_samp +
                                     block_x_in_mcu);
        if (source_block >= prepared.c_blocks.size() / 64u) {
          return PILLOW_C_INVALID_ARGUMENT;
        }
        const int *coefficients = prepared.c_blocks.data() + source_block * 64u;
        c_scan_blocks.insert(c_scan_blocks.end(), coefficients,
                             coefficients + 64);
      }
    }
    std::uint64_t dc_first_freq[256] = {};
    std::uint64_t c_ac_first_low_freq[256] = {};
    std::uint64_t m_ac_first_low_freq[256] = {};
    std::uint64_t y_ac_first_low_freq[256] = {};
    std::uint64_t k_ac_first_low_freq[256] = {};
    std::uint64_t c_ac_first_high_freq[256] = {};
    std::uint64_t m_ac_first_high_freq[256] = {};
    std::uint64_t y_ac_first_high_freq[256] = {};
    std::uint64_t k_ac_first_high_freq[256] = {};
    std::uint64_t c_ac_refine_mid_freq[256] = {};
    std::uint64_t m_ac_refine_mid_freq[256] = {};
    std::uint64_t y_ac_refine_mid_freq[256] = {};
    std::uint64_t k_ac_refine_mid_freq[256] = {};
    std::uint64_t c_ac_refine_final_freq[256] = {};
    std::uint64_t m_ac_refine_final_freq[256] = {};
    std::uint64_t y_ac_refine_final_freq[256] = {};
    std::uint64_t k_ac_refine_final_freq[256] = {};
    const std::size_t c_dc_restart_interval =
        static_cast<std::size_t>(restart_interval) *
        static_cast<std::size_t>(prepared.c_blocks_per_mcu);
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.c_blocks, 1, dc_first_freq, c_dc_restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.m_blocks, 1, dc_first_freq, restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.y_blocks, 1, dc_first_freq, restart_interval);
    jpeg_collect_progressive_dc_first_frequencies(
        prepared.k_blocks, 1, dc_first_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        c_scan_blocks, 1, 5, 2, c_ac_first_low_freq, c_ac_restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.m_blocks, 1, 5, 2, m_ac_first_low_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.y_blocks, 1, 5, 2, y_ac_first_low_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.k_blocks, 1, 5, 2, k_ac_first_low_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        c_scan_blocks, 6, 63, 2, c_ac_first_high_freq, c_ac_restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.m_blocks, 6, 63, 2, m_ac_first_high_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.y_blocks, 6, 63, 2, y_ac_first_high_freq, restart_interval);
    jpeg_collect_progressive_ac_first_frequencies(
        prepared.k_blocks, 6, 63, 2, k_ac_first_high_freq, restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(c_scan_blocks, 1, 63, 2, 1,
                                                   c_ac_refine_mid_freq,
                                                   c_ac_restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(
        prepared.m_blocks, 1, 63, 2, 1, m_ac_refine_mid_freq, restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(
        prepared.y_blocks, 1, 63, 2, 1, y_ac_refine_mid_freq, restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(
        prepared.k_blocks, 1, 63, 2, 1, k_ac_refine_mid_freq, restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(c_scan_blocks, 1, 63, 1, 0,
                                                   c_ac_refine_final_freq,
                                                   c_ac_restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.m_blocks, 1, 63, 1,
                                                   0, m_ac_refine_final_freq,
                                                   restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.y_blocks, 1, 63, 1,
                                                   0, y_ac_refine_final_freq,
                                                   restart_interval);
    jpeg_collect_progressive_ac_refine_frequencies(prepared.k_blocks, 1, 63, 1,
                                                   0, k_ac_refine_final_freq,
                                                   restart_interval);
    JpegHuffmanTable dc_first_table;
    JpegHuffmanTable c_ac_first_low_table;
    JpegHuffmanTable m_ac_first_low_table;
    JpegHuffmanTable y_ac_first_low_table;
    JpegHuffmanTable k_ac_first_low_table;
    JpegHuffmanTable c_ac_first_high_table;
    JpegHuffmanTable m_ac_first_high_table;
    JpegHuffmanTable y_ac_first_high_table;
    JpegHuffmanTable k_ac_first_high_table;
    JpegHuffmanTable c_ac_refine_mid_table;
    JpegHuffmanTable m_ac_refine_mid_table;
    JpegHuffmanTable y_ac_refine_mid_table;
    JpegHuffmanTable k_ac_refine_mid_table;
    JpegHuffmanTable c_ac_refine_final_table;
    JpegHuffmanTable m_ac_refine_final_table;
    JpegHuffmanTable y_ac_refine_final_table;
    JpegHuffmanTable k_ac_refine_final_table;
    status = jpeg_build_optimized_huffman_table(dc_first_freq, &dc_first_table);
    if (status != PILLOW_C_OK) {
      return status;
    }
    JpegHuffmanTable *ac_tables[] = {
        &c_ac_first_low_table,    &m_ac_first_low_table,
        &y_ac_first_low_table,    &k_ac_first_low_table,
        &c_ac_first_high_table,   &m_ac_first_high_table,
        &y_ac_first_high_table,   &k_ac_first_high_table,
        &c_ac_refine_mid_table,   &m_ac_refine_mid_table,
        &y_ac_refine_mid_table,   &k_ac_refine_mid_table,
        &c_ac_refine_final_table, &m_ac_refine_final_table,
        &y_ac_refine_final_table, &k_ac_refine_final_table};
    std::uint64_t *ac_freqs[] = {
        c_ac_first_low_freq,    m_ac_first_low_freq,    y_ac_first_low_freq,
        k_ac_first_low_freq,    c_ac_first_high_freq,   m_ac_first_high_freq,
        y_ac_first_high_freq,   k_ac_first_high_freq,   c_ac_refine_mid_freq,
        m_ac_refine_mid_freq,   y_ac_refine_mid_freq,   k_ac_refine_mid_freq,
        c_ac_refine_final_freq, m_ac_refine_final_freq, y_ac_refine_final_freq,
        k_ac_refine_final_freq};
    for (int i = 0; i < 16; ++i) {
      status = jpeg_build_optimized_huffman_table(ac_freqs[i], ac_tables[i]);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    std::vector<std::uint8_t> out;
    out.reserve(1536u + prepared.c_blocks.size() + prepared.m_blocks.size() +
                prepared.y_blocks.size() + prepared.k_blocks.size());
    out.push_back(0xffu);
    out.push_back(0xd8u);
    if (has_dpi) {
      std::uint8_t jfif_unit = 0;
      std::uint16_t jfif_x_density = 0;
      std::uint16_t jfif_y_density = 0;
      status = jpeg_density_from_dpi(dpi_x, dpi_y, &jfif_unit, &jfif_x_density,
                                     &jfif_y_density);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status =
          jpeg_append_jfif_app0(out, jfif_unit, jfif_x_density, jfif_y_density);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_adobe_rgb_app14(out);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_append_luminance_dqt(out, prepared.qtable);
    if (status != PILLOW_C_OK) {
      return status;
    }
    if (prepared.has_myk_qtable) {
      status = jpeg_append_dqt_table(out, 1, prepared.myk_qtable);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = jpeg_append_cmyk_sof(out, image, prepared.has_myk_qtable ? 1 : 0,
                                  0xc2u, prepared.h_samp, prepared.v_samp);
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
    status = jpeg_append_cmyk_interleaved_sos_segment(out, 0x00, 0x00, 0x00,
                                                      0x00, 0, 0, 0, 1);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_cmyk_dc_first_scan(
        prepared.c_blocks, prepared.m_blocks, prepared.y_blocks,
        prepared.k_blocks, prepared.c_blocks_per_mcu, 1, dc_first_table, &out,
        restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    std::uint16_t active_restart_interval = restart_interval;
    const struct AcFirstScan {
      const std::vector<int> *blocks;
      const JpegHuffmanTable *table;
      int component;
      int ss;
      int se;
      int al;
      std::uint16_t restart_interval;
    } ac_first_scans[] = {
        {&c_scan_blocks, &c_ac_first_low_table, 'C', 1, 5, 2,
         c_ac_restart_interval},
        {&prepared.m_blocks, &m_ac_first_low_table, 'M', 1, 5, 2,
         restart_interval},
        {&prepared.y_blocks, &y_ac_first_low_table, 'Y', 1, 5, 2,
         restart_interval},
        {&prepared.k_blocks, &k_ac_first_low_table, 'K', 1, 5, 2,
         restart_interval},
        {&c_scan_blocks, &c_ac_first_high_table, 'C', 6, 63, 2,
         c_ac_restart_interval},
        {&prepared.m_blocks, &m_ac_first_high_table, 'M', 6, 63, 2,
         restart_interval},
        {&prepared.y_blocks, &y_ac_first_high_table, 'Y', 6, 63, 2,
         restart_interval},
        {&prepared.k_blocks, &k_ac_first_high_table, 'K', 6, 63, 2,
         restart_interval},
    };
    for (const AcFirstScan &scan : ac_first_scans) {
      status = jpeg_append_dht_segment(out, 1, 0, *scan.table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      if (scan.restart_interval != active_restart_interval) {
        status = jpeg_append_dri_segment(out, scan.restart_interval);
        if (status != PILLOW_C_OK) {
          return status;
        }
        active_restart_interval = scan.restart_interval;
      }
      status = jpeg_append_cmyk_single_sos_segment(
          out, scan.component, 0x00, scan.ss, scan.se, 0, scan.al);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_progressive_ac_first_scan(
          *scan.blocks, scan.ss, scan.se, scan.al, *scan.table, &out,
          scan.restart_interval);
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
      std::uint16_t restart_interval;
    } ac_refine_mid_scans[] = {
        {&c_scan_blocks, &c_ac_refine_mid_table, 'C', 2, 1,
         c_ac_restart_interval},
        {&prepared.m_blocks, &m_ac_refine_mid_table, 'M', 2, 1,
         restart_interval},
        {&prepared.y_blocks, &y_ac_refine_mid_table, 'Y', 2, 1,
         restart_interval},
        {&prepared.k_blocks, &k_ac_refine_mid_table, 'K', 2, 1,
         restart_interval},
    };
    for (const AcRefineScan &scan : ac_refine_mid_scans) {
      status = jpeg_append_dht_segment(out, 1, 0, *scan.table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      if (scan.restart_interval != active_restart_interval) {
        status = jpeg_append_dri_segment(out, scan.restart_interval);
        if (status != PILLOW_C_OK) {
          return status;
        }
        active_restart_interval = scan.restart_interval;
      }
      status = jpeg_append_cmyk_single_sos_segment(out, scan.component, 0x00, 1,
                                                   63, scan.ah, scan.al);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_progressive_ac_refine_scan(
          *scan.blocks, 1, 63, scan.ah, scan.al, *scan.table, &out,
          scan.restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    if (restart_interval != active_restart_interval) {
      status = jpeg_append_dri_segment(out, restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
      active_restart_interval = restart_interval;
    }
    status = jpeg_append_cmyk_interleaved_sos_segment(out, 0x00, 0x00, 0x00,
                                                      0x00, 0, 0, 1, 0);
    if (status != PILLOW_C_OK) {
      return status;
    }
    status = jpeg_encode_progressive_cmyk_dc_refine_scan(
        prepared.c_blocks, prepared.m_blocks, prepared.y_blocks,
        prepared.k_blocks, prepared.c_blocks_per_mcu, 0, &out,
        restart_interval);
    if (status != PILLOW_C_OK) {
      return status;
    }
    const AcRefineScan ac_refine_final_scans[] = {
        {&c_scan_blocks, &c_ac_refine_final_table, 'C', 1, 0,
         c_ac_restart_interval},
        {&prepared.m_blocks, &m_ac_refine_final_table, 'M', 1, 0,
         restart_interval},
        {&prepared.y_blocks, &y_ac_refine_final_table, 'Y', 1, 0,
         restart_interval},
        {&prepared.k_blocks, &k_ac_refine_final_table, 'K', 1, 0,
         restart_interval},
    };
    for (const AcRefineScan &scan : ac_refine_final_scans) {
      status = jpeg_append_dht_segment(out, 1, 0, *scan.table);
      if (status != PILLOW_C_OK) {
        return status;
      }
      if (scan.restart_interval != active_restart_interval) {
        status = jpeg_append_dri_segment(out, scan.restart_interval);
        if (status != PILLOW_C_OK) {
          return status;
        }
        active_restart_interval = scan.restart_interval;
      }
      status = jpeg_append_cmyk_single_sos_segment(out, scan.component, 0x00, 1,
                                                   63, scan.ah, scan.al);
      if (status != PILLOW_C_OK) {
        return status;
      }
      status = jpeg_encode_progressive_ac_refine_scan(
          *scan.blocks, 1, 63, scan.ah, scan.al, *scan.table, &out,
          scan.restart_interval);
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
} // namespace pillow_c_jpeg
