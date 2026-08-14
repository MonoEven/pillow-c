#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "pillow_c_internal.h"

namespace pillow_c_jpeg {

inline constexpr int JPEG_ZIGZAG[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

struct JpegHuffmanTable {
  std::uint8_t counts[17] = {};
  std::vector<std::uint8_t> symbols;
  std::uint16_t codes[256] = {};
  std::uint8_t sizes[256] = {};
};

struct JpegBitWriter {
  std::vector<std::uint8_t> *out = nullptr;
  std::uint8_t current = 0;
  int used = 0;
};

int jpeg_value_category(int value);
std::uint16_t jpeg_value_bits(int value, int category);
void jpeg_scaled_qtable(const int base_qtable[64], int quality,
                        int out_qtable[64]);
void jpeg_scaled_luminance_qtable(int quality, int out_qtable[64]);
void jpeg_scaled_chrominance_qtable(int quality, int out_qtable[64]);
int jpeg_scaled_custom_qtable(const int *source_qtable, int quality,
                              int out_qtable[64]);
std::int64_t jpeg_arithmetic_right_shift(std::int64_t value, int bits);
std::int64_t jpeg_descale(std::int64_t value, int bits);
void jpeg_fdct_quantize_samples(const int samples[64], const int qtable[64],
                                int out_zz[64]);
void jpeg_fdct_quantize_luma_block(const PillowCImage *image, int block_x,
                                   int block_y, const int qtable[64],
                                   int out_zz[64]);
void jpeg_fdct_quantize_plane_block(const std::vector<std::uint8_t> &plane,
                                    int width, int height, int block_x,
                                    int block_y, const int qtable[64],
                                    int out_zz[64]);
void jpeg_collect_huffman_frequencies(const std::vector<int> &blocks,
                                      std::uint64_t dc_freq[256],
                                      std::uint64_t ac_freq[256],
                                      std::size_t restart_interval_blocks = 0u);
int jpeg_build_optimized_huffman_table(const std::uint64_t frequencies[256],
                                       JpegHuffmanTable *table);
int jpeg_build_huffman_table_from_spec(const std::uint8_t counts[16],
                                       const std::uint8_t *symbols,
                                       std::size_t symbol_count,
                                       JpegHuffmanTable *table);
int jpeg_build_standard_luminance_huffman_tables(JpegHuffmanTable *dc_table,
                                                 JpegHuffmanTable *ac_table);
int jpeg_build_standard_chrominance_huffman_tables(JpegHuffmanTable *dc_table,
                                                   JpegHuffmanTable *ac_table);
int jpeg_append_dht_segment(std::vector<std::uint8_t> &out, int table_class,
                            int table_id, const JpegHuffmanTable &table);
void jpeg_bit_writer_emit_byte(JpegBitWriter *writer, std::uint8_t value);
void jpeg_bit_writer_write(JpegBitWriter *writer, std::uint16_t code, int size);
void jpeg_bit_writer_flush(JpegBitWriter *writer);
int jpeg_write_huffman_symbol(JpegBitWriter *writer,
                              const JpegHuffmanTable &table, int symbol);
int jpeg_encode_block_entropy(JpegBitWriter *writer, const int coeffs[64],
                              int *previous_dc,
                              const JpegHuffmanTable &dc_table,
                              const JpegHuffmanTable &ac_table);
int jpeg_encode_luma_entropy(const std::vector<int> &blocks,
                             const JpegHuffmanTable &dc_table,
                             const JpegHuffmanTable &ac_table,
                             std::uint16_t restart_interval,
                             std::vector<std::uint8_t> *out);
int jpeg_successive_value(int value, int al);
int jpeg_successive_dc_value(int value, int al);
int jpeg_successive_bit(int value, int al);
int jpeg_append_dri_segment(std::vector<std::uint8_t> &out,
                            std::uint16_t restart_interval);
void jpeg_write_restart_marker(JpegBitWriter *writer, int *restart_index);
void jpeg_collect_progressive_dc_first_frequencies(
    const std::vector<int> &blocks, int al, std::uint64_t dc_freq[256],
    std::size_t restart_interval_blocks = 0u);
int jpeg_progressive_eob_run_symbol(std::uint32_t eob_run);
void jpeg_collect_progressive_ac_first_frequencies(
    const std::vector<int> &blocks, int ss, int se, int al,
    std::uint64_t ac_freq[256], std::size_t restart_interval_blocks = 0u);
void jpeg_collect_progressive_ac_refine_frequencies(
    const std::vector<int> &blocks, int ss, int se, int ah, int al,
    std::uint64_t ac_freq[256], std::size_t restart_interval_blocks = 0u);
int jpeg_encode_progressive_dc_first_scan(const std::vector<int> &blocks,
                                          int al,
                                          const JpegHuffmanTable &dc_table,
                                          std::vector<std::uint8_t> *out,
                                          std::uint16_t restart_interval = 0);
int jpeg_encode_progressive_dc_refine_scan(const std::vector<int> &blocks,
                                           int al,
                                           std::vector<std::uint8_t> *out,
                                           std::uint16_t restart_interval = 0);
int jpeg_encode_progressive_ac_first_scan(const std::vector<int> &blocks,
                                          int ss, int se, int al,
                                          const JpegHuffmanTable &ac_table,
                                          std::vector<std::uint8_t> *out,
                                          std::uint16_t restart_interval = 0);
int jpeg_emit_correction_bits(JpegBitWriter *writer,
                              const std::vector<int> &bits);
int jpeg_emit_progressive_eob_run(JpegBitWriter *writer,
                                  const JpegHuffmanTable &ac_table,
                                  std::uint32_t *eob_run,
                                  std::vector<int> *correction_bits);
int jpeg_encode_progressive_ac_refine_scan(const std::vector<int> &blocks,
                                           int ss, int se, int ah, int al,
                                           const JpegHuffmanTable &ac_table,
                                           std::vector<std::uint8_t> *out,
                                           std::uint16_t restart_interval = 0);
int jpeg_append_luminance_sos_segment(std::vector<std::uint8_t> &out, int ss,
                                      int se, int ah, int al);
int jpeg_encode_rgb_interleaved_entropy(const std::vector<int> &y_blocks,
                                        const std::vector<int> &cb_blocks,
                                        const std::vector<int> &cr_blocks,
                                        int y_blocks_per_mcu,
                                        const JpegHuffmanTable &luma_dc_table,
                                        const JpegHuffmanTable &luma_ac_table,
                                        const JpegHuffmanTable &chroma_dc_table,
                                        const JpegHuffmanTable &chroma_ac_table,
                                        std::vector<std::uint8_t> *out,
                                        std::uint16_t restart_interval = 0);

int append_jpeg_segment(std::vector<std::uint8_t> &out, std::uint8_t marker,
                        const std::uint8_t *payload, std::size_t payload_size);
int append_jpeg_icc_segment(std::vector<std::uint8_t> &out,
                            const std::uint8_t *profile,
                            std::size_t profile_size);
int append_jpeg_xmp_segment(std::vector<std::uint8_t> &out,
                            const std::uint8_t *xmp, std::size_t xmp_size);
int build_jpeg_metadata_segments(
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, std::vector<std::uint8_t> *out_segments);
int patch_jpeg_metadata_segments(const char *path, const std::uint8_t *comment,
                                 std::size_t comment_size,
                                 const std::uint8_t *icc_profile,
                                 std::size_t icc_profile_size,
                                 const std::uint8_t *exif,
                                 std::size_t exif_size, const std::uint8_t *xmp,
                                 std::size_t xmp_size);
int patch_jpeg_source_comment_segment(const PillowCImage *image,
                                      const char *path);
int patch_jpeg_extra_segments(const char *path, const std::uint8_t *extra,
                              std::size_t extra_size);
int patch_jpeg_metadata_extra_segments(
    const char *path, const std::uint8_t *extra, std::size_t extra_size,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size);
int jpeg_density_from_dpi(double dpi_x, double dpi_y, std::uint8_t *out_unit,
                          std::uint16_t *out_x, std::uint16_t *out_y);
int patch_jpeg_jfif_density(const char *path, double dpi_x, double dpi_y);
int jpeg_append_adobe_rgb_app14(std::vector<std::uint8_t> &out);
int jpeg_append_jfif_app0(std::vector<std::uint8_t> &out, std::uint8_t unit,
                          std::uint16_t x_density, std::uint16_t y_density);
int jpeg_append_dqt_table(std::vector<std::uint8_t> &out, int table_id,
                          const int qtable[64]);
int jpeg_append_luminance_dqt(std::vector<std::uint8_t> &out,
                              const int qtable[64]);
int jpeg_smooth_fullsize_plane(const std::vector<std::uint8_t> &src,
                               int width, int height, int block_width,
                               int smoothing_factor,
                               std::vector<std::uint8_t> *out);
int jpeg_smooth_h2v2_plane(const std::vector<std::uint8_t> &src, int width,
                           int height, int out_width, int smoothing_factor,
                           std::vector<std::uint8_t> *out);
int filter_jpeg_streamtype_file(const char *path, int streamtype);

int save_jpeg_l_optimized_huffman(const PillowCImage *image, const char *path,
                                  int quality, bool has_dpi, double dpi_x,
                                  double dpi_y,
                                  const int *custom_qtable = nullptr,
                                  bool optimize_huffman = true,
                                  int restart_marker_blocks = 0,
                                  int smoothing_factor = 0);
int save_jpeg_l_progressive_huffman(const PillowCImage *image, const char *path,
                                    int quality, bool has_dpi, double dpi_x,
                                    double dpi_y,
                                    const int *custom_qtable = nullptr,
                                    int restart_marker_blocks = 0,
                                    int smoothing_factor = 0);
int save_jpeg_rgb_progressive_huffman(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, int subsampling, const int *custom_luma_qtable,
    const int *custom_chroma_qtable, int chroma_qtable_id,
    int restart_marker_blocks = 0, int restart_marker_rows = 0,
    int smoothing_factor = 0);
int save_jpeg_rgb_optimized_huffman(const PillowCImage *image, const char *path,
                                    int quality, bool has_dpi, double dpi_x,
                                    double dpi_y, int subsampling,
                                    bool optimize_huffman = true,
                                    int restart_marker_blocks = 0,
                                    int smoothing_factor = 0);
int save_jpeg_rgb_qtables_optimized_huffman(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize,
    bool allow_cmyk_progressive_subsampling = false,
    int restart_marker_blocks = 0, int restart_marker_rows = 0);
int save_jpeg_rgb_keep_rgb_options(const PillowCImage *image, const char *path,
                                   int quality, bool has_dpi, double dpi_x,
                                   double dpi_y, int subsampling,
                                   int progressive, int optimize, int keep_rgb,
                                   std::uint16_t restart_interval = 0);
int save_jpeg_rgb_qtables_keep_rgb_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize, int keep_rgb,
    std::uint16_t restart_interval = 0);
int jpeg_keep_rgb_restart_interval_from_options(
    const PillowCImage *image, int restart_marker_blocks,
    int restart_marker_rows, std::uint16_t *out_restart_interval);
int save_jpeg_cmyk_baseline(const PillowCImage *image, const char *path,
                            int quality, const int *qtables = nullptr,
                            std::size_t qtable_count = 0u,
                            bool optimize = false, bool has_dpi = false,
                            double dpi_x = 0.0, double dpi_y = 0.0,
                            int subsampling = -1,
                            std::uint16_t restart_interval = 0u,
                            int smoothing_factor = 0);
int save_jpeg_cmyk_progressive(const PillowCImage *image, const char *path,
                               int quality, bool has_dpi = false,
                               double dpi_x = 0.0, double dpi_y = 0.0,
                               const int *qtables = nullptr,
                               std::size_t qtable_count = 0u,
                               int subsampling = -1,
                               std::uint16_t restart_interval = 0u,
                               std::uint16_t c_ac_restart_interval = 0u,
                               int smoothing_factor = 0);

} // namespace pillow_c_jpeg
