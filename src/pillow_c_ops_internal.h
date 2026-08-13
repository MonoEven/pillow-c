#pragma once

#include "pillow_c_internal.h"

// Private native seams between the operations family translation units.
// These declarations are not part of the DLL ABI.
bool pillow_c_ops_supports_imageops_lut(const PillowCImage* source);
int pillow_c_ops_apply_point_lut_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    PillowCImage* target);
int pillow_c_ops_apply_single_lut_into(
    const PillowCImage* source,
    const std::uint8_t* lut,
    std::size_t lut_size,
    PillowCImage* target);

int histogram_image(
    const PillowCImage* source,
    std::uint64_t* out_histogram,
    std::size_t out_count);
int histogram_image_masked(
    const PillowCImage* source,
    const PillowCImage* mask,
    std::uint64_t* out_histogram,
    std::size_t out_count);
bool autocontrast_supported_mode(const PillowCImage* source);
int autocontrast_image_into(
    const PillowCImage* source,
    double low_cutoff,
    double high_cutoff,
    const std::uint8_t* ignore_values,
    std::size_t ignore_count,
    const PillowCImage* mask,
    bool preserve_tone,
    PillowCImage* target);
