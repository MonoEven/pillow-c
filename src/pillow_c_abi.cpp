#include "pillow_c_internal.h"

#include <climits>
#include <cstring>

namespace {

const char* status_message(int status)
{
    switch (status) {
    case PILLOW_C_OK:
        return "ok";
    case PILLOW_C_NULL_POINTER:
        return "null pointer";
    case PILLOW_C_INVALID_LENGTH:
        return "invalid length";
    case PILLOW_C_INVALID_ARGUMENT:
        return "invalid argument";
    case PILLOW_C_ALLOCATION_FAILED:
        return "allocation failed";
    case PILLOW_C_MISMATCH:
        return "mismatch";
    default:
        return "unknown status";
    }
}

}

extern "C" __declspec(dllexport) int pillow_c_abi_version(
    int* out_major,
    int* out_minor,
    int* out_patch)
{
    if (!out_major || !out_minor || !out_patch) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_major = 0;
    *out_minor = 1;
    *out_patch = 0;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_status_message(
    int status,
    char* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!out_required) {
        return PILLOW_C_NULL_POINTER;
    }

    const char* message = status_message(status);
    const std::size_t required = std::strlen(message) + 1;
    *out_required = required;
    if (!out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out, message, required);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_width(const PillowCImage* image, int* out_width)
{
    if (!image || !out_width) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_width = image->width;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_height(const PillowCImage* image, int* out_height)
{
    if (!image || !out_height) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_height = image->height;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_channels(const PillowCImage* image, int* out_channels)
{
    if (!image || !out_channels) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_channels = image->channels;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_mode(const PillowCImage* image, int* out_mode)
{
    if (!image || !out_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_mode = image->mode;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_exif_orientation(const PillowCImage* image, int* out_orientation)
{
    if (!image || !out_orientation) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_orientation = image->exif_orientation;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_stride(const PillowCImage* image, int* out_stride)
{
    if (!image || !out_stride) {
        return PILLOW_C_NULL_POINTER;
    }
    if (image->stride > static_cast<std::size_t>(INT32_MAX)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    *out_stride = static_cast<int>(image->stride);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_size(const PillowCImage* image, std::size_t* out_size)
{
    if (!image || !out_size) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_size = image->pixels.size();
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_data(
    PillowCImage* image,
    std::uint8_t** out_data,
    std::size_t* out_size)
{
    if (!image || !out_data || !out_size) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_data = image->pixels.data();
    *out_size = image->pixels.size();
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_readonly(
    const PillowCImage* image,
    int* out_readonly)
{
    if (!image || !out_readonly) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_readonly = image->buffer_readonly ? 1 : 0;
    return PILLOW_C_OK;
}

