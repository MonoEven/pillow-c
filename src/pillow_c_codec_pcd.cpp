// BEHAV-OPEN-007: Kodak PhotoCD (PCD) opener.
//
// Mirrors Pillow 11.3.0's PcdImagePlugin + PcdDecode.c + UnpackYCC.c: the
// "PCD_" header sector at offset 2048, the orientation byte at 1538, the
// 96-sector offset to the base image, the row-pair YCC unpacking with the
// PhotoYCC lookup tables, and Pillow's load_end quirk where orientations 1/3
// crash with an AttributeError (ImagingCore has no rotate) that escapes
// load unwrapped.

#include "pillow_c_internal.h"

namespace {

constexpr int PILLOW_C_OPEN_PCD_ROTATE = -53;
constexpr int PILLOW_C_OPEN_PCD_TRUNCATED = -54;

constexpr std::size_t PCD_TILE_OFFSET = 96u * 2048u;
constexpr std::size_t PCD_CHUNK = 2304u; // 2 rows: 768+768 Y + 384 Cb + 384 Cr
constexpr int PCD_WIDTH = 768;
constexpr int PCD_HEIGHT = 512;

// Tables generated from Pillow's src/libImaging/UnpackYCC.c (pcdtables.py
// transforms from the Roberts/Ford colour space conversion FAQ).
#include "pcd_ycc_tables.inc"

int open_pcd_image(const char* path, PillowCImage** out_image)
{
    if (!path || !out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    try {
        std::vector<std::uint8_t> data;
        if (!read_binary_file(path, &data)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        // PcdImagePlugin._open: seek(2048), read 2048, require "PCD_"
        if (data.size() < 4096u ||
            data[2048] != 'P' || data[2049] != 'C' || data[2050] != 'D' || data[2051] != '_') {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int orientation = data[2048u + 1538u] & 3;

        // the base image decode; a truncated stream fails before Pillow's
        // load_end rotation quirk
        const std::size_t avail = data.size() > PCD_TILE_OFFSET ? data.size() - PCD_TILE_OFFSET : 0u;
        if (avail < PCD_CHUNK * (PCD_HEIGHT / 2)) {
            return PILLOW_C_OPEN_PCD_TRUNCATED;
        }

        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(PCD_WIDTH, PCD_HEIGHT, 3, &image_stride, &image_size)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            PCD_WIDTH,
            PCD_HEIGHT,
            PILLOW_C_MODE_RGB,
            3,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        std::uint8_t* pixels = image->pixels.data();
        const std::uint8_t* src = data.data() + PCD_TILE_OFFSET;
        for (int c = 0; c < PCD_HEIGHT / 2; ++c) {
            const std::uint8_t* chunk = src + static_cast<std::size_t>(c) * PCD_CHUNK;
            std::uint8_t* row0 = pixels + static_cast<std::size_t>(2 * c) * image_stride;
            std::uint8_t* row1 = pixels + static_cast<std::size_t>(2 * c + 1) * image_stride;
            for (int x = 0; x < PCD_WIDTH; ++x) {
                const int y0 = chunk[x];
                const int y1 = chunk[x + PCD_WIDTH];
                const int cb = chunk[1536 + x / 2];
                const int cr = chunk[1920 + x / 2];
                {
                    const int l = PCD_L[y0];
                    int r = l + PCD_CR[cr];
                    int g = l + PCD_GR[cr] + PCD_GB[cb];
                    int b = l + PCD_CB[cb];
                    row0[3 * x + 0] = static_cast<std::uint8_t>(r <= 0 ? 0 : (r >= 255 ? 255 : r));
                    row0[3 * x + 1] = static_cast<std::uint8_t>(g <= 0 ? 0 : (g >= 255 ? 255 : g));
                    row0[3 * x + 2] = static_cast<std::uint8_t>(b <= 0 ? 0 : (b >= 255 ? 255 : b));
                }
                {
                    const int l = PCD_L[y1];
                    int r = l + PCD_CR[cr];
                    int g = l + PCD_GR[cr] + PCD_GB[cb];
                    int b = l + PCD_CB[cb];
                    row1[3 * x + 0] = static_cast<std::uint8_t>(r <= 0 ? 0 : (r >= 255 ? 255 : r));
                    row1[3 * x + 1] = static_cast<std::uint8_t>(g <= 0 ? 0 : (g >= 255 ? 255 : g));
                    row1[3 * x + 2] = static_cast<std::uint8_t>(b <= 0 ? 0 : (b >= 255 ? 255 : b));
                }
            }
        }

        // Pillow's load_end: self.im.rotate(90 / -90) -- ImagingCore has no
        // rotate in 11.3.0, so orientations 1/3 raise the AttributeError
        // that escapes load unwrapped.
        if (orientation == 1 || orientation == 3) {
            delete image;
            return PILLOW_C_OPEN_PCD_ROTATE;
        }

        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_open_pcd(
    const char* path,
    PillowCImage** out_image)
{
    return open_pcd_image(path, out_image);
}
