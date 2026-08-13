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

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "pillow_c_wic_internal.h"
#include <wincodec.h>
#include <windows.h>

namespace pillow_c_jpeg {
int jpeg_restart_interval_from_rows(const PillowCImage *image,
                                    int restart_marker_rows,
                                    int *out_restart_interval);
int save_jpeg_restart_marker_blocks_core(const PillowCImage *image,
                                         const char *path, int quality,
                                         int restart_marker_blocks,
                                         bool optimize_huffman, bool has_dpi,
                                         double dpi_x, double dpi_y);
int save_jpeg_progressive_restart_marker_core(
    const PillowCImage *image, const char *path, int quality,
    int restart_marker_blocks, int restart_marker_rows, bool has_dpi,
    double dpi_x, double dpi_y);
int jpeg_wic_subsampling_option(int subsampling,
                                WICJpegYCrCbSubsamplingOption *out_option) {
  if (!out_option) {
    return PILLOW_C_NULL_POINTER;
  }
  switch (subsampling) {
  case 0:
    *out_option = WICJpegYCrCbSubsampling444;
    return PILLOW_C_OK;
  case 1:
    *out_option = WICJpegYCrCbSubsampling422;
    return PILLOW_C_OK;
  case 2:
    *out_option = WICJpegYCrCbSubsampling420;
    return PILLOW_C_OK;
  default:
    return PILLOW_C_INVALID_ARGUMENT;
  }
}
int save_jpeg_image_with_options(const PillowCImage *image, const char *path,
                                 int quality, bool has_dpi, double dpi_x,
                                 double dpi_y, int subsampling, int progressive,
                                 int optimize) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->width <= 0 || image->height <= 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (!((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
        (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
        (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4))) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
  if (refresh_status != PILLOW_C_OK) {
    return refresh_status;
  }
  if (image->stride >
          static_cast<std::size_t>(std::numeric_limits<UINT>::max()) ||
      image->pixels.size() >
          static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const bool has_quality = quality != -1;
  const int clamped_quality = std::max(0, std::min(quality, 100));
  if (progressive < -1 || progressive > 1 || optimize < -1 || optimize > 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (image->mode == PILLOW_C_MODE_CMYK) {
    if (progressive == 1) {
      if (subsampling != -1) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
      return save_jpeg_cmyk_progressive(image, path, quality, has_dpi, dpi_x,
                                        dpi_y);
    }
    if (optimize == 1 && subsampling != -1) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_jpeg_cmyk_baseline(image, path, quality, nullptr, 0u,
                                   optimize == 1,
                                   has_dpi, dpi_x, dpi_y, subsampling, 0u);
  }
  if (progressive == 1) {
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1 &&
        subsampling == -1) {
      return save_jpeg_l_progressive_huffman(image, path, quality, has_dpi,
                                             dpi_x, dpi_y);
    }
    if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
      return save_jpeg_rgb_progressive_huffman(image, path, quality, has_dpi,
                                               dpi_x, dpi_y, subsampling,
                                               nullptr, nullptr, 1);
    }
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (optimize == 1) {
    if (image->mode == PILLOW_C_MODE_L && image->channels == 1 &&
        subsampling == -1) {
      return save_jpeg_l_optimized_huffman(image, path, quality, has_dpi, dpi_x,
                                           dpi_y);
    }
    if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
      return save_jpeg_rgb_optimized_huffman(image, path, quality, has_dpi,
                                             dpi_x, dpi_y, subsampling);
    }
    return PILLOW_C_INVALID_ARGUMENT;
  }
  bool has_subsampling = subsampling != -1;
  WICJpegYCrCbSubsamplingOption wic_subsampling =
      WICJpegYCrCbSubsamplingDefault;
  if (has_subsampling) {
    const int status =
        jpeg_wic_subsampling_option(subsampling, &wic_subsampling);
    if (status != PILLOW_C_OK) {
      return status;
    }
  }
  if (has_dpi) {
    std::uint8_t unit = 0;
    std::uint16_t x_density = 0;
    std::uint16_t y_density = 0;
    const int status =
        jpeg_density_from_dpi(dpi_x, dpi_y, &unit, &x_density, &y_density);
    if (status != PILLOW_C_OK) {
      return status;
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
    hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr,
                                encoder.put());
    if (FAILED(hr)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> encoder_options;
    const bool has_encoder_options = has_quality || has_subsampling;
    hr = has_encoder_options
             ? encoder->CreateNewFrame(frame.put(), encoder_options.put())
             : encoder->CreateNewFrame(frame.put(), nullptr);
    if (FAILED(hr)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    if (has_encoder_options) {
      if (!encoder_options.get()) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
    }
    if (has_quality) {
      PROPBAG2 option = {};
      option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
      VARIANT value;
      VariantInit(&value);
      value.vt = VT_R4;
      value.fltVal = static_cast<float>(clamped_quality) / 100.0f;
      hr = encoder_options->Write(1, &option, &value);
      if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
    }
    if (has_subsampling) {
      PROPBAG2 option = {};
      option.pstrName = const_cast<LPOLESTR>(L"JpegYCrCbSubsampling");
      VARIANT value;
      VariantInit(&value);
      value.vt = VT_UI1;
      value.bVal = static_cast<BYTE>(wic_subsampling);
      hr = encoder_options->Write(1, &option, &value);
      if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
      }
    }
    hr = frame->Initialize(has_encoder_options ? encoder_options.get()
                                               : nullptr);
    if (FAILED(hr)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    hr = frame->SetSize(static_cast<UINT>(image->width),
                        static_cast<UINT>(image->height));
    if (FAILED(hr)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    if (image->mode == PILLOW_C_MODE_L) {
      format = GUID_WICPixelFormat8bppGray;
    }
    WICPixelFormatGUID encoder_format = format;
    hr = frame->SetPixelFormat(&encoder_format);
    if (FAILED(hr) || !IsEqualGUID(encoder_format, format)) {
      return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<std::uint8_t> encoded_pixels;
    const std::uint8_t *write_data = image->pixels.data();
    if (image->mode == PILLOW_C_MODE_RGB) {
      encoded_pixels.assign(image->pixels.size(), std::uint8_t{0});
      for (int y = 0; y < image->height; ++y) {
        const std::uint8_t *src_row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        std::uint8_t *dst_row =
            encoded_pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
          const std::size_t offset = static_cast<std::size_t>(x) * 3u;
          dst_row[offset + 0u] = src_row[offset + 2u];
          dst_row[offset + 1u] = src_row[offset + 1u];
          dst_row[offset + 2u] = src_row[offset + 0u];
        }
      }
      write_data = encoded_pixels.data();
    }
    hr = frame->WritePixels(static_cast<UINT>(image->height),
                            static_cast<UINT>(image->stride),
                            static_cast<UINT>(image->pixels.size()),
                            const_cast<BYTE *>(write_data));
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
    frame.reset();
    encoder_options.reset();
    encoder.reset();
    stream.reset();
    factory.reset();
    if (has_dpi) {
      status = patch_jpeg_jfif_density(path, dpi_x, dpi_y);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    return PILLOW_C_OK;
  } catch (const std::bad_alloc &) {
    return PILLOW_C_ALLOCATION_FAILED;
  }
}
int save_jpeg_image_with_metadata_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize) {
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status =
      save_jpeg_image_with_options(image, path, quality, has_dpi, dpi_x, dpi_y,
                                   subsampling, progressive, optimize);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_segments(path, comment, comment_size, icc_profile,
                                      icc_profile_size, exif, exif_size, xmp,
                                      xmp_size);
}
int save_jpeg_image_with_metadata_extra_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize, const std::uint8_t *extra,
    std::size_t extra_size) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) ||
      (exif_size > 0u && !exif) || (xmp_size > 0u && !xmp) ||
      (extra_size > 0u && !extra)) {
    return PILLOW_C_NULL_POINTER;
  }
  int status = save_jpeg_image_with_options(
      image, path, quality, has_dpi, dpi_x, dpi_y, subsampling, progressive,
      optimize);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = patch_jpeg_metadata_extra_segments(
      path, extra, extra_size, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
int save_jpeg_image_with_qtables_metadata_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, bool allow_cmyk_progressive_subsampling,
    int restart_marker_blocks, int restart_marker_rows) {
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = save_jpeg_rgb_qtables_optimized_huffman(
      image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, allow_cmyk_progressive_subsampling,
      restart_marker_blocks, restart_marker_rows);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_segments(path, comment, comment_size, icc_profile,
                                      icc_profile_size, exif, exif_size, xmp,
                                      xmp_size);
}
int save_jpeg_image_with_qtables_metadata_extra_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, const std::uint8_t *extra,
    std::size_t extra_size, int restart_marker_blocks = 0,
    int restart_marker_rows = 0) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) ||
      (exif_size > 0u && !exif) || (xmp_size > 0u && !xmp) ||
      (extra_size > 0u && !extra) ||
      (qtable_count > 0u && !qtables)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = save_jpeg_rgb_qtables_optimized_huffman(
      image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, true, restart_marker_blocks,
      restart_marker_rows);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = patch_jpeg_metadata_extra_segments(
      path, extra, extra_size, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (comment_size == 0u) {
    return patch_jpeg_source_comment_segment(image, path);
  }
  return PILLOW_C_OK;
}
int save_jpeg_image_with_metadata_restart_marker_extra_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows, const std::uint8_t *extra,
    std::size_t extra_size) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) ||
      (exif_size > 0u && !exif) || (xmp_size > 0u && !xmp) ||
      (extra_size > 0u && !extra) ||
      (qtable_count > 0u && !qtables) ||
      (qtable_count == 0u && qtables)) {
    return PILLOW_C_NULL_POINTER;
  }
  if (qtable_count > 2u || restart_marker_blocks < 0 ||
      restart_marker_blocks > std::numeric_limits<std::uint16_t>::max() ||
      restart_marker_rows < 0 ||
      (restart_marker_blocks != 0 && restart_marker_rows != 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (qtable_count == 0u && subsampling != -1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }

  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (qtable_count > 0u) {
    return save_jpeg_image_with_qtables_metadata_extra_options(
        image, path, quality, has_dpi, dpi_x, dpi_y, comment, comment_size,
        icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size,
        qtables, qtable_count, subsampling, progressive, optimize, extra,
        extra_size, restart_marker_blocks, restart_marker_rows);
  }

  if (progressive == 1) {
    status = save_jpeg_progressive_restart_marker_core(
        image, path, quality, restart_marker_blocks, restart_marker_rows,
        has_dpi, dpi_x, dpi_y);
  } else {
    int restart_interval = restart_marker_blocks;
    if (restart_marker_rows != 0) {
      status = jpeg_restart_interval_from_rows(
          image, restart_marker_rows, &restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = save_jpeg_restart_marker_blocks_core(
        image, path, quality, restart_interval, optimize == 1, has_dpi, dpi_x,
        dpi_y);
  }
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = patch_jpeg_metadata_extra_segments(
      path, extra, extra_size, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (comment_size == 0u) {
    return patch_jpeg_source_comment_segment(image, path);
  }
  return PILLOW_C_OK;
}
int save_jpeg_image_with_keep_rgb_restart_marker_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, const int *qtables, std::size_t qtable_count,
    int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows, const std::uint8_t *extra = nullptr,
    std::size_t extra_size = 0u) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp) || (qtable_count > 0u && !qtables) ||
      (extra_size > 0u && !extra)) {
    return PILLOW_C_NULL_POINTER;
  }
  if (image->mode != PILLOW_C_MODE_RGB || image->channels != 3 ||
      progressive < -1 || progressive > 1 || optimize < -1 || optimize > 1 ||
      qtable_count > 2u || (qtable_count == 0u && qtables != nullptr)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  std::uint16_t restart_interval = 0u;
  status = jpeg_keep_rgb_restart_interval_from_options(
      image, restart_marker_blocks, restart_marker_rows, &restart_interval);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (qtable_count == 0u) {
    status = save_jpeg_rgb_keep_rgb_options(image, path, quality, false, 0.0,
                                            0.0, -1, progressive, optimize, 1,
                                            restart_interval);
  } else {
    status = save_jpeg_rgb_qtables_keep_rgb_options(
        image, path, quality, false, 0.0, 0.0, qtables, qtable_count, -1,
        progressive, optimize, 1, restart_interval);
  }
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_extra_segments(
      path, extra, extra_size, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size);
}
int save_jpeg_image_with_qtables_metadata_keep_rgb_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize, int keep_rgb) {
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            nullptr, 0u, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = save_jpeg_rgb_qtables_keep_rgb_options(
      image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, keep_rgb);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_segments(path, comment, comment_size, icc_profile,
                                      icc_profile_size, exif, exif_size,
                                      nullptr, 0u);
}
int save_jpeg_image_with_qtables_metadata_keep_rgb_xmp_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int keep_rgb) {
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = save_jpeg_rgb_qtables_keep_rgb_options(
      image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, keep_rgb);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_segments(path, comment, comment_size, icc_profile,
                                      icc_profile_size, exif, exif_size, xmp,
                                      xmp_size);
}
int save_jpeg_image_with_qtables_metadata_keep_rgb_extra_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int keep_rgb,
    const std::uint8_t *extra, std::size_t extra_size) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) ||
      (exif_size > 0u && !exif) || (xmp_size > 0u && !xmp) ||
      (extra_size > 0u && !extra) ||
      (qtable_count > 0u && !qtables)) {
    return PILLOW_C_NULL_POINTER;
  }
  if (keep_rgb != 1 || qtable_count < 1u || qtable_count > 2u ||
      progressive < -1 || progressive > 1 || optimize < -1 || optimize > 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }

  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = save_jpeg_rgb_qtables_keep_rgb_options(
      image, path, quality, has_dpi, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, keep_rgb);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = patch_jpeg_metadata_extra_segments(
      path, extra, extra_size, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (comment_size == 0u) {
    return patch_jpeg_source_comment_segment(image, path);
  }
  return PILLOW_C_OK;
}
int save_jpeg_image_with_metadata_keep_rgb_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize, int keep_rgb) {
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (image && image->mode == PILLOW_C_MODE_CMYK && image->channels == 4 &&
      keep_rgb == 1) {
    status =
        save_jpeg_image_with_options(image, path, quality, has_dpi, dpi_x,
                                     dpi_y, subsampling, progressive, optimize);
  } else {
    status = save_jpeg_rgb_keep_rgb_options(image, path, quality, has_dpi,
                                            dpi_x, dpi_y, subsampling,
                                            progressive, optimize, keep_rgb);
  }
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_segments(path, comment, comment_size, icc_profile,
                                      icc_profile_size, exif, exif_size, xmp,
                                      xmp_size);
}
int save_jpeg_image_with_metadata_keep_rgb_extra_options(
    const PillowCImage *image, const char *path, int quality, bool has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize, int keep_rgb,
    const std::uint8_t *extra, std::size_t extra_size) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) ||
      (exif_size > 0u && !exif) || (xmp_size > 0u && !xmp) ||
      (extra_size > 0u && !extra)) {
    return PILLOW_C_NULL_POINTER;
  }
  if (keep_rgb != 1 || progressive < -1 || progressive > 1 ||
      optimize < -1 || optimize > 1) {
    return PILLOW_C_INVALID_ARGUMENT;
  }

  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
    status = save_jpeg_image_with_options(
        image, path, quality, has_dpi, dpi_x, dpi_y, subsampling, progressive,
        optimize);
  } else {
    status = save_jpeg_rgb_keep_rgb_options(
        image, path, quality, has_dpi, dpi_x, dpi_y, subsampling, progressive,
        optimize, keep_rgb);
  }
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = patch_jpeg_metadata_extra_segments(
      path, extra, extra_size, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
int save_jpeg_image_with_quality(const PillowCImage *image, const char *path,
                                 int quality) {
  const int status = save_jpeg_image_with_options(image, path, quality, false,
                                                  0.0, 0.0, -1, -1, -1);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
int save_jpeg_image(const PillowCImage *image, const char *path) {
  return save_jpeg_image_with_quality(image, path, -1);
}
int jpeg_restart_interval_from_rows(const PillowCImage *image,
                                    int restart_marker_rows,
                                    int *out_restart_interval) {
  if (!image || !out_restart_interval) {
    return PILLOW_C_NULL_POINTER;
  }
  if (restart_marker_rows < 0 || image->width <= 0 ||
      image->width > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  int mcu_columns = 0;
  if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
    mcu_columns = (image->width + 7) / 8;
  } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
    mcu_columns = (image->width + 15) / 16;
  } else if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
    mcu_columns = (image->width + 7) / 8;
  } else {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const std::uint64_t restart_interval =
      static_cast<std::uint64_t>(mcu_columns) *
      static_cast<std::uint64_t>(restart_marker_rows);
  if (restart_interval > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  *out_restart_interval = static_cast<int>(restart_interval);
  return PILLOW_C_OK;
}
int save_jpeg_restart_marker_blocks_core(const PillowCImage *image,
                                         const char *path, int quality,
                                         int restart_marker_blocks,
                                         bool optimize_huffman,
                                         bool has_dpi = false,
                                         double dpi_x = 0.0,
                                         double dpi_y = 0.0) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (restart_marker_blocks < 0 ||
      restart_marker_blocks > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
      return refresh_status;
    }
    return save_jpeg_cmyk_baseline(
        image, path, quality, nullptr, 0u, optimize_huffman, has_dpi, dpi_x,
        dpi_y, -1, static_cast<std::uint16_t>(restart_marker_blocks));
  }
  if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
    return save_jpeg_rgb_optimized_huffman(
        image, path, quality, has_dpi, dpi_x, dpi_y, -1, optimize_huffman,
        restart_marker_blocks);
  }
  if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
    return save_jpeg_l_optimized_huffman(
        image, path, quality, has_dpi, dpi_x, dpi_y, nullptr, optimize_huffman,
        restart_marker_blocks);
  }
  return PILLOW_C_INVALID_ARGUMENT;
}
int save_jpeg_progressive_restart_marker_core(const PillowCImage *image,
                                              const char *path, int quality,
                                              int restart_marker_blocks,
                                              int restart_marker_rows,
                                              bool has_dpi = false,
                                              double dpi_x = 0.0,
                                              double dpi_y = 0.0) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  if (restart_marker_blocks < 0 || restart_marker_rows < 0 ||
      restart_marker_blocks > std::numeric_limits<std::uint16_t>::max() ||
      (restart_marker_blocks != 0 && restart_marker_rows != 0)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const int refresh_status = pillow_c_refresh_const_buffer_view_image(image);
  if (refresh_status != PILLOW_C_OK) {
    return refresh_status;
  }
  if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
    int restart_interval = restart_marker_blocks;
    if (restart_marker_rows != 0) {
      const int status = jpeg_restart_interval_from_rows(
          image, restart_marker_rows, &restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    return save_jpeg_l_progressive_huffman(
        image, path, quality, has_dpi, dpi_x, dpi_y, nullptr,
        restart_interval);
  }
  if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
    return save_jpeg_rgb_progressive_huffman(
        image, path, quality, has_dpi, dpi_x, dpi_y, -1, nullptr, nullptr, 1,
        restart_marker_blocks, restart_marker_rows);
  }
  if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
    int restart_interval = restart_marker_blocks;
    if (restart_marker_rows != 0) {
      const int status = jpeg_restart_interval_from_rows(
          image, restart_marker_rows, &restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    return save_jpeg_cmyk_progressive(
        image, path, quality, has_dpi, dpi_x, dpi_y, nullptr, 0u, -1,
        static_cast<std::uint16_t>(restart_interval),
        static_cast<std::uint16_t>(restart_interval));
  }
  return PILLOW_C_INVALID_ARGUMENT;
}
int save_jpeg_metadata_restart_marker_core(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, int restart_marker_blocks, int restart_marker_rows,
    bool optimize_huffman, bool progressive) {
  if (!image || !path) {
    return PILLOW_C_NULL_POINTER;
  }
  const bool cmyk_optimized_restart = image->mode == PILLOW_C_MODE_CMYK &&
                                      image->channels == 4 &&
                                      optimize_huffman && !progressive;
  const bool cmyk_progressive_restart = image->mode == PILLOW_C_MODE_CMYK &&
                                        image->channels == 4 &&
                                        !optimize_huffman && progressive;
  if (!((image->mode == PILLOW_C_MODE_L && image->channels == 1) ||
        (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) ||
        cmyk_optimized_restart || cmyk_progressive_restart)) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if ((comment_size > 0u && !comment) ||
      (icc_profile_size > 0u && !icc_profile) || (exif_size > 0u && !exif) ||
      (xmp_size > 0u && !xmp)) {
    return PILLOW_C_NULL_POINTER;
  }
  std::vector<std::uint8_t> unused_segments;
  int status = build_jpeg_metadata_segments(comment, comment_size, icc_profile,
                                            icc_profile_size, exif, exif_size,
                                            xmp, xmp_size, &unused_segments);
  if (status != PILLOW_C_OK) {
    return status;
  }
  if (restart_marker_blocks != 0 && restart_marker_rows != 0) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (restart_marker_blocks < 0 ||
      restart_marker_blocks > std::numeric_limits<std::uint16_t>::max()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  if (progressive) {
    status = save_jpeg_progressive_restart_marker_core(
        image, path, quality, restart_marker_blocks, restart_marker_rows);
  } else {
    int restart_interval = restart_marker_blocks;
    if (restart_marker_rows != 0) {
      status = jpeg_restart_interval_from_rows(image, restart_marker_rows,
                                               &restart_interval);
      if (status != PILLOW_C_OK) {
        return status;
      }
    }
    status = save_jpeg_restart_marker_blocks_core(
        image, path, quality, restart_interval, optimize_huffman);
  }
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_metadata_segments(path, comment, comment_size, icc_profile,
                                      icc_profile_size, exif, exif_size, xmp,
                                      xmp_size);
}
} // namespace pillow_c_jpeg

using namespace pillow_c_jpeg;
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg(const PillowCImage *image, const char *path) {
  return save_jpeg_image(image, path);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_quality(const PillowCImage *image, const char *path,
                                 int quality) {
  return save_jpeg_image_with_quality(image, path, quality);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_options(const PillowCImage *image, const char *path,
                                 int quality, int has_dpi, double dpi_x,
                                 double dpi_y) {
  const int status = save_jpeg_image_with_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, -1, -1, -1);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_subsampling_options(const PillowCImage *image,
                                             const char *path, int quality,
                                             int has_dpi, double dpi_x,
                                             double dpi_y, int subsampling) {
  const int status = save_jpeg_image_with_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, subsampling, -1, -1);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, int subsampling, int progressive,
    int optimize) {
  const int status =
      save_jpeg_image_with_options(image, path, quality, has_dpi != 0, dpi_x,
                                   dpi_y, subsampling, progressive, optimize);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg_extra_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, int subsampling, int progressive, int optimize,
    const std::uint8_t *extra, std::size_t extra_size) {
  int status = save_jpeg_image_with_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, subsampling,
      progressive, optimize);
  if (status != PILLOW_C_OK) {
    return status;
  }
  status = patch_jpeg_extra_segments(path, extra, extra_size);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_extra_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize,
    const std::uint8_t *extra, std::size_t extra_size) {
  return save_jpeg_image_with_metadata_extra_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size,
      subsampling, progressive, optimize, extra, extra_size);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_restart_marker_extra_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows, const std::uint8_t *extra,
    std::size_t extra_size) {
  return save_jpeg_image_with_metadata_restart_marker_extra_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size, qtables,
      qtable_count, subsampling, progressive, optimize, restart_marker_blocks,
      restart_marker_rows, extra, extra_size);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_restart_marker_blocks_options(
    const PillowCImage *image, const char *path, int quality,
    int restart_marker_blocks) {
  const int status = save_jpeg_restart_marker_blocks_core(
      image, path, quality, restart_marker_blocks, false);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_restart_marker_rows_options(const PillowCImage *image,
                                                     const char *path,
                                                     int quality,
                                                     int restart_marker_rows) {
  int restart_interval = 0;
  const int status = jpeg_restart_interval_from_rows(image, restart_marker_rows,
                                                     &restart_interval);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return pillow_c_image_save_jpeg_restart_marker_blocks_options(
      image, path, quality, restart_interval);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_restart_marker_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, int restart_marker_blocks, int restart_marker_rows) {
  return save_jpeg_metadata_restart_marker_core(
      image, path, quality, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size, restart_marker_blocks,
      restart_marker_rows, false, false);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_restart_marker_encode_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, int restart_marker_blocks, int restart_marker_rows,
    int optimize) {
  return save_jpeg_metadata_restart_marker_core(
      image, path, quality, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size, restart_marker_blocks,
      restart_marker_rows, optimize != 0, false);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_restart_marker_progressive_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, int restart_marker_blocks, int restart_marker_rows) {
  return save_jpeg_metadata_restart_marker_core(
      image, path, quality, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size, restart_marker_blocks,
      restart_marker_rows, false, true);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_encode_keep_rgb_options(const PillowCImage *image,
                                                 const char *path, int quality,
                                                 int has_dpi, double dpi_x,
                                                 double dpi_y, int subsampling,
                                                 int progressive, int optimize,
                                                 int keep_rgb) {
  int status = PILLOW_C_OK;
  if (image && image->mode == PILLOW_C_MODE_CMYK && image->channels == 4 &&
      keep_rgb == 1) {
    status =
        save_jpeg_image_with_options(image, path, quality, has_dpi != 0, dpi_x,
                                     dpi_y, subsampling, progressive, optimize);
  } else {
    status = save_jpeg_rgb_keep_rgb_options(image, path, quality, has_dpi != 0,
                                            dpi_x, dpi_y, subsampling,
                                            progressive, optimize, keep_rgb);
  }
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize) {
  const int status = save_jpeg_rgb_qtables_optimized_huffman(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, true);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_keep_rgb_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize, int keep_rgb) {
  const int status = save_jpeg_rgb_qtables_keep_rgb_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, qtables, qtable_count,
      subsampling, progressive, optimize, keep_rgb);
  if (status != PILLOW_C_OK) {
    return status;
  }
  return patch_jpeg_source_comment_segment(image, path);
}
extern "C" __declspec(dllexport) int pillow_c_image_save_jpeg_metadata_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size) {
  return save_jpeg_image_with_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, nullptr, 0u, -1, -1, -1);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_subsampling_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, int subsampling) {
  return save_jpeg_image_with_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, nullptr, 0u, subsampling,
      -1, -1);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, int subsampling, int progressive, int optimize) {
  return save_jpeg_image_with_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, nullptr, 0u, subsampling,
      progressive, optimize);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_xmp_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize) {
  return save_jpeg_image_with_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size,
      subsampling, progressive, optimize);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_keep_rgb_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, int subsampling, int progressive, int optimize,
    int keep_rgb) {
  return save_jpeg_image_with_metadata_keep_rgb_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, nullptr, 0u, subsampling,
      progressive, optimize, keep_rgb);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_keep_rgb_extra_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize, int keep_rgb,
    const std::uint8_t *extra, std::size_t extra_size) {
  return save_jpeg_image_with_metadata_keep_rgb_extra_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size,
      subsampling, progressive, optimize, keep_rgb, extra, extra_size);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_metadata_keep_rgb_xmp_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    int subsampling, int progressive, int optimize, int keep_rgb) {
  return save_jpeg_image_with_metadata_keep_rgb_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size,
      subsampling, progressive, optimize, keep_rgb);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize) {
  return save_jpeg_image_with_qtables_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, nullptr, 0u, qtables,
      qtable_count, subsampling, progressive, optimize, true, 0, 0);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_xmp_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize) {
  return save_jpeg_image_with_qtables_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size, qtables,
      qtable_count, subsampling, progressive, optimize, true, 0, 0);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_extra_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, const std::uint8_t *extra,
    std::size_t extra_size) {
  return save_jpeg_image_with_qtables_metadata_extra_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size, qtables,
      qtable_count, subsampling, progressive, optimize, extra, extra_size);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_restart_marker_encode_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows) {
  return save_jpeg_image_with_qtables_metadata_options(
      image, path, quality, false, 0.0, 0.0, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size, qtables, qtable_count,
      subsampling, progressive, optimize, true, restart_marker_blocks,
      restart_marker_rows);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_dpi_restart_marker_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows) {
  return save_jpeg_image_with_qtables_metadata_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size, qtables,
      qtable_count, subsampling, progressive, optimize, true,
      restart_marker_blocks, restart_marker_rows);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_keep_rgb_restart_marker_encode_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, const int *qtables, std::size_t qtable_count,
    int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows) {
  return save_jpeg_image_with_keep_rgb_restart_marker_options(
      image, path, quality, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size, qtables, qtable_count,
      progressive, optimize, restart_marker_blocks, restart_marker_rows);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_keep_rgb_restart_marker_extra_encode_options(
    const PillowCImage *image, const char *path, int quality,
    const std::uint8_t *comment, std::size_t comment_size,
    const std::uint8_t *icc_profile, std::size_t icc_profile_size,
    const std::uint8_t *exif, std::size_t exif_size, const std::uint8_t *xmp,
    std::size_t xmp_size, const int *qtables, std::size_t qtable_count,
    int progressive, int optimize, int restart_marker_blocks,
    int restart_marker_rows, const std::uint8_t *extra,
    std::size_t extra_size) {
  return save_jpeg_image_with_keep_rgb_restart_marker_options(
      image, path, quality, comment, comment_size, icc_profile,
      icc_profile_size, exif, exif_size, xmp, xmp_size, qtables, qtable_count,
      progressive, optimize, restart_marker_blocks, restart_marker_rows,
      extra, extra_size);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const int *qtables, std::size_t qtable_count,
    int subsampling, int progressive, int optimize, int keep_rgb) {
  return save_jpeg_image_with_qtables_metadata_keep_rgb_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, qtables, qtable_count,
      subsampling, progressive, optimize, keep_rgb);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_xmp_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int keep_rgb) {
  return save_jpeg_image_with_qtables_metadata_keep_rgb_xmp_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size, qtables,
      qtable_count, subsampling, progressive, optimize, keep_rgb);
}
extern "C" __declspec(dllexport) int
pillow_c_image_save_jpeg_qtables_metadata_keep_rgb_extra_encode_options(
    const PillowCImage *image, const char *path, int quality, int has_dpi,
    double dpi_x, double dpi_y, const std::uint8_t *comment,
    std::size_t comment_size, const std::uint8_t *icc_profile,
    std::size_t icc_profile_size, const std::uint8_t *exif,
    std::size_t exif_size, const std::uint8_t *xmp, std::size_t xmp_size,
    const int *qtables, std::size_t qtable_count, int subsampling,
    int progressive, int optimize, int keep_rgb, const std::uint8_t *extra,
    std::size_t extra_size) {
  return save_jpeg_image_with_qtables_metadata_keep_rgb_extra_options(
      image, path, quality, has_dpi != 0, dpi_x, dpi_y, comment, comment_size,
      icc_profile, icc_profile_size, exif, exif_size, xmp, xmp_size, qtables,
      qtable_count, subsampling, progressive, optimize, keep_rgb, extra,
      extra_size);
}
