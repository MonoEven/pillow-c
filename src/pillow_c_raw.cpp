#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "pillow_c_internal.h"

namespace {
enum class RawCodecKind {
    Unsupported,
    One,
    P,
    PA,
    L,
    LA,
    RGB,
    RGBX,
    BGR,
    BGRX,
    XBGR,
    RGBA,
    BGRA,
    ARGB,
    ABGR,
    I,
    I8,
    I32Big,
    I32Native,
    I16Little,
    I16Big,
    I16Native,
    I16SignedLittle,
    F,
    F32Big,
    F32Native,
    F64Little,
    One8,
    CMYK,
    YCbCr,
    HSV,
    LAB,
};

struct RawCodecSpec {
    RawCodecKind kind;
    int bytes_per_pixel;
    bool planar = false;
};

inline bool native_is_little_endian()
{
    const std::uint16_t value = 1;
    return *reinterpret_cast<const std::uint8_t*>(&value) == 1;
}

inline std::uint16_t read_u16_le(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

inline std::uint16_t read_u16_be(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[0]) << 8) |
           static_cast<std::uint16_t>(data[1]);
}

inline void write_i32_le(std::uint8_t* data, std::uint32_t value)
{
    data[0] = static_cast<std::uint8_t>(value & 0xFFu);
    data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    data[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    data[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

inline std::int32_t read_i32_le(const std::uint8_t* data)
{
    const std::uint32_t value =
        static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    return static_cast<std::int32_t>(value);
}

inline float read_f32_le(const std::uint8_t* data)
{
    const std::uint32_t bits =
        static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline double read_f64_le(const std::uint8_t* data)
{
    const std::uint64_t bits =
        static_cast<std::uint64_t>(data[0]) |
        (static_cast<std::uint64_t>(data[1]) << 8) |
        (static_cast<std::uint64_t>(data[2]) << 16) |
        (static_cast<std::uint64_t>(data[3]) << 24) |
        (static_cast<std::uint64_t>(data[4]) << 32) |
        (static_cast<std::uint64_t>(data[5]) << 40) |
        (static_cast<std::uint64_t>(data[6]) << 48) |
        (static_cast<std::uint64_t>(data[7]) << 56);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline void write_f32_le(std::uint8_t* data, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_i32_le(data, bits);
}

inline std::uint16_t clip_i32_to_u16(std::int32_t value)
{
    if (value <= 0) {
        return 0;
    }
    if (value >= 65535) {
        return 65535;
    }
    return static_cast<std::uint16_t>(value);
}

RawCodecSpec raw_decode_spec(int target_mode, const char* raw_mode)
{
    if (!raw_mode) {
        return {RawCodecKind::Unsupported, 0};
    }

    switch (target_mode) {
    case PILLOW_C_MODE_1:
        if (std::strcmp(raw_mode, "1") == 0) {
            return {RawCodecKind::One, 0};
        }
        if (std::strcmp(raw_mode, "1;8") == 0) {
            return {RawCodecKind::One8, 1};
        }
        break;
    case PILLOW_C_MODE_P:
        if (std::strcmp(raw_mode, "P") == 0) {
            return {RawCodecKind::P, 1};
        }
        break;
    case PILLOW_C_MODE_PA:
        if (std::strcmp(raw_mode, "PA") == 0) {
            return {RawCodecKind::PA, 2};
        }
        break;
    case PILLOW_C_MODE_L:
        if (std::strcmp(raw_mode, "L") == 0) {
            return {RawCodecKind::L, 1};
        }
        break;
    case PILLOW_C_MODE_LA:
        if (std::strcmp(raw_mode, "LA") == 0) {
            return {RawCodecKind::LA, 2};
        }
        if (std::strcmp(raw_mode, "LA;L") == 0) {
            return {RawCodecKind::LA, 2, true};
        }
        break;
    case PILLOW_C_MODE_RGB:
        if (std::strcmp(raw_mode, "RGB") == 0) {
            return {RawCodecKind::RGB, 3};
        }
        if (std::strcmp(raw_mode, "RGB;L") == 0) {
            return {RawCodecKind::RGB, 3, true};
        }
        if (std::strcmp(raw_mode, "RGBX") == 0) {
            return {RawCodecKind::RGBX, 4};
        }
        if (std::strcmp(raw_mode, "RGBX;L") == 0) {
            return {RawCodecKind::RGBX, 4, true};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        if (std::strcmp(raw_mode, "BGRX") == 0) {
            return {RawCodecKind::BGRX, 4};
        }
        if (std::strcmp(raw_mode, "XBGR") == 0) {
            return {RawCodecKind::XBGR, 4};
        }
        break;
    case PILLOW_C_MODE_RGBA:
        if (std::strcmp(raw_mode, "RGBA") == 0) {
            return {RawCodecKind::RGBA, 4};
        }
        if (std::strcmp(raw_mode, "RGBA;L") == 0) {
            return {RawCodecKind::RGBA, 4, true};
        }
        if (std::strcmp(raw_mode, "BGRA") == 0) {
            return {RawCodecKind::BGRA, 4};
        }
        if (std::strcmp(raw_mode, "ARGB") == 0) {
            return {RawCodecKind::ARGB, 4};
        }
        if (std::strcmp(raw_mode, "ABGR") == 0) {
            return {RawCodecKind::ABGR, 4};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        break;
    case PILLOW_C_MODE_RGBX:
        if (std::strcmp(raw_mode, "RGBX") == 0) {
            return {RawCodecKind::RGBX, 4};
        }
        if (std::strcmp(raw_mode, "RGBX;L") == 0) {
            return {RawCodecKind::RGBX, 4, true};
        }
        break;
    case PILLOW_C_MODE_CMYK:
        if (std::strcmp(raw_mode, "CMYK") == 0) {
            return {RawCodecKind::CMYK, 4};
        }
        if (std::strcmp(raw_mode, "CMYK;L") == 0) {
            return {RawCodecKind::CMYK, 4, true};
        }
        break;
    case PILLOW_C_MODE_YCBCR:
        if (std::strcmp(raw_mode, "YCbCr") == 0) {
            return {RawCodecKind::YCbCr, 3};
        }
        break;
    case PILLOW_C_MODE_HSV:
        if (std::strcmp(raw_mode, "HSV") == 0) {
            return {RawCodecKind::HSV, 3};
        }
        break;
    case PILLOW_C_MODE_LAB:
        if (std::strcmp(raw_mode, "LAB") == 0) {
            return {RawCodecKind::LAB, 3};
        }
        break;
    case PILLOW_C_MODE_I:
        if (std::strcmp(raw_mode, "I") == 0 || std::strcmp(raw_mode, "I;32") == 0 || std::strcmp(raw_mode, "I;32S") == 0) {
            return {RawCodecKind::I, 4};
        }
        if (std::strcmp(raw_mode, "I;32B") == 0) {
            return {RawCodecKind::I32Big, 4};
        }
        if (std::strcmp(raw_mode, "I;32N") == 0) {
            return {RawCodecKind::I32Native, 4};
        }
        if (std::strcmp(raw_mode, "I;8") == 0) {
            return {RawCodecKind::I8, 1};
        }
        if (std::strcmp(raw_mode, "I;16") == 0) {
            return {RawCodecKind::I16Little, 2};
        }
        if (std::strcmp(raw_mode, "I;16B") == 0) {
            return {RawCodecKind::I16Big, 2};
        }
        if (std::strcmp(raw_mode, "I;16N") == 0) {
            return {RawCodecKind::I16Native, 2};
        }
        if (std::strcmp(raw_mode, "I;16S") == 0) {
            return {RawCodecKind::I16SignedLittle, 2};
        }
        break;
    case PILLOW_C_MODE_F:
        if (std::strcmp(raw_mode, "F") == 0 || std::strcmp(raw_mode, "F;32F") == 0) {
            return {RawCodecKind::F, 4};
        }
        if (std::strcmp(raw_mode, "F;32BF") == 0) {
            return {RawCodecKind::F32Big, 4};
        }
        if (std::strcmp(raw_mode, "F;32NF") == 0) {
            return {RawCodecKind::F32Native, 4};
        }
        if (std::strcmp(raw_mode, "F;64F") == 0) {
            return {RawCodecKind::F64Little, 8};
        }
        break;
    case PILLOW_C_MODE_I16:
        if (std::strcmp(raw_mode, "I;16") == 0) {
            return {RawCodecKind::I16Little, 2};
        }
        break;
    case PILLOW_C_MODE_I16B:
        if (std::strcmp(raw_mode, "I;16B") == 0) {
            return {RawCodecKind::I16Big, 2};
        }
        break;
    default:
        break;
    }

    return {RawCodecKind::Unsupported, 0};
}

RawCodecSpec raw_encode_spec(int source_mode, const char* raw_mode)
{
    if (!raw_mode) {
        return {RawCodecKind::Unsupported, 0};
    }

    switch (source_mode) {
    case PILLOW_C_MODE_1:
        if (std::strcmp(raw_mode, "1") == 0) {
            return {RawCodecKind::One, 0};
        }
        break;
    case PILLOW_C_MODE_P:
        if (std::strcmp(raw_mode, "P") == 0) {
            return {RawCodecKind::P, 1};
        }
        break;
    case PILLOW_C_MODE_PA:
        if (std::strcmp(raw_mode, "PA") == 0) {
            return {RawCodecKind::PA, 2};
        }
        break;
    case PILLOW_C_MODE_L:
        if (std::strcmp(raw_mode, "L") == 0) {
            return {RawCodecKind::L, 1};
        }
        break;
    case PILLOW_C_MODE_LA:
        if (std::strcmp(raw_mode, "LA") == 0) {
            return {RawCodecKind::LA, 2};
        }
        if (std::strcmp(raw_mode, "LA;L") == 0) {
            return {RawCodecKind::LA, 2, true};
        }
        break;
    case PILLOW_C_MODE_RGB:
        if (std::strcmp(raw_mode, "RGB") == 0) {
            return {RawCodecKind::RGB, 3};
        }
        if (std::strcmp(raw_mode, "RGB;L") == 0) {
            return {RawCodecKind::RGB, 3, true};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        if (std::strcmp(raw_mode, "RGBX") == 0 || std::strcmp(raw_mode, "RGBA") == 0) {
            return {RawCodecKind::RGBX, 4};
        }
        if (std::strcmp(raw_mode, "RGBX;L") == 0) {
            return {RawCodecKind::RGBX, 4, true};
        }
        if (std::strcmp(raw_mode, "BGRX") == 0) {
            return {RawCodecKind::BGRX, 4};
        }
        if (std::strcmp(raw_mode, "XBGR") == 0) {
            return {RawCodecKind::XBGR, 4};
        }
        break;
    case PILLOW_C_MODE_RGBA:
        if (std::strcmp(raw_mode, "RGBA") == 0) {
            return {RawCodecKind::RGBA, 4};
        }
        if (std::strcmp(raw_mode, "RGBA;L") == 0) {
            return {RawCodecKind::RGBA, 4, true};
        }
        if (std::strcmp(raw_mode, "BGRA") == 0) {
            return {RawCodecKind::BGRA, 4};
        }
        if (std::strcmp(raw_mode, "ABGR") == 0) {
            return {RawCodecKind::ABGR, 4};
        }
        if (std::strcmp(raw_mode, "RGB") == 0) {
            return {RawCodecKind::RGB, 3};
        }
        if (std::strcmp(raw_mode, "BGR") == 0) {
            return {RawCodecKind::BGR, 3};
        }
        break;
    case PILLOW_C_MODE_RGBX:
        if (std::strcmp(raw_mode, "RGBX") == 0) {
            return {RawCodecKind::RGBX, 4};
        }
        if (std::strcmp(raw_mode, "RGBX;L") == 0) {
            return {RawCodecKind::RGBX, 4, true};
        }
        break;
    case PILLOW_C_MODE_CMYK:
        if (std::strcmp(raw_mode, "CMYK") == 0) {
            return {RawCodecKind::CMYK, 4};
        }
        if (std::strcmp(raw_mode, "CMYK;L") == 0) {
            return {RawCodecKind::CMYK, 4, true};
        }
        break;
    case PILLOW_C_MODE_YCBCR:
        if (std::strcmp(raw_mode, "YCbCr") == 0) {
            return {RawCodecKind::YCbCr, 3};
        }
        break;
    case PILLOW_C_MODE_HSV:
        if (std::strcmp(raw_mode, "HSV") == 0) {
            return {RawCodecKind::HSV, 3};
        }
        break;
    case PILLOW_C_MODE_LAB:
        if (std::strcmp(raw_mode, "LAB") == 0) {
            return {RawCodecKind::LAB, 3};
        }
        break;
    case PILLOW_C_MODE_I:
        if (std::strcmp(raw_mode, "I") == 0 || std::strcmp(raw_mode, "I;32S") == 0) {
            return {RawCodecKind::I, 4};
        }
        if (std::strcmp(raw_mode, "I;16B") == 0) {
            return {RawCodecKind::I16Big, 2};
        }
        break;
    case PILLOW_C_MODE_F:
        if (std::strcmp(raw_mode, "F") == 0 || std::strcmp(raw_mode, "F;32F") == 0) {
            return {RawCodecKind::F, 4};
        }
        if (std::strcmp(raw_mode, "F;32NF") == 0) {
            return {RawCodecKind::F32Native, 4};
        }
        break;
    case PILLOW_C_MODE_I16:
        if (std::strcmp(raw_mode, "I;16") == 0) {
            return {RawCodecKind::I16Little, 2};
        }
        break;
    case PILLOW_C_MODE_I16B:
        if (std::strcmp(raw_mode, "I;16B") == 0) {
            return {RawCodecKind::I16Big, 2};
        }
        break;
    default:
        break;
    }

    return {RawCodecKind::Unsupported, 0};
}

bool checked_raw_output_size(const PillowCImage* image, int bytes_per_pixel, std::size_t* out_size)
{
    if (!image || bytes_per_pixel <= 0 || !out_size) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height);
    if (bytes_per_pixel != 0 && pixels > static_cast<std::size_t>(-1) / static_cast<std::size_t>(bytes_per_pixel)) {
        return false;
    }
    *out_size = pixels * static_cast<std::size_t>(bytes_per_pixel);
    return true;
}

bool checked_mode1_raw_size(const PillowCImage* image, std::size_t* row_bytes, std::size_t* out_size)
{
    if (!image || !row_bytes || !out_size || image->width < 0 || image->height < 0) {
        return false;
    }
    const std::size_t row = (static_cast<std::size_t>(image->width) + 7u) / 8u;
    const std::size_t height = static_cast<std::size_t>(image->height);
    if (row != 0 && height > static_cast<std::size_t>(-1) / row) {
        return false;
    }
    *row_bytes = row;
    *out_size = row * height;
    return true;
}

void decode_raw_pixel(const RawCodecSpec& spec, const std::uint8_t* src, std::uint8_t* dst, int target_mode)
{
    switch (target_mode) {
    case PILLOW_C_MODE_1:
        if (spec.kind == RawCodecKind::One8) {
            dst[0] = src[0] != 0 ? 255u : 0u;
        }
        return;
    case PILLOW_C_MODE_P:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_L:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_LA:
    case PILLOW_C_MODE_PA:
        dst[0] = src[0];
        dst[1] = src[1];
        return;
    case PILLOW_C_MODE_RGB:
        switch (spec.kind) {
        case RawCodecKind::RGB:
        case RawCodecKind::RGBX:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            return;
        case RawCodecKind::BGR:
        case RawCodecKind::BGRX:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            return;
        case RawCodecKind::XBGR:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_RGBA:
        switch (spec.kind) {
        case RawCodecKind::RGBA:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::BGRA:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            return;
        case RawCodecKind::ARGB:
            dst[0] = src[1];
            dst[1] = src[2];
            dst[2] = src[3];
            dst[3] = src[0];
            return;
        case RawCodecKind::ABGR:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        case RawCodecKind::BGR:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 255;
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_RGBX:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    case PILLOW_C_MODE_CMYK:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    case PILLOW_C_MODE_YCBCR:
    case PILLOW_C_MODE_HSV:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        return;
    case PILLOW_C_MODE_LAB:
        dst[0] = src[0];
        dst[1] = static_cast<std::uint8_t>(src[1] ^ 0x80u);
        dst[2] = static_cast<std::uint8_t>(src[2] ^ 0x80u);
        return;
    case PILLOW_C_MODE_I:
        switch (spec.kind) {
        case RawCodecKind::I:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::I8:
            write_i32_le(dst, static_cast<std::int32_t>(static_cast<std::int8_t>(src[0])));
            return;
        case RawCodecKind::I32Big:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        case RawCodecKind::I32Native:
            if (native_is_little_endian()) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];
            } else {
                dst[0] = src[3];
                dst[1] = src[2];
                dst[2] = src[1];
                dst[3] = src[0];
            }
            return;
        case RawCodecKind::I16Little:
            write_i32_le(dst, read_u16_le(src));
            return;
        case RawCodecKind::I16Big:
            write_i32_le(dst, read_u16_be(src));
            return;
        case RawCodecKind::I16Native:
            write_i32_le(dst, native_is_little_endian() ? read_u16_le(src) : read_u16_be(src));
            return;
        case RawCodecKind::I16SignedLittle:
            write_i32_le(dst, static_cast<std::int32_t>(static_cast<std::int16_t>(read_u16_le(src))));
            return;
        default:
            return;
        }
        return;
    case PILLOW_C_MODE_F:
        if (spec.kind == RawCodecKind::F ||
            (spec.kind == RawCodecKind::F32Native && native_is_little_endian())) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else if (spec.kind == RawCodecKind::F32Big ||
                   spec.kind == RawCodecKind::F32Native) {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
        } else if (spec.kind == RawCodecKind::F64Little) {
            write_f32_le(dst, static_cast<float>(read_f64_le(src)));
        }
        return;
    case PILLOW_C_MODE_I16:
        if (spec.kind == RawCodecKind::I16Little) {
            dst[0] = src[0];
            dst[1] = src[1];
        }
        return;
    case PILLOW_C_MODE_I16B:
        if (spec.kind == RawCodecKind::I16Big) {
            dst[0] = src[0];
            dst[1] = src[1];
        }
        return;
    default:
        return;
    }
}

int set_mode1_raw_bytes_image(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    int stride,
    int orientation)
{
    if (!image || !data) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stride < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t tight_stride = 0;
    std::size_t tight_size = 0;
    if (!checked_mode1_raw_size(image, &tight_stride, &tight_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t source_stride = stride == 0 ? tight_stride : static_cast<std::size_t>(stride);
    if (source_stride < tight_stride) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->height > 0) {
        const std::size_t required = source_stride * static_cast<std::size_t>(image->height - 1) + tight_stride;
        if (required > size) {
            return PILLOW_C_INVALID_LENGTH;
        }
    }

    for (int y = 0; y < image->height; ++y) {
        const int source_y = orientation < 0 ? (image->height - 1 - y) : y;
        const std::uint8_t* src_row = data + static_cast<std::size_t>(source_y) * source_stride;
        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            const std::uint8_t packed = src_row[static_cast<std::size_t>(x) / 8u];
            const int shift = 7 - (x & 7);
            dst_row[x] = ((packed >> shift) & 1u) ? 255u : 0u;
        }
    }
    return PILLOW_C_OK;
}

void encode_raw_pixel(const RawCodecSpec& spec, const std::uint8_t* src, std::uint8_t* dst, int source_mode)
{
    switch (source_mode) {
    case PILLOW_C_MODE_P:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_L:
        dst[0] = src[0];
        return;
    case PILLOW_C_MODE_LA:
    case PILLOW_C_MODE_PA:
        dst[0] = src[0];
        dst[1] = src[1];
        return;
    case PILLOW_C_MODE_RGB:
        switch (spec.kind) {
        case RawCodecKind::RGB:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            return;
        case RawCodecKind::BGR:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            return;
        case RawCodecKind::RGBX:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
            return;
        case RawCodecKind::BGRX:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 0;
            return;
        case RawCodecKind::XBGR:
            dst[0] = 0;
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_RGBA:
        switch (spec.kind) {
        case RawCodecKind::RGBA:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::BGRA:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            return;
        case RawCodecKind::ABGR:
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            return;
        case RawCodecKind::RGB:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            return;
        case RawCodecKind::BGR:
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            return;
        default:
            return;
        }
    case PILLOW_C_MODE_RGBX:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    case PILLOW_C_MODE_CMYK:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        return;
    case PILLOW_C_MODE_YCBCR:
    case PILLOW_C_MODE_HSV:
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        return;
    case PILLOW_C_MODE_LAB:
        dst[0] = src[0];
        dst[1] = static_cast<std::uint8_t>(src[1] ^ 0x80u);
        dst[2] = static_cast<std::uint8_t>(src[2] ^ 0x80u);
        return;
    case PILLOW_C_MODE_I:
        switch (spec.kind) {
        case RawCodecKind::I:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            return;
        case RawCodecKind::I16Big: {
            const std::uint16_t value = clip_i32_to_u16(read_i32_le(src));
            dst[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            dst[1] = static_cast<std::uint8_t>(value & 0xFFu);
            return;
        }
        default:
            return;
        }
        return;
    case PILLOW_C_MODE_F:
        if (spec.kind == RawCodecKind::F ||
            (spec.kind == RawCodecKind::F32Native && native_is_little_endian())) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else if (spec.kind == RawCodecKind::F32Native) {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
        }
        return;
    case PILLOW_C_MODE_I16:
        if (spec.kind == RawCodecKind::I16Little) {
            dst[0] = src[0];
            dst[1] = src[1];
        }
        return;
    case PILLOW_C_MODE_I16B:
        if (spec.kind == RawCodecKind::I16Big) {
            dst[0] = src[0];
            dst[1] = src[1];
        }
        return;
    default:
        return;
    }
}

int set_raw_bytes_image(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    const char* raw_mode,
    int stride,
    int orientation)
{
    if (!image || !data || !raw_mode) {
        return PILLOW_C_NULL_POINTER;
    }
    const RawCodecSpec spec = raw_decode_spec(image->mode, raw_mode);
    if (spec.kind == RawCodecKind::One) {
        return set_mode1_raw_bytes_image(image, data, size, stride, orientation);
    }
    if (spec.kind == RawCodecKind::Unsupported || spec.bytes_per_pixel <= 0 || stride < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const std::size_t tight_stride = static_cast<std::size_t>(image->width) * static_cast<std::size_t>(spec.bytes_per_pixel);
    const std::size_t source_stride = stride == 0 ? tight_stride : static_cast<std::size_t>(stride);
    if (source_stride < tight_stride) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->height > 0) {
        const std::size_t required = source_stride * static_cast<std::size_t>(image->height - 1) + tight_stride;
        if (required > size) {
            return PILLOW_C_INVALID_LENGTH;
        }
    }

    for (int y = 0; y < image->height; ++y) {
        const int source_y = orientation < 0 ? (image->height - 1 - y) : y;
        const std::uint8_t* src_row = data + static_cast<std::size_t>(source_y) * source_stride;
        std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = 0; x < image->width; ++x) {
            if (spec.planar) {
                std::uint8_t pixel[4] = {0, 0, 0, 0};
                for (int c = 0; c < spec.bytes_per_pixel && c < 4; ++c) {
                    pixel[c] = src_row[static_cast<std::size_t>(c) * static_cast<std::size_t>(image->width) + static_cast<std::size_t>(x)];
                }
                decode_raw_pixel(
                    spec,
                    pixel,
                    dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels),
                    image->mode);
            } else {
                decode_raw_pixel(
                    spec,
                    src_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(spec.bytes_per_pixel),
                    dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels),
                    image->mode);
            }
        }
    }
    return PILLOW_C_OK;
}

int refresh_buffer_view_image(PillowCImage* image)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!image->buffer_readonly) {
        return PILLOW_C_OK;
    }
    if (!image->buffer_source || image->buffer_raw_mode.empty()) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return set_raw_bytes_image(
        image,
        image->buffer_source,
        image->buffer_source_size,
        image->buffer_raw_mode.c_str(),
        image->buffer_stride,
        image->buffer_orientation);
}

void clear_buffer_view_image(PillowCImage* image)
{
    if (!image) {
        return;
    }
    image->buffer_source = nullptr;
    image->buffer_source_size = 0;
    image->buffer_raw_mode.clear();
    image->buffer_stride = 0;
    image->buffer_orientation = 1;
    image->buffer_readonly = false;
}

int detach_buffer_view_image(PillowCImage* image)
{
    const int status = refresh_buffer_view_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    clear_buffer_view_image(image);
    return PILLOW_C_OK;
}

template <typename Func>
int with_detached_buffer_view(PillowCImage* image, Func func)
{
    const int status = detach_buffer_view_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return func();
}

int refresh_const_buffer_view_image(const PillowCImage* image)
{
    return refresh_buffer_view_image(const_cast<PillowCImage*>(image));
}

int frombuffer_raw_image(
    int width,
    int height,
    int mode,
    const std::uint8_t* data,
    std::size_t size,
    const char* raw_mode,
    int stride,
    int orientation,
    int alias_source,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (!data || !raw_mode) {
        return PILLOW_C_NULL_POINTER;
    }

    int output_mode = mode;
    if (std::strcmp(raw_mode, "L") == 0) {
        output_mode = PILLOW_C_MODE_L;
    } else if (std::strcmp(raw_mode, "RGBA") == 0) {
        output_mode = PILLOW_C_MODE_RGBA;
    } else if (std::strcmp(raw_mode, "RGBX") == 0) {
        output_mode = PILLOW_C_MODE_RGBX;
    }

    const int channels = channels_for_mode(output_mode);
    if (channels == 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::size_t image_stride = 0;
    std::size_t image_size = 0;
    if (!checked_image_size(width, height, channels, &image_stride, &image_size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    try {
        auto* image = new PillowCImage{
            width,
            height,
            output_mode,
            channels,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        const int status = set_raw_bytes_image(image, data, size, raw_mode, stride, orientation);
        if (status != PILLOW_C_OK) {
            delete image;
            return status;
        }
        const bool can_alias_source =
            std::strcmp(raw_mode, "L") == 0 ||
            std::strcmp(raw_mode, "RGBA") == 0 ||
            std::strcmp(raw_mode, "RGBX") == 0;
        if (alias_source != 0 && can_alias_source) {
            image->buffer_source = data;
            image->buffer_source_size = size;
            image->buffer_raw_mode = raw_mode;
            image->buffer_stride = stride;
            image->buffer_orientation = orientation;
            image->buffer_readonly = true;
        }
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int get_mode1_raw_bytes_oriented_image(
    const PillowCImage* image,
    int orientation,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }

    std::size_t row_bytes = 0;
    std::size_t required = 0;
    if (!checked_mode1_raw_size(image, &row_bytes, &required)) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (required == 0) {
        return PILLOW_C_OK;
    }
    std::fill(out, out + required, std::uint8_t{0});

    for (int y = 0; y < image->height; ++y) {
        const int output_y = orientation < 0 ? (image->height - 1 - y) : y;
        const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        std::uint8_t* dst_row = out + static_cast<std::size_t>(output_y) * row_bytes;
        for (int x = 0; x < image->width; ++x) {
            if (src_row[x] != 0) {
                dst_row[static_cast<std::size_t>(x) / 8u] |= static_cast<std::uint8_t>(0x80u >> (x & 7));
            }
        }
    }
    return PILLOW_C_OK;
}

int get_raw_bytes_oriented_image(
    const PillowCImage* image,
    const char* raw_mode,
    int orientation,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    if (!image || !raw_mode || !out_required) {
        return PILLOW_C_NULL_POINTER;
    }
    const RawCodecSpec spec = raw_encode_spec(image->mode, raw_mode);
    if (spec.kind == RawCodecKind::One) {
        return get_mode1_raw_bytes_oriented_image(image, orientation, out, out_size, out_required);
    }
    if (spec.kind == RawCodecKind::Unsupported || spec.bytes_per_pixel <= 0) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::size_t required = 0;
    if (!checked_raw_output_size(image, spec.bytes_per_pixel, &required)) {
        *out_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_required = required;
    if (!out) {
        return PILLOW_C_OK;
    }
    if (out_size < required) {
        return PILLOW_C_INVALID_LENGTH;
    }

    for (int y = 0; y < image->height; ++y) {
        const int output_y = orientation < 0 ? (image->height - 1 - y) : y;
        const std::uint8_t* src_row = image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        std::uint8_t* dst_row = out + static_cast<std::size_t>(output_y) * static_cast<std::size_t>(image->width) * static_cast<std::size_t>(spec.bytes_per_pixel);
        for (int x = 0; x < image->width; ++x) {
            if (spec.planar) {
                std::uint8_t pixel[4] = {0, 0, 0, 0};
                encode_raw_pixel(
                    spec,
                    src_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels),
                    pixel,
                    image->mode);
                for (int c = 0; c < spec.bytes_per_pixel && c < 4; ++c) {
                    dst_row[static_cast<std::size_t>(c) * static_cast<std::size_t>(image->width) + static_cast<std::size_t>(x)] = pixel[c];
                }
            } else {
                encode_raw_pixel(
                    spec,
                    src_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(image->channels),
                    dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(spec.bytes_per_pixel),
                    image->mode);
            }
        }
    }
    return PILLOW_C_OK;
}

int get_raw_bytes_image(
    const PillowCImage* image,
    const char* raw_mode,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    return get_raw_bytes_oriented_image(image, raw_mode, 1, out, out_size, out_required);
}

} // namespace

bool pillow_c_checked_mode1_raw_size(
    const PillowCImage* image,
    std::size_t* row_bytes,
    std::size_t* out_size)
{
    return checked_mode1_raw_size(image, row_bytes, out_size);
}

std::int32_t pillow_c_read_i32_le(const std::uint8_t* data)
{
    return read_i32_le(data);
}

std::uint16_t pillow_c_clip_i32_to_u16(std::int32_t value)
{
    return clip_i32_to_u16(value);
}

float pillow_c_read_f32_le(const std::uint8_t* data)
{
    return read_f32_le(data);
}

void pillow_c_write_i32_le(std::uint8_t* data, std::uint32_t value)
{
    write_i32_le(data, value);
}

void pillow_c_write_f32_le(std::uint8_t* data, float value)
{
    write_f32_le(data, value);
}

int pillow_c_refresh_const_buffer_view_image(const PillowCImage* image)
{
    return refresh_const_buffer_view_image(image);
}

int pillow_c_detach_buffer_view_image(PillowCImage* image)
{
    return detach_buffer_view_image(image);
}

extern "C" __declspec(dllexport) int pillow_c_image_frombuffer_raw(
    int width,
    int height,
    int mode,
    const std::uint8_t* data,
    std::size_t size,
    const char* raw_mode,
    int stride,
    int orientation,
    int alias_source,
    PillowCImage** out_image)
{
    return frombuffer_raw_image(width, height, mode, data, size, raw_mode, stride, orientation, alias_source, out_image);
}

extern "C" __declspec(dllexport) int pillow_c_image_refresh_buffer(PillowCImage* image)
{
    return refresh_buffer_view_image(image);
}

extern "C" __declspec(dllexport) int pillow_c_image_detach_buffer(PillowCImage* image)
{
    return detach_buffer_view_image(image);
}

extern "C" __declspec(dllexport) int pillow_c_image_set_bytes(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size)
{
    if (!image || !data) {
        return PILLOW_C_NULL_POINTER;
    }
    const int detach_status = detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        return detach_status;
    }
    if (size != image->pixels.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(image->pixels.data(), data, size);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_set_raw_bytes(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    const char* raw_mode,
    int stride,
    int orientation)
{
    const int detach_status = detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        return detach_status;
    }
    return set_raw_bytes_image(image, data, size, raw_mode, stride, orientation);
}

extern "C" __declspec(dllexport) int pillow_c_image_put_data(
    PillowCImage* image,
    const std::uint8_t* data,
    std::size_t size,
    std::size_t pixel_count)
{
    if (!image || (!data && size > 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    const int detach_status = detach_buffer_view_image(image);
    if (detach_status != PILLOW_C_OK) {
        return detach_status;
    }
    if (pixel_count > static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const std::size_t expected_size = pixel_count * static_cast<std::size_t>(image->channels);
    if (size != expected_size || expected_size > image->pixels.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (expected_size > 0) {
        std::memcpy(image->pixels.data(), data, expected_size);
    }
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_get_bytes(
    const PillowCImage* image,
    std::uint8_t* out,
    std::size_t size)
{
    if (!image || !out) {
        return PILLOW_C_NULL_POINTER;
    }
    if (size != image->pixels.size()) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const int refresh_status = refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    std::memcpy(out, image->pixels.data(), size);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_get_raw_bytes(
    const PillowCImage* image,
    const char* raw_mode,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    const int refresh_status = refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return get_raw_bytes_image(image, raw_mode, out, out_size, out_required);
}

extern "C" __declspec(dllexport) int pillow_c_image_get_raw_bytes_oriented(
    const PillowCImage* image,
    const char* raw_mode,
    int orientation,
    std::uint8_t* out,
    std::size_t out_size,
    std::size_t* out_required)
{
    const int refresh_status = refresh_const_buffer_view_image(image);
    if (refresh_status != PILLOW_C_OK) {
        return refresh_status;
    }
    return get_raw_bytes_oriented_image(image, raw_mode, orientation, out, out_size, out_required);
}

