#include <cstddef>
#include <cstdint>
#include <cstring>

#include "pillow_c_internal.h"

extern "C" __declspec(dllexport) int pillow_c_image_metadata_jpeg_comment(
    const PillowCImage *image, int *out_has_comment, std::uint8_t *out_comment,
    std::size_t out_comment_size, std::size_t *out_comment_required) {
  if (!image) {
    return PILLOW_C_NULL_POINTER;
  }
  return copy_metadata_blob(image->jpeg_comment, out_has_comment, out_comment,
                            out_comment_size, out_comment_required);
}
extern "C" __declspec(dllexport) int pillow_c_image_metadata_jpeg_icc_profile(
    const PillowCImage *image, int *out_has_profile, std::uint8_t *out_profile,
    std::size_t out_profile_size, std::size_t *out_profile_required) {
  if (!image) {
    return PILLOW_C_NULL_POINTER;
  }
  return copy_metadata_blob(
      image->jpeg_icc_profile, image->has_jpeg_icc_profile, out_has_profile,
      out_profile, out_profile_size, out_profile_required);
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_icc_profile_state(const PillowCImage *image,
                                               int *out_state) {
  if (!image || !out_state) {
    return PILLOW_C_NULL_POINTER;
  }
  *out_state = image->has_jpeg_icc_profile
                   ? 1
                   : (image->has_jpeg_icc_profile_none ? 2 : 0);
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_photoshop_resource_count(const PillowCImage *image,
                                                      std::size_t *out_count) {
  if (!image || !out_count) {
    return PILLOW_C_NULL_POINTER;
  }
  *out_count = image->jpeg_photoshop_resources.size();
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_photoshop_resource(
    const PillowCImage *image, std::size_t index, int *out_code,
    std::uint8_t *out_value, std::size_t out_value_size,
    std::size_t *out_value_required) {
  if (!image || !out_code || !out_value_required) {
    return PILLOW_C_NULL_POINTER;
  }
  if (index >= image->jpeg_photoshop_resources.size()) {
    return PILLOW_C_INVALID_ARGUMENT;
  }
  const auto &resource = image->jpeg_photoshop_resources[index];
  *out_code = resource.first;
  *out_value_required = resource.second.size();
  if (resource.second.empty() || !out_value) {
    return PILLOW_C_OK;
  }
  if (out_value_size < resource.second.size()) {
    return PILLOW_C_INVALID_LENGTH;
  }
  std::memcpy(out_value, resource.second.data(), resource.second.size());
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_photoshop_resolution_info(
    const PillowCImage *image, int *out_has_resolution_info,
    double *out_x_resolution, int *out_displayed_units_x,
    double *out_y_resolution, int *out_displayed_units_y) {
  if (!image || !out_has_resolution_info || !out_x_resolution ||
      !out_displayed_units_x || !out_y_resolution || !out_displayed_units_y) {
    return PILLOW_C_NULL_POINTER;
  }
  *out_has_resolution_info = image->has_jpeg_photoshop_resolution_info ? 1 : 0;
  *out_x_resolution = image->jpeg_photoshop_x_resolution;
  *out_displayed_units_x = image->jpeg_photoshop_displayed_units_x;
  *out_y_resolution = image->jpeg_photoshop_y_resolution;
  *out_displayed_units_y = image->jpeg_photoshop_displayed_units_y;
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int pillow_c_image_metadata_jpeg_exif(
    const PillowCImage *image, int *out_has_exif, std::uint8_t *out_exif,
    std::size_t out_exif_size, std::size_t *out_exif_required) {
  if (!image) {
    return PILLOW_C_NULL_POINTER;
  }
  return copy_metadata_blob(image->jpeg_exif, out_has_exif, out_exif,
                            out_exif_size, out_exif_required);
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_qtable_count(const PillowCImage *image,
                                          std::size_t *out_count) {
  if (!image || !out_count) {
    return PILLOW_C_NULL_POINTER;
  }
  *out_count = image->jpeg_qtable_count;
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_qtable(const PillowCImage *image,
                                    std::size_t index, int *out_values,
                                    std::size_t out_value_count) {
  if (!image || !out_values) {
    return PILLOW_C_NULL_POINTER;
  }
  if (index >= image->jpeg_qtable_count || out_value_count < 64u ||
      image->jpeg_qtables.size() < (index + 1u) * 64u) {
    return out_value_count < 64u ? PILLOW_C_INVALID_LENGTH
                                 : PILLOW_C_INVALID_ARGUMENT;
  }
  const int *source = image->jpeg_qtables.data() + index * 64u;
  std::memcpy(out_values, source, 64u * sizeof(int));
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int
pillow_c_image_metadata_jpeg_subsampling(const PillowCImage *image,
                                         int *out_subsampling) {
  if (!image || !out_subsampling) {
    return PILLOW_C_NULL_POINTER;
  }
  *out_subsampling = image->jpeg_subsampling;
  return PILLOW_C_OK;
}
extern "C" __declspec(dllexport) int pillow_c_image_metadata_jpeg_open_info(
    const PillowCImage *image, int *out_progressive, int *out_has_adobe,
    int *out_adobe, int *out_adobe_transform) {
  if (!image || !out_progressive || !out_has_adobe || !out_adobe ||
      !out_adobe_transform) {
    return PILLOW_C_NULL_POINTER;
  }
  // API-OPENINFO-001: Pillow's progressive/progression info keys and the
  // adobe/adobe_transform APP14 values for opened JPEGs.
  *out_progressive = image->jpeg_progressive ? 1 : 0;
  *out_has_adobe = image->jpeg_has_adobe ? 1 : 0;
  *out_adobe = image->jpeg_adobe;
  *out_adobe_transform = image->jpeg_adobe_transform;
  return PILLOW_C_OK;
}
