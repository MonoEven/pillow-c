#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "pillow_c_internal.h"

#define CMS_NO_REGISTER_KEYWORD 1
#include <lcms2.h>

namespace {

struct PillowCLabTransforms {
    cmsHTRANSFORM rgb_to_lab = nullptr;
    cmsHTRANSFORM rgba_to_lab = nullptr;
    cmsHTRANSFORM lab_to_rgb = nullptr;
    cmsHTRANSFORM lab_to_rgba = nullptr;

    PillowCLabTransforms() noexcept
    {
        cmsHPROFILE srgb_profile = cmsCreate_sRGBProfile();
        cmsHPROFILE lab_profile = cmsCreateLab2Profile(nullptr);
        if (srgb_profile && lab_profile) {
            rgb_to_lab = cmsCreateTransform(
                srgb_profile,
                TYPE_RGB_8,
                lab_profile,
                TYPE_Lab_8,
                INTENT_PERCEPTUAL,
                cmsFLAGS_NOCACHE);
            rgba_to_lab = cmsCreateTransform(
                srgb_profile,
                TYPE_RGBA_8,
                lab_profile,
                TYPE_Lab_8,
                INTENT_PERCEPTUAL,
                cmsFLAGS_NOCACHE);
            lab_to_rgb = cmsCreateTransform(
                lab_profile,
                TYPE_Lab_8,
                srgb_profile,
                TYPE_RGB_8,
                INTENT_PERCEPTUAL,
                cmsFLAGS_NOCACHE);
            lab_to_rgba = cmsCreateTransform(
                lab_profile,
                TYPE_Lab_8,
                srgb_profile,
                TYPE_RGBA_8,
                INTENT_PERCEPTUAL,
                cmsFLAGS_NOCACHE);
        }
        if (srgb_profile) {
            cmsCloseProfile(srgb_profile);
        }
        if (lab_profile) {
            cmsCloseProfile(lab_profile);
        }
        if (!rgb_to_lab || !rgba_to_lab || !lab_to_rgb || !lab_to_rgba) {
            if (rgb_to_lab) {
                cmsDeleteTransform(rgb_to_lab);
            }
            if (rgba_to_lab) {
                cmsDeleteTransform(rgba_to_lab);
            }
            if (lab_to_rgb) {
                cmsDeleteTransform(lab_to_rgb);
            }
            if (lab_to_rgba) {
                cmsDeleteTransform(lab_to_rgba);
            }
            rgb_to_lab = nullptr;
            rgba_to_lab = nullptr;
            lab_to_rgb = nullptr;
            lab_to_rgba = nullptr;
        }
    }

    ~PillowCLabTransforms()
    {
        if (rgb_to_lab) {
            cmsDeleteTransform(rgb_to_lab);
        }
        if (rgba_to_lab) {
            cmsDeleteTransform(rgba_to_lab);
        }
        if (lab_to_rgb) {
            cmsDeleteTransform(lab_to_rgb);
        }
        if (lab_to_rgba) {
            cmsDeleteTransform(lab_to_rgba);
        }
    }

    PillowCLabTransforms(const PillowCLabTransforms&) = delete;
    PillowCLabTransforms& operator=(const PillowCLabTransforms&) = delete;
};

PillowCLabTransforms& pillow_c_lab_transforms()
{
    static PillowCLabTransforms transforms;
    return transforms;
}

struct PillowCCmsProfile {
    cmsHPROFILE handle = nullptr;
    std::atomic<std::size_t> references{1};
    bool is_builtin_srgb = false;
};

struct PillowCCmsTransform {
    cmsHTRANSFORM handle = nullptr;
    int input_mode = 0;
    int output_mode = 0;
    std::vector<std::uint8_t> output_profile_bytes;
};

int own_cms_transform(
    cmsHTRANSFORM handle,
    int input_mode,
    int output_mode,
    cmsHPROFILE output_profile,
    PillowCCmsTransform** out_transform)
{
    cmsUInt32Number profile_size = 0;
    if (!cmsSaveProfileToMem(output_profile, nullptr, &profile_size) ||
        profile_size == 0) {
        cmsDeleteTransform(handle);
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::uint8_t> profile_bytes(profile_size);
        cmsUInt32Number written = profile_size;
        if (!cmsSaveProfileToMem(output_profile, profile_bytes.data(), &written) ||
            written != profile_size) {
            cmsDeleteTransform(handle);
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* transform = new PillowCCmsTransform{
            handle,
            input_mode,
            output_mode,
            std::move(profile_bytes)};
        *out_transform = transform;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        cmsDeleteTransform(handle);
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int own_cms_profile(
    cmsHPROFILE handle,
    PillowCCmsProfile** out_profile,
    bool is_builtin_srgb = false)
{
    if (!handle) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    PillowCCmsProfile* profile = new (std::nothrow) PillowCCmsProfile{};
    if (!profile) {
        cmsCloseProfile(handle);
        return PILLOW_C_ALLOCATION_FAILED;
    }
    profile->handle = handle;
    profile->is_builtin_srgb = is_builtin_srgb;
    *out_profile = profile;
    return PILLOW_C_OK;
}

const cmsCIEXYZ* cms_profile_media_white_point(const PillowCCmsProfile* profile)
{
    if (profile->is_builtin_srgb) {
        return cmsD50_XYZ();
    }
    return static_cast<const cmsCIEXYZ*>(
        cmsReadTag(profile->handle, cmsSigMediaWhitePointTag));
}

} // namespace

int pillow_c_apply_builtin_lab_transform(
    const PillowCImage* source,
    PillowCImage* target,
    int target_mode)
{
    if (!source || !target) {
        return PILLOW_C_NULL_POINTER;
    }
    if (source->width == 0 || source->height == 0) {
        return PILLOW_C_OK;
    }

    PillowCLabTransforms& transforms = pillow_c_lab_transforms();
    const bool rgb_to_lab =
        (source->mode == PILLOW_C_MODE_RGB ||
         source->mode == PILLOW_C_MODE_RGBA ||
         source->mode == PILLOW_C_MODE_RGBX) &&
        target_mode == PILLOW_C_MODE_LAB;
    const bool lab_to_rgb =
        source->mode == PILLOW_C_MODE_LAB &&
        (target_mode == PILLOW_C_MODE_RGB ||
         target_mode == PILLOW_C_MODE_RGBA ||
         target_mode == PILLOW_C_MODE_RGBX);
    if (!rgb_to_lab && !lab_to_rgb) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    cmsHTRANSFORM transform = rgb_to_lab
        ? (source->mode == PILLOW_C_MODE_RGB ? transforms.rgb_to_lab : transforms.rgba_to_lab)
        : (target_mode == PILLOW_C_MODE_RGB ? transforms.lab_to_rgb : transforms.lab_to_rgba);
    if (!transform) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    if (lab_to_rgb && target->channels == 4) {
        std::fill(target->pixels.begin(), target->pixels.end(), std::uint8_t{255});
    }
    for (int y = 0; y < source->height; ++y) {
        const std::uint8_t* src_row =
            source->pixels.data() + static_cast<std::size_t>(y) * source->stride;
        std::uint8_t* dst_row =
            target->pixels.data() + static_cast<std::size_t>(y) * target->stride;
        cmsDoTransform(
            transform,
            src_row,
            dst_row,
            static_cast<cmsUInt32Number>(source->width));
    }
    return PILLOW_C_OK;
}
// ARCH-MOD-004 CMS ABI exports follow.
extern "C" __declspec(dllexport) int pillow_c_cms_profile_create_srgb(
    PillowCCmsProfile** out_profile)
{
    if (!out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_profile = nullptr;

    return own_cms_profile(cmsCreate_sRGBProfile(), out_profile, true);
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_create_lab(
    double color_temperature,
    PillowCCmsProfile** out_profile)
{
    if (!out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_profile = nullptr;
    if (!std::isfinite(color_temperature)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    cmsCIExyY white_point{};
    const cmsCIExyY* requested_white_point = nullptr;
    if (color_temperature > 0.0) {
        if (!cmsWhitePointFromTemp(&white_point, color_temperature)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        requested_white_point = &white_point;
    }

    return own_cms_profile(cmsCreateLab2Profile(requested_white_point), out_profile);
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_create_xyz(
    PillowCCmsProfile** out_profile)
{
    if (!out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_profile = nullptr;
    return own_cms_profile(cmsCreateXYZProfile(), out_profile);
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_default_intent(
    const PillowCCmsProfile* profile,
    int* out_intent)
{
    if (!profile || !profile->handle || !out_intent) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_intent = static_cast<int>(cmsGetHeaderRenderingIntent(profile->handle));
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_header(
    const PillowCCmsProfile* profile,
    std::uint32_t* out_device_class,
    std::uint32_t* out_color_space,
    std::uint32_t* out_connection_space,
    std::uint32_t* out_encoded_version,
    double* out_version,
    int* out_is_matrix_shaper)
{
    if (!profile || !profile->handle ||
        !out_device_class || !out_color_space || !out_connection_space ||
        !out_encoded_version || !out_version || !out_is_matrix_shaper) {
        return PILLOW_C_NULL_POINTER;
    }

    *out_device_class = static_cast<std::uint32_t>(cmsGetDeviceClass(profile->handle));
    *out_color_space = static_cast<std::uint32_t>(cmsGetColorSpace(profile->handle));
    *out_connection_space = static_cast<std::uint32_t>(cmsGetPCS(profile->handle));
    *out_encoded_version = static_cast<std::uint32_t>(
        cmsGetEncodedICCversion(profile->handle));
    *out_version = static_cast<double>(cmsGetProfileVersion(profile->handle));
    *out_is_matrix_shaper = cmsIsMatrixShaper(profile->handle) ? 1 : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_header_identity(
    const PillowCCmsProfile* profile,
    int* out_year,
    int* out_month,
    int* out_day,
    int* out_hour,
    int* out_minute,
    int* out_second,
    std::uint32_t* out_flags,
    std::uint32_t* out_manufacturer,
    std::uint32_t* out_model,
    std::uint8_t* out_profile_id,
    std::size_t profile_id_size)
{
    if (!profile || !profile->handle ||
        !out_year || !out_month || !out_day ||
        !out_hour || !out_minute || !out_second ||
        !out_flags || !out_manufacturer || !out_model || !out_profile_id) {
        return PILLOW_C_NULL_POINTER;
    }
    if (profile_id_size != 16u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    *out_year = 0;
    *out_month = 0;
    *out_day = 0;
    *out_hour = 0;
    *out_minute = 0;
    *out_second = 0;
    *out_flags = 0;
    *out_manufacturer = 0;
    *out_model = 0;
    std::fill(
        out_profile_id,
        out_profile_id + profile_id_size,
        std::uint8_t{0});

    struct tm creation{};
    if (!cmsGetHeaderCreationDateTime(profile->handle, &creation)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_year = creation.tm_year + 1900;
    *out_month = creation.tm_mon;
    *out_day = creation.tm_mday;
    *out_hour = creation.tm_hour;
    *out_minute = creation.tm_min;
    *out_second = creation.tm_sec;
    *out_flags = static_cast<std::uint32_t>(cmsGetHeaderFlags(profile->handle));
    *out_manufacturer = static_cast<std::uint32_t>(
        cmsGetHeaderManufacturer(profile->handle));
    *out_model = static_cast<std::uint32_t>(cmsGetHeaderModel(profile->handle));
    cmsGetHeaderProfileID(profile->handle, out_profile_id);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_media_white_point(
    const PillowCCmsProfile* profile,
    int* out_present,
    double* out_xyz_x,
    double* out_xyz_y,
    double* out_xyz_z,
    double* out_xyy_x,
    double* out_xyy_y,
    double* out_xyy_luminance)
{
    if (!profile || !profile->handle || !out_present ||
        !out_xyz_x || !out_xyz_y || !out_xyz_z ||
        !out_xyy_x || !out_xyy_y || !out_xyy_luminance) {
        return PILLOW_C_NULL_POINTER;
    }

    *out_present = 0;
    *out_xyz_x = 0.0;
    *out_xyz_y = 0.0;
    *out_xyz_z = 0.0;
    *out_xyy_x = 0.0;
    *out_xyy_y = 0.0;
    *out_xyy_luminance = 0.0;

    const auto* white_point = cms_profile_media_white_point(profile);
    if (!white_point) {
        return PILLOW_C_OK;
    }

    cmsCIExyY chromaticity{};
    cmsXYZ2xyY(&chromaticity, white_point);
    *out_present = 1;
    *out_xyz_x = white_point->X;
    *out_xyz_y = white_point->Y;
    *out_xyz_z = white_point->Z;
    *out_xyy_x = chromaticity.x;
    *out_xyy_y = chromaticity.y;
    *out_xyy_luminance = chromaticity.Y;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int
pillow_c_cms_profile_media_white_point_temperature(
    const PillowCCmsProfile* profile,
    int* out_present,
    double* out_temperature)
{
    if (!profile || !profile->handle || !out_present || !out_temperature) {
        return PILLOW_C_NULL_POINTER;
    }

    *out_present = 0;
    *out_temperature = 0.0;
    const auto* white_point = cms_profile_media_white_point(profile);
    if (!white_point) {
        return PILLOW_C_OK;
    }

    cmsCIExyY chromaticity{};
    cmsXYZ2xyY(&chromaticity, white_point);
    cmsFloat64Number temperature = 0.0;
    if (!cmsTempFromWhitePoint(&temperature, &chromaticity)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    *out_present = 1;
    *out_temperature = static_cast<double>(temperature);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_optional_xyz_tags(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    double* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_present || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 2u || value_count != 12u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_present, out_present + present_count, 0);
    std::fill(out_values, out_values + value_count, 0.0);
    const cmsTagSignature tags[] = {
        cmsSigMediaBlackPointTag,
        cmsSigLuminanceTag
    };
    for (std::size_t index = 0; index < 2u; ++index) {
        const auto* xyz = static_cast<const cmsCIEXYZ*>(
            cmsReadTag(profile->handle, tags[index]));
        if (!xyz) {
            continue;
        }

        cmsCIExyY chromaticity{};
        cmsXYZ2xyY(&chromaticity, xyz);
        const std::size_t offset = index * 6u;
        out_present[index] = 1;
        out_values[offset] = xyz->X;
        out_values[offset + 1u] = xyz->Y;
        out_values[offset + 2u] = xyz->Z;
        out_values[offset + 3u] = chromaticity.x;
        out_values[offset + 4u] = chromaticity.y;
        out_values[offset + 5u] = chromaticity.Y;
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_chromaticity(
    const PillowCCmsProfile* profile,
    int* out_present,
    double* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_present || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (value_count != 9u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    *out_present = 0;
    std::fill(out_values, out_values + value_count, 0.0);
    const auto* chromaticity = static_cast<const cmsCIExyYTRIPLE*>(
        cmsReadTag(profile->handle, cmsSigChromaticityTag));
    if (!chromaticity) {
        return PILLOW_C_OK;
    }

    const cmsCIExyY* values[] = {
        &chromaticity->Red,
        &chromaticity->Green,
        &chromaticity->Blue
    };
    for (std::size_t index = 0; index < 3u; ++index) {
        const std::size_t offset = index * 3u;
        out_values[offset] = values[index]->x;
        out_values[offset + 1u] = values[index]->y;
        out_values[offset + 2u] = values[index]->Y;
    }
    *out_present = 1;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_optional_signatures(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    std::uint32_t* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_present || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 3u || value_count != 3u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_present, out_present + present_count, 0);
    std::fill(out_values, out_values + value_count, std::uint32_t{0});
    const cmsTagSignature tags[] = {
        cmsSigPerceptualRenderingIntentGamutTag,
        cmsSigSaturationRenderingIntentGamutTag,
        cmsSigTechnologyTag
    };
    for (std::size_t index = 0; index < 3u; ++index) {
        const auto* signature = static_cast<const cmsUInt32Number*>(
            cmsReadTag(profile->handle, tags[index]));
        if (!signature) {
            continue;
        }
        out_present[index] = 1;
        out_values[index] = static_cast<std::uint32_t>(*signature);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_optional_text_tags(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    char* out_screening_description,
    std::size_t screening_description_size,
    std::size_t* out_screening_description_required,
    char* out_target,
    std::size_t target_size,
    std::size_t* out_target_required)
{
    if (!profile || !profile->handle || !out_present ||
        !out_screening_description_required || !out_target_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 2u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((!out_screening_description && screening_description_size != 0u) ||
        (!out_target && target_size != 0u)) {
        return PILLOW_C_NULL_POINTER;
    }

    std::fill(out_present, out_present + present_count, 0);
    *out_screening_description_required = 0u;
    *out_target_required = 0u;

    const cmsTagSignature tags[] = {
        cmsSigScreeningDescTag,
        cmsSigCharTargetTag
    };
    const cmsMLU* text_tags[2] = {nullptr, nullptr};
    std::size_t required[2] = {0u, 0u};
    for (std::size_t index = 0; index < 2u; ++index) {
        text_tags[index] = static_cast<const cmsMLU*>(
            cmsReadTag(profile->handle, tags[index]));
        if (!text_tags[index]) {
            continue;
        }
        const cmsUInt32Number text_required = cmsMLUgetUTF8(
            text_tags[index],
            cmsNoLanguage,
            cmsNoCountry,
            nullptr,
            0);
        if (text_required == 0u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        out_present[index] = 1;
        required[index] = static_cast<std::size_t>(text_required);
    }

    *out_screening_description_required = required[0];
    *out_target_required = required[1];
    char* outputs[] = {out_screening_description, out_target};
    const std::size_t output_sizes[] = {
        screening_description_size,
        target_size
    };
    for (std::size_t index = 0; index < 2u; ++index) {
        if (outputs[index] && output_sizes[index] > 0u && !text_tags[index]) {
            outputs[index][0] = '\0';
        }
        if (!outputs[index] || !text_tags[index]) {
            continue;
        }
        if (output_sizes[index] < required[index]) {
            return PILLOW_C_INVALID_LENGTH;
        }
    }
    for (std::size_t index = 0; index < 2u; ++index) {
        if (!outputs[index] || !text_tags[index]) {
            continue;
        }
        if (cmsMLUgetUTF8(
                text_tags[index],
                cmsNoLanguage,
                cmsNoCountry,
                outputs[index],
                static_cast<cmsUInt32Number>(required[index])) != required[index]) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_condition_tags(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    std::uint32_t* out_codes,
    std::size_t code_count,
    double* out_values,
    std::size_t value_count,
    char* out_viewing_description,
    std::size_t viewing_description_size,
    std::size_t* out_viewing_description_required)
{
    if (!profile || !profile->handle || !out_present || !out_codes ||
        !out_values || !out_viewing_description_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 3u || code_count != 4u || value_count != 10u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!out_viewing_description && viewing_description_size != 0u) {
        return PILLOW_C_NULL_POINTER;
    }

    std::fill(out_present, out_present + present_count, 0);
    std::fill(out_codes, out_codes + code_count, std::uint32_t{0});
    std::fill(out_values, out_values + value_count, 0.0);
    *out_viewing_description_required = 0u;

    const auto* measurement = static_cast<const cmsICCMeasurementConditions*>(
        cmsReadTag(profile->handle, cmsSigMeasurementTag));
    if (measurement) {
        out_present[0] = 1;
        out_codes[0] = static_cast<std::uint32_t>(measurement->Observer);
        out_codes[1] = static_cast<std::uint32_t>(measurement->Geometry);
        out_codes[2] = static_cast<std::uint32_t>(measurement->IlluminantType);
        out_values[0] = measurement->Backing.X;
        out_values[1] = measurement->Backing.Y;
        out_values[2] = measurement->Backing.Z;
        out_values[3] = measurement->Flare;
    }

    const auto* viewing = static_cast<const cmsICCViewingConditions*>(
        cmsReadTag(profile->handle, cmsSigViewingConditionsTag));
    if (viewing) {
        out_present[1] = 1;
        out_codes[3] = static_cast<std::uint32_t>(viewing->IlluminantType);
        out_values[4] = viewing->IlluminantXYZ.X;
        out_values[5] = viewing->IlluminantXYZ.Y;
        out_values[6] = viewing->IlluminantXYZ.Z;
        out_values[7] = viewing->SurroundXYZ.X;
        out_values[8] = viewing->SurroundXYZ.Y;
        out_values[9] = viewing->SurroundXYZ.Z;
    }

    const auto* description = static_cast<const cmsMLU*>(
        cmsReadTag(profile->handle, cmsSigViewingCondDescTag));
    if (!description) {
        if (out_viewing_description && viewing_description_size > 0u) {
            out_viewing_description[0] = '\0';
        }
        return PILLOW_C_OK;
    }

    const cmsUInt32Number description_required = cmsMLUgetUTF8(
        description,
        cmsNoLanguage,
        cmsNoCountry,
        nullptr,
        0);
    if (description_required == 0u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    out_present[2] = 1;
    *out_viewing_description_required = static_cast<std::size_t>(
        description_required);
    if (!out_viewing_description) {
        return PILLOW_C_OK;
    }
    if (viewing_description_size < description_required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (cmsMLUgetUTF8(
            description,
            cmsNoLanguage,
            cmsNoCountry,
            out_viewing_description,
            description_required) != description_required) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int
pillow_c_cms_profile_attributes_and_colorimetric_intent(
    const PillowCCmsProfile* profile,
    std::uint64_t* out_attributes,
    int* out_colorimetric_intent_present,
    std::uint32_t* out_colorimetric_intent)
{
    if (!profile || !profile->handle || !out_attributes ||
        !out_colorimetric_intent_present || !out_colorimetric_intent) {
        return PILLOW_C_NULL_POINTER;
    }

    *out_attributes = 0u;
    *out_colorimetric_intent_present = 0;
    *out_colorimetric_intent = 0u;

    cmsUInt64Number attributes = 0u;
    cmsGetHeaderAttributes(profile->handle, &attributes);
    *out_attributes = static_cast<std::uint64_t>(attributes);

    const auto* colorimetric_intent = static_cast<const cmsUInt32Number*>(
        cmsReadTag(profile->handle, cmsSigColorimetricIntentImageStateTag));
    if (colorimetric_intent) {
        *out_colorimetric_intent_present = 1;
        *out_colorimetric_intent = static_cast<std::uint32_t>(
            *colorimetric_intent);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_colorant_tables(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    std::uint32_t* out_counts,
    std::size_t count_count,
    char* out_table,
    std::size_t table_size,
    std::size_t* out_table_required,
    char* out_table_out,
    std::size_t table_out_size,
    std::size_t* out_table_out_required)
{
    if (!profile || !profile->handle || !out_present || !out_counts ||
        !out_table_required || !out_table_out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 2u || count_count != 2u) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((!out_table && table_size != 0u) ||
        (!out_table_out && table_out_size != 0u)) {
        return PILLOW_C_NULL_POINTER;
    }

    std::fill(out_present, out_present + present_count, 0);
    std::fill(out_counts, out_counts + count_count, std::uint32_t{0});
    *out_table_required = 0u;
    *out_table_out_required = 0u;

    const cmsTagSignature tags[] = {
        cmsSigColorantTableTag,
        cmsSigColorantTableOutTag
    };
    const cmsNAMEDCOLORLIST* tables[2] = {nullptr, nullptr};
    std::size_t required[2] = {0u, 0u};
    for (std::size_t index = 0; index < 2u; ++index) {
        tables[index] = static_cast<const cmsNAMEDCOLORLIST*>(
            cmsReadTag(profile->handle, tags[index]));
        if (!tables[index]) {
            continue;
        }

        const cmsUInt32Number count = cmsNamedColorCount(tables[index]);
        if (count > std::numeric_limits<std::size_t>::max() / cmsMAX_PATH) {
            return PILLOW_C_INVALID_LENGTH;
        }
        out_present[index] = 1;
        out_counts[index] = static_cast<std::uint32_t>(count);
        required[index] = static_cast<std::size_t>(count) * cmsMAX_PATH;
    }

    *out_table_required = required[0];
    *out_table_out_required = required[1];
    char* outputs[] = {out_table, out_table_out};
    const std::size_t output_sizes[] = {table_size, table_out_size};
    for (std::size_t index = 0; index < 2u; ++index) {
        if (outputs[index] && output_sizes[index] < required[index]) {
            return PILLOW_C_INVALID_LENGTH;
        }
    }
    for (std::size_t index = 0; index < 2u; ++index) {
        if (!outputs[index] || !tables[index] || required[index] == 0u) {
            continue;
        }

        std::memset(outputs[index], 0, required[index]);
        for (cmsUInt32Number color = 0; color < out_counts[index]; ++color) {
            char* name = outputs[index] +
                static_cast<std::size_t>(color) * cmsMAX_PATH;
            if (!cmsNamedColorInfo(
                    tables[index], color, name, nullptr, nullptr, nullptr,
                    nullptr)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_clut(
    const PillowCCmsProfile* profile,
    int* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (value_count != 12u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_values, out_values + value_count, 0);
    for (cmsUInt32Number intent = 0u; intent < 4u; ++intent) {
        for (cmsUInt32Number direction = LCMS_USED_AS_INPUT;
             direction <= LCMS_USED_AS_PROOF;
             ++direction) {
            const std::size_t index = static_cast<std::size_t>(intent) * 3u +
                direction;
            out_values[index] = cmsIsCLUT(
                profile->handle, intent, direction) ? 1 : 0;
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_intent_support(
    const PillowCCmsProfile* profile,
    int* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (value_count != 12u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_values, out_values + value_count, 0);
    for (cmsUInt32Number intent = 0u; intent < 4u; ++intent) {
        for (cmsUInt32Number direction = LCMS_USED_AS_INPUT;
             direction <= LCMS_USED_AS_PROOF;
             ++direction) {
            const std::size_t index = static_cast<std::size_t>(intent) * 3u +
                direction;
            out_values[index] = cmsIsIntentSupported(
                profile->handle, intent, direction) ? 1 : 0;
        }
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_rgb_colorants(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    double* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_present || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 3u || value_count != 18u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_present, out_present + present_count, 0);
    std::fill(out_values, out_values + value_count, 0.0);
    const cmsTagSignature tags[] = {
        cmsSigRedColorantTag,
        cmsSigGreenColorantTag,
        cmsSigBlueColorantTag
    };
    for (std::size_t index = 0; index < 3u; ++index) {
        const auto* colorant = static_cast<const cmsCIEXYZ*>(
            cmsReadTag(profile->handle, tags[index]));
        if (!colorant) {
            continue;
        }

        cmsCIExyY chromaticity{};
        cmsXYZ2xyY(&chromaticity, colorant);
        const std::size_t offset = index * 6u;
        out_present[index] = 1;
        out_values[offset] = colorant->X;
        out_values[offset + 1u] = colorant->Y;
        out_values[offset + 2u] = colorant->Z;
        out_values[offset + 3u] = chromaticity.x;
        out_values[offset + 4u] = chromaticity.y;
        out_values[offset + 5u] = chromaticity.Y;
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_rgb_primaries(
    const PillowCCmsProfile* profile,
    int* out_present,
    std::size_t present_count,
    double* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_present || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (present_count != 3u || value_count != 18u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    std::fill(out_present, out_present + present_count, 0);
    std::fill(out_values, out_values + value_count, 0.0);
    if (!cmsIsMatrixShaper(profile->handle)) {
        return PILLOW_C_OK;
    }

    cmsHPROFILE xyz_profile = cmsCreateXYZProfile();
    if (!xyz_profile) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    cmsHTRANSFORM transform = cmsCreateTransform(
        profile->handle,
        TYPE_RGB_DBL,
        xyz_profile,
        TYPE_XYZ_DBL,
        INTENT_RELATIVE_COLORIMETRIC,
        cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE);
    cmsCloseProfile(xyz_profile);
    if (!transform) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const double input[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };
    cmsCIEXYZTRIPLE primaries{};
    cmsDoTransform(transform, input, &primaries, 3u);
    cmsDeleteTransform(transform);

    const cmsCIEXYZ* xyz_values[] = {
        &primaries.Red,
        &primaries.Green,
        &primaries.Blue
    };
    for (std::size_t index = 0; index < 3u; ++index) {
        cmsCIExyY chromaticity{};
        cmsXYZ2xyY(&chromaticity, xyz_values[index]);
        const std::size_t offset = index * 6u;
        out_present[index] = 1;
        out_values[offset] = xyz_values[index]->X;
        out_values[offset + 1u] = xyz_values[index]->Y;
        out_values[offset + 2u] = xyz_values[index]->Z;
        out_values[offset + 3u] = chromaticity.x;
        out_values[offset + 4u] = chromaticity.y;
        out_values[offset + 5u] = chromaticity.Y;
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_chromatic_adaptation(
    const PillowCCmsProfile* profile,
    int* out_present,
    double* out_values,
    std::size_t value_count)
{
    if (!profile || !profile->handle || !out_present || !out_values) {
        return PILLOW_C_NULL_POINTER;
    }
    if (value_count != 18u) {
        return PILLOW_C_INVALID_LENGTH;
    }

    *out_present = 0;
    std::fill(out_values, out_values + value_count, 0.0);
    const auto* matrix = static_cast<const cmsFloat64Number*>(
        cmsReadTag(profile->handle, cmsSigChromaticAdaptationTag));
    if (!matrix) {
        return PILLOW_C_OK;
    }

    for (std::size_t row = 0; row < 3u; ++row) {
        const std::size_t offset = row * 3u;
        cmsCIEXYZ xyz{
            matrix[offset],
            matrix[offset + 1u],
            matrix[offset + 2u]
        };
        cmsCIExyY chromaticity{};
        cmsXYZ2xyY(&chromaticity, &xyz);
        out_values[offset] = xyz.X;
        out_values[offset + 1u] = xyz.Y;
        out_values[offset + 2u] = xyz.Z;
        out_values[9u + offset] = chromaticity.x;
        out_values[9u + offset + 1u] = chromaticity.y;
        out_values[9u + offset + 2u] = chromaticity.Y;
    }
    *out_present = 1;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_intent_supported(
    const PillowCCmsProfile* profile,
    int intent,
    int direction,
    int* out_supported)
{
    if (!profile || !profile->handle || !out_supported) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_supported = 0;
    if (intent < INTENT_PERCEPTUAL || intent > INTENT_ABSOLUTE_COLORIMETRIC ||
        direction < LCMS_USED_AS_INPUT || direction > LCMS_USED_AS_PROOF) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_supported = cmsIsIntentSupported(
        profile->handle,
        static_cast<cmsUInt32Number>(intent),
        static_cast<cmsUInt32Number>(direction)) ? 1 : 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_open_file(
    const char* path,
    PillowCCmsProfile** out_profile)
{
    if (!path || !out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_profile = nullptr;
    cmsHPROFILE handle = cmsOpenProfileFromFile(path, "r");
    if (!handle) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return own_cms_profile(handle, out_profile);
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_open_file_wide(
    const wchar_t* path,
    PillowCCmsProfile** out_profile)
{
    if (!path || !out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_profile = nullptr;

    std::FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"rb") != 0 || !file) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const __int64 file_size = _ftelli64(file);
    if (file_size <= 0 ||
        static_cast<unsigned __int64>(file_size) >
            static_cast<unsigned __int64>(std::numeric_limits<cmsUInt32Number>::max())) {
        std::fclose(file);
        return PILLOW_C_INVALID_LENGTH;
    }
    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<std::uint8_t> data;
    try {
        data.resize(static_cast<std::size_t>(file_size));
    } catch (const std::bad_alloc&) {
        std::fclose(file);
        return PILLOW_C_ALLOCATION_FAILED;
    }
    const std::size_t read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    cmsHPROFILE handle = cmsOpenProfileFromMem(
        data.data(),
        static_cast<cmsUInt32Number>(data.size()));
    if (!handle) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return own_cms_profile(handle, out_profile);
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_open_memory(
    const std::uint8_t* data,
    std::size_t size,
    PillowCCmsProfile** out_profile)
{
    if (!data || !out_profile) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_profile = nullptr;
    if (size == 0 ||
        size > static_cast<std::size_t>(std::numeric_limits<cmsUInt32Number>::max())) {
        return PILLOW_C_INVALID_LENGTH;
    }
    cmsHPROFILE handle = cmsOpenProfileFromMem(
        data,
        static_cast<cmsUInt32Number>(size));
    if (!handle) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return own_cms_profile(handle, out_profile);
}

extern "C" __declspec(dllexport) int pillow_c_cms_transform_build(
    const PillowCCmsProfile* input_profile,
    const PillowCCmsProfile* output_profile,
    int input_mode,
    int output_mode,
    int rendering_intent,
    unsigned int flags,
    PillowCCmsTransform** out_transform)
{
    if (!input_profile || !input_profile->handle ||
        !output_profile || !output_profile->handle || !out_transform) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_transform = nullptr;
    const cmsColorSpaceSignature input_space = cmsGetColorSpace(input_profile->handle);
    const cmsColorSpaceSignature output_space = cmsGetColorSpace(output_profile->handle);
    const bool rgb_to_lab =
        input_mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigLabData;
    const bool lab_to_rgb =
        input_mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigRgbData;
    const bool rgb_to_rgb =
        input_mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigRgbData;
    const bool lab_to_lab =
        input_mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigLabData;
    const bool supported_intent =
        rendering_intent == INTENT_PERCEPTUAL ||
        rendering_intent == INTENT_RELATIVE_COLORIMETRIC ||
        ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_SATURATION) ||
        ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC);
    const bool supported_flags =
        flags == 0u ||
        (flags == cmsFLAGS_BLACKPOINTCOMPENSATION &&
         (rendering_intent == INTENT_PERCEPTUAL ||
          ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_RELATIVE_COLORIMETRIC) ||
          ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_SATURATION) ||
          ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC)));
    if ((!rgb_to_lab && !lab_to_rgb && !rgb_to_rgb && !lab_to_lab) ||
        !supported_intent ||
        !supported_flags) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    cmsHTRANSFORM handle = cmsCreateTransform(
        input_profile->handle,
        (rgb_to_lab || rgb_to_rgb) ? TYPE_RGB_8 : TYPE_Lab_8,
        output_profile->handle,
        (rgb_to_lab || lab_to_lab) ? TYPE_Lab_8 : TYPE_RGB_8,
        rendering_intent,
        flags);
    if (!handle) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    return own_cms_transform(
        handle,
        input_mode,
        output_mode,
        output_profile->handle,
        out_transform);
}

extern "C" __declspec(dllexport) int pillow_c_cms_proof_transform_build(
    const PillowCCmsProfile* input_profile,
    const PillowCCmsProfile* output_profile,
    const PillowCCmsProfile* proof_profile,
    int input_mode,
    int output_mode,
    int rendering_intent,
    int proof_rendering_intent,
    unsigned int flags,
    PillowCCmsTransform** out_transform)
{
    if (!input_profile || !input_profile->handle ||
        !output_profile || !output_profile->handle ||
        !proof_profile || !proof_profile->handle || !out_transform) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_transform = nullptr;
    const cmsColorSpaceSignature input_space = cmsGetColorSpace(input_profile->handle);
    const cmsColorSpaceSignature output_space = cmsGetColorSpace(output_profile->handle);
    const cmsColorSpaceSignature proof_space = cmsGetColorSpace(proof_profile->handle);
    const bool rgb_to_rgb =
        input_mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigRgbData &&
        proof_space == cmsSigRgbData;
    const bool rgb_to_lab =
        input_mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigLabData &&
        proof_space == cmsSigRgbData;
    const bool lab_to_rgb =
        input_mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigRgbData &&
        proof_space == cmsSigRgbData;
    const bool lab_to_lab =
        input_mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigLabData &&
        proof_space == cmsSigRgbData;
    const bool supported_rendering_intent =
        rendering_intent == INTENT_PERCEPTUAL ||
        ((rgb_to_rgb || rgb_to_lab || lab_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_RELATIVE_COLORIMETRIC) ||
        ((rgb_to_rgb || rgb_to_lab || lab_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_SATURATION) ||
        ((rgb_to_rgb || rgb_to_lab || lab_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC);
    const bool supported_flags =
        flags == cmsFLAGS_SOFTPROOFING ||
        ((((lab_to_rgb || lab_to_lab) &&
           (rendering_intent == INTENT_PERCEPTUAL ||
             rendering_intent == INTENT_RELATIVE_COLORIMETRIC ||
             rendering_intent == INTENT_SATURATION ||
             rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC)) ||
          (rgb_to_lab &&
           (rendering_intent == INTENT_PERCEPTUAL ||
            rendering_intent == INTENT_RELATIVE_COLORIMETRIC ||
            rendering_intent == INTENT_SATURATION ||
            rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC)) ||
          (rgb_to_rgb &&
            (rendering_intent == INTENT_PERCEPTUAL ||
             rendering_intent == INTENT_RELATIVE_COLORIMETRIC ||
             rendering_intent == INTENT_SATURATION ||
             rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC))) &&
         flags == (cmsFLAGS_SOFTPROOFING | cmsFLAGS_GAMUTCHECK));
    const bool supported_defaults =
        supported_rendering_intent &&
        proof_rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC &&
        supported_flags;
    if ((!rgb_to_rgb && !rgb_to_lab && !lab_to_rgb && !lab_to_lab) ||
        !supported_defaults) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    cmsHTRANSFORM handle = cmsCreateProofingTransform(
        input_profile->handle,
        (lab_to_rgb || lab_to_lab) ? TYPE_Lab_8 : TYPE_RGB_8,
        output_profile->handle,
        (rgb_to_lab || lab_to_lab) ? TYPE_Lab_8 : TYPE_RGB_8,
        proof_profile->handle,
        rendering_intent,
        proof_rendering_intent,
        flags);
    if (!handle) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return own_cms_transform(
        handle,
        input_mode,
        output_mode,
        output_profile->handle,
        out_transform);
}

extern "C" __declspec(dllexport) int pillow_c_cms_transform_apply(
    const PillowCCmsTransform* transform,
    const PillowCImage* source,
    PillowCImage** out_image)
{
    if (!transform || !transform->handle || !source || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const bool rgb_to_lab =
        transform->input_mode == PILLOW_C_MODE_RGB &&
        transform->output_mode == PILLOW_C_MODE_LAB;
    const bool lab_to_rgb =
        transform->input_mode == PILLOW_C_MODE_LAB &&
        transform->output_mode == PILLOW_C_MODE_RGB;
    const bool rgb_to_rgb =
        transform->input_mode == PILLOW_C_MODE_RGB &&
        transform->output_mode == PILLOW_C_MODE_RGB;
    const bool lab_to_lab =
        transform->input_mode == PILLOW_C_MODE_LAB &&
        transform->output_mode == PILLOW_C_MODE_LAB;
    if (source->mode != transform->input_mode ||
        (!rgb_to_lab && !lab_to_rgb && !rgb_to_rgb && !lab_to_lab)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(
            source->width,
            source->height,
            3,
            &stride,
            &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        auto* result = new PillowCImage{
            source->width,
            source->height,
            transform->output_mode,
            3,
            stride,
            std::vector<std::uint8_t>(size)};
        for (int y = 0; y < source->height; ++y) {
            cmsDoTransform(
                transform->handle,
                source->pixels.data() + static_cast<std::size_t>(y) * source->stride,
                result->pixels.data() + static_cast<std::size_t>(y) * result->stride,
                static_cast<cmsUInt32Number>(source->width));
        }
        *out_image = result;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_transform_apply_in_place(
    const PillowCCmsTransform* transform,
    PillowCImage* image)
{
    if (!transform || !transform->handle || !image) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool rgb_to_rgb =
        transform->input_mode == PILLOW_C_MODE_RGB &&
        transform->output_mode == PILLOW_C_MODE_RGB;
    const bool lab_to_lab =
        transform->input_mode == PILLOW_C_MODE_LAB &&
        transform->output_mode == PILLOW_C_MODE_LAB;
    if ((!rgb_to_rgb && !lab_to_lab) || image->mode != transform->input_mode) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int detach_status = pillow_c_detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        return detach_status;
    }
        for (int y = 0; y < image->height; ++y) {
            std::uint8_t* row =
                image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
            cmsDoTransform(
                transform->handle,
                row,
                row,
                static_cast<cmsUInt32Number>(image->width));
        }
        return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_transform_output_profile_bytes(
    const PillowCCmsTransform* transform,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!transform || !transform->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t required = transform->output_profile_bytes.size();
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (required > 0) {
        std::memcpy(out, transform->output_profile_bytes.data(), required);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_transform_free(
    PillowCCmsTransform* transform)
{
    if (!transform) {
        return PILLOW_C_NULL_POINTER;
    }
    cmsDeleteTransform(transform->handle);
    delete transform;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_to_profile_image(
    const PillowCImage* source,
    const PillowCCmsProfile* input_profile,
    const PillowCCmsProfile* output_profile,
    int output_mode,
    int rendering_intent,
    unsigned int flags,
    PillowCImage** out_image)
{
    if (!source || !input_profile || !input_profile->handle ||
        !output_profile || !output_profile->handle || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    const cmsColorSpaceSignature input_space = cmsGetColorSpace(input_profile->handle);
    const cmsColorSpaceSignature output_space = cmsGetColorSpace(output_profile->handle);
    const bool rgb_to_lab =
        source->mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigLabData;
    const bool lab_to_rgb =
        source->mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigRgbData;
    const bool rgb_to_rgb =
        source->mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigRgbData;
    const bool lab_to_lab =
        source->mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigLabData;
    const bool supported_intent =
        rendering_intent == INTENT_PERCEPTUAL ||
        ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_RELATIVE_COLORIMETRIC) ||
        ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_SATURATION) ||
        ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC);
    const bool supported_flags =
        flags == 0u ||
        (flags == cmsFLAGS_BLACKPOINTCOMPENSATION &&
         (rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
         (rendering_intent == INTENT_PERCEPTUAL ||
          ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_RELATIVE_COLORIMETRIC) ||
          ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_SATURATION) ||
          ((rgb_to_lab || lab_to_rgb || rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC)));
    if ((!rgb_to_lab && !lab_to_rgb && !rgb_to_rgb && !lab_to_lab) ||
        !supported_intent ||
        !supported_flags) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int refresh_status = pillow_c_refresh_const_buffer_view_image(source);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }

    cmsHTRANSFORM transform = cmsCreateTransform(
        input_profile->handle,
        (rgb_to_lab || rgb_to_rgb) ? TYPE_RGB_8 : TYPE_Lab_8,
        output_profile->handle,
        (rgb_to_lab || lab_to_lab) ? TYPE_Lab_8 : TYPE_RGB_8,
        rendering_intent,
        flags);
    if (!transform) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(
            source->width,
            source->height,
            3,
            &stride,
            &size)) {
        cmsDeleteTransform(transform);
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        auto* result = new PillowCImage{
            source->width,
            source->height,
            output_mode,
            3,
            stride,
            std::vector<std::uint8_t>(size)};
        for (int y = 0; y < source->height; ++y) {
            cmsDoTransform(
                transform,
                source->pixels.data() + static_cast<std::size_t>(y) * source->stride,
                result->pixels.data() + static_cast<std::size_t>(y) * result->stride,
                static_cast<cmsUInt32Number>(source->width));
        }
        cmsDeleteTransform(transform);
        *out_image = result;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        cmsDeleteTransform(transform);
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_to_profile_image_in_place(
    PillowCImage* image,
    const PillowCCmsProfile* input_profile,
    const PillowCCmsProfile* output_profile,
    int output_mode,
    int rendering_intent,
    unsigned int flags)
{
    if (!image || !input_profile || !input_profile->handle ||
        !output_profile || !output_profile->handle) {
        return PILLOW_C_NULL_POINTER;
    }
    const cmsColorSpaceSignature input_space = cmsGetColorSpace(input_profile->handle);
    const cmsColorSpaceSignature output_space = cmsGetColorSpace(output_profile->handle);
    const bool rgb_to_rgb =
        image->mode == PILLOW_C_MODE_RGB &&
        output_mode == PILLOW_C_MODE_RGB &&
        input_space == cmsSigRgbData &&
        output_space == cmsSigRgbData;
    const bool lab_to_lab =
        image->mode == PILLOW_C_MODE_LAB &&
        output_mode == PILLOW_C_MODE_LAB &&
        input_space == cmsSigLabData &&
        output_space == cmsSigLabData;
    const bool supported_intent =
        rendering_intent == INTENT_PERCEPTUAL ||
        ((rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_RELATIVE_COLORIMETRIC) ||
        ((rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_SATURATION) ||
        ((rgb_to_rgb || lab_to_lab) &&
         rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC);
    const bool supported_flags =
        flags == 0u ||
        (flags == cmsFLAGS_BLACKPOINTCOMPENSATION &&
         (((rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_PERCEPTUAL) ||
          ((rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_RELATIVE_COLORIMETRIC) ||
          ((rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_SATURATION) ||
          ((rgb_to_rgb || lab_to_lab) &&
           rendering_intent == INTENT_ABSOLUTE_COLORIMETRIC)));
    if ((!rgb_to_rgb && !lab_to_lab) ||
        !supported_intent ||
        !supported_flags) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const cmsUInt32Number pixel_format = rgb_to_rgb ? TYPE_RGB_8 : TYPE_Lab_8;
    cmsHTRANSFORM transform = cmsCreateTransform(
        input_profile->handle,
        pixel_format,
        output_profile->handle,
        pixel_format,
        rendering_intent,
        flags);
    if (!transform) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int detach_status = pillow_c_detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        cmsDeleteTransform(transform);
        return detach_status;
    }
    for (int y = 0; y < image->height; ++y) {
        std::uint8_t* row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        cmsDoTransform(
            transform,
            row,
            row,
            static_cast<cmsUInt32Number>(image->width));
    }
    cmsDeleteTransform(transform);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_name(
    const PillowCCmsProfile* profile,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!profile || !profile->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;

    const cmsUInt32Number description_size = cmsGetProfileInfoASCII(
        profile->handle,
        cmsInfoDescription,
        "en",
        "US",
        nullptr,
        0);
    if (description_size == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<char> description(description_size, '\0');
        if (cmsGetProfileInfoASCII(
                profile->handle,
                cmsInfoDescription,
                "en",
                "US",
                description.data(),
                description_size) == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::string name = std::string(description.data()) + '\n';
        const std::size_t required = name.size() + 1u;
        *out_required = required;
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, name.c_str(), required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_info(
    const PillowCCmsProfile* profile,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!profile || !profile->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;

    try {
        std::string info;
        const cmsInfoType fields[] = {cmsInfoDescription, cmsInfoCopyright};
        for (const cmsInfoType field : fields) {
            const cmsUInt32Number field_size = cmsGetProfileInfoASCII(
                profile->handle,
                field,
                "en",
                "US",
                nullptr,
                0);
            if (field_size == 0) {
                continue;
            }
            std::vector<char> field_value(field_size, '\0');
            if (cmsGetProfileInfoASCII(
                    profile->handle,
                    field,
                    "en",
                    "US",
                    field_value.data(),
                    field_size) == 0) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            if (field_value[0] == '\0') {
                continue;
            }
            if (!info.empty()) {
                info += "\r\n\r\n";
            }
            info += field_value.data();
        }
        info += "\r\n\r\n";

        const std::size_t required = info.size() + 1u;
        *out_required = required;
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, info.c_str(), required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_copyright(
    const PillowCCmsProfile* profile,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!profile || !profile->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;

    const cmsUInt32Number copyright_size = cmsGetProfileInfoASCII(
        profile->handle,
        cmsInfoCopyright,
        "en",
        "US",
        nullptr,
        0);
    if (copyright_size == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        std::vector<char> copyright_value(copyright_size, '\0');
        if (cmsGetProfileInfoASCII(
                profile->handle,
                cmsInfoCopyright,
                "en",
                "US",
                copyright_value.data(),
                copyright_size) == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::string copyright = std::string(copyright_value.data()) + '\n';
        const std::size_t required = copyright.size() + 1u;
        *out_required = required;
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, copyright.c_str(), required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_manufacturer(
    const PillowCCmsProfile* profile,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!profile || !profile->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;

    const cmsUInt32Number manufacturer_size = cmsGetProfileInfoASCII(
        profile->handle,
        cmsInfoManufacturer,
        "en",
        "US",
        nullptr,
        0);
    if (manufacturer_size == 0) {
        constexpr char missing_manufacturer[] = "\n";
        *out_required = sizeof(missing_manufacturer);
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < sizeof(missing_manufacturer)) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, missing_manufacturer, sizeof(missing_manufacturer));
        return PILLOW_C_OK;
    }

    try {
        std::vector<char> manufacturer_value(manufacturer_size, '\0');
        if (cmsGetProfileInfoASCII(
                profile->handle,
                cmsInfoManufacturer,
                "en",
                "US",
                manufacturer_value.data(),
                manufacturer_size) == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::string manufacturer = std::string(manufacturer_value.data()) + '\n';
        const std::size_t required = manufacturer.size() + 1u;
        *out_required = required;
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, manufacturer.c_str(), required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_model(
    const PillowCCmsProfile* profile,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!profile || !profile->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;

    const cmsUInt32Number model_size = cmsGetProfileInfoASCII(
        profile->handle,
        cmsInfoModel,
        "en",
        "US",
        nullptr,
        0);
    if (model_size == 0) {
        constexpr char missing_model[] = "\n";
        *out_required = sizeof(missing_model);
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < sizeof(missing_model)) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, missing_model, sizeof(missing_model));
        return PILLOW_C_OK;
    }

    try {
        std::vector<char> model_value(model_size, '\0');
        if (cmsGetProfileInfoASCII(
                profile->handle,
                cmsInfoModel,
                "en",
                "US",
                model_value.data(),
                model_size) == 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::string model = std::string(model_value.data()) + '\n';
        const std::size_t required = model.size() + 1u;
        *out_required = required;
        if (!out) {
            return PILLOW_C_OK;
        }
        if (out_size < required) {
            return PILLOW_C_INVALID_LENGTH;
        }
        std::memcpy(out, model.c_str(), required);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_bytes(
    const PillowCCmsProfile* profile,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!profile || !profile->handle || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_required = 0;
    cmsUInt32Number required = 0;
    if (!cmsSaveProfileToMem(profile->handle, nullptr, &required) || required == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_required = static_cast<std::size_t>(required);
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < static_cast<std::size_t>(required)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    cmsUInt32Number written = required;
    if (!cmsSaveProfileToMem(profile->handle, out, &written) || written != required) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_free(
    PillowCCmsProfile* profile)
{
    if (!profile || !profile->handle) {
        return PILLOW_C_NULL_POINTER;
    }
    if (profile->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        cmsCloseProfile(profile->handle);
        profile->handle = nullptr;
        delete profile;
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_cms_profile_retain(
    PillowCCmsProfile* profile)
{
    if (!profile || !profile->handle) {
        return PILLOW_C_NULL_POINTER;
    }
    profile->references.fetch_add(1, std::memory_order_relaxed);
    return PILLOW_C_OK;
}
