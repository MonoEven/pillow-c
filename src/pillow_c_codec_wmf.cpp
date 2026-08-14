// BEHAV-OPEN-009: Windows Metafile (WMF/EMF) opener.
//
// Mirrors Pillow 11.3.0's WmfImagePlugin + display.c PyImaging_DrawWmf: the
// 44-byte header parse (placeable vs enhanced), the size/dpi math, and the
// GDI render (SetWinMetaFileBits / SetEnhMetaFileBits -> a 24-bit DIB filled
// white -> EnumEnhMetaFile with plain PlayEnhMetaFileRecord -> bottom-up BGR
// bits converted to top-down RGB storage).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "pillow_c_internal.h"

namespace {

constexpr int PILLOW_C_OPEN_WMF_INCH = -55;     // "Invalid inch" ValueError escape
constexpr int PILLOW_C_OPEN_WMF_METAFILE = -56; // "cannot load metafile" OSError escape
constexpr int PILLOW_C_OPEN_WMF_BITMAP = -57;   // "cannot create bitmap" OSError escape
constexpr int PILLOW_C_OPEN_WMF_SELECT = -58;   // "cannot select bitmap" OSError escape

std::int16_t wmf_read_i16le(const std::uint8_t* p)
{
    return static_cast<std::int16_t>(p[0] | (p[1] << 8));
}

std::uint16_t wmf_read_u16le(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::int32_t wmf_read_i32le(const std::uint8_t* p)
{
    return static_cast<std::int32_t>(read_le32(p));
}

int CALLBACK wmf_play_record(HDC hdc, HANDLETABLE* table, const ENHMETARECORD* record, int count, LPARAM data)
{
    (void)data;
    PlayEnhMetaFileRecord(hdc, table, record, count);
    return 1;
}

int open_wmf_image(const char* path, PillowCImage** out_image)
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
        if (data.size() < 44u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const std::uint8_t* s = data.data();

        int width = 0;
        int height = 0;
        double dpi_x = 72.0;
        double dpi_y = 72.0;
        bool placeable = false;

        if (data.size() >= 6u && s[0] == 0xD7u && s[1] == 0xCDu && s[2] == 0xC6u && s[3] == 0x9Au &&
            s[4] == 0x00u && s[5] == 0x00u) {
            // placeable windows metafile
            const int inch = wmf_read_u16le(s + 14u);
            if (inch == 0) {
                return PILLOW_C_OPEN_WMF_INCH;
            }
            const int x0 = wmf_read_i16le(s + 6u);
            const int y0 = wmf_read_i16le(s + 8u);
            const int x1 = wmf_read_i16le(s + 10u);
            const int y1 = wmf_read_i16le(s + 12u);
            width = ((x1 - x0) * 72) / inch;
            height = ((y1 - y0) * 72) / inch;
            dpi_x = 72.0;
            dpi_y = 72.0;
            if (s[22] != 0x01u || s[23] != 0x00u || s[24] != 0x09u || s[25] != 0x00u) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            placeable = true;
        } else if (s[0] == 0x01u && s[1] == 0x00u && s[2] == 0x00u && s[3] == 0x00u &&
                   s[40] == ' ' && s[41] == 'E' && s[42] == 'M' && s[43] == 'F') {
            // enhanced metafile
            const int x0 = wmf_read_i32le(s + 8u);
            const int y0 = wmf_read_i32le(s + 12u);
            const int x1 = wmf_read_i32le(s + 16u);
            const int y1 = wmf_read_i32le(s + 20u);
            const int fx0 = wmf_read_i32le(s + 24u);
            const int fy0 = wmf_read_i32le(s + 28u);
            const int fx1 = wmf_read_i32le(s + 32u);
            const int fy1 = wmf_read_i32le(s + 36u);
            width = x1 - x0;
            height = y1 - y0;
            const double xdpi = 2540.0 * (x1 - x0) / (fx1 - fx0);
            const double ydpi = 2540.0 * (y1 - y0) / (fy1 - fy0);
            if (xdpi == ydpi) {
                dpi_x = xdpi;
                dpi_y = xdpi;
            } else {
                dpi_x = xdpi;
                dpi_y = ydpi;
            }
        } else {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
            return PILLOW_C_INVALID_ARGUMENT;
        }

        // GDI render (display.c PyImaging_DrawWmf)
        HENHMETAFILE meta = nullptr;
        if (placeable) {
            meta = SetWinMetaFileBits(static_cast<UINT>(data.size() - 22u), data.data() + 22u, nullptr, nullptr);
        } else {
            meta = SetEnhMetaFileBits(static_cast<UINT>(data.size()), data.data());
        }
        if (!meta) {
            return PILLOW_C_OPEN_WMF_METAFILE;
        }

        BITMAPCOREHEADER core;
        std::memset(&core, 0, sizeof(core));
        core.bcSize = sizeof(core);
        core.bcWidth = static_cast<WORD>(width);
        core.bcHeight = static_cast<WORD>(height);
        core.bcPlanes = 1;
        core.bcBitCount = 24;

        HDC dc = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, reinterpret_cast<const BITMAPINFO*>(&core), DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap) {
            DeleteDC(dc);
            DeleteEnhMetaFile(meta);
            return PILLOW_C_OPEN_WMF_BITMAP;
        }
        if (!SelectObject(dc, bitmap)) {
            DeleteObject(bitmap);
            DeleteDC(dc);
            DeleteEnhMetaFile(meta);
            return PILLOW_C_OPEN_WMF_SELECT;
        }

        RECT rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = width;
        rect.bottom = height;
        FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        EnumEnhMetaFile(dc, meta, wmf_play_record, nullptr, &rect);
        GdiFlush();

        const std::size_t stride = (static_cast<std::size_t>(width) * 3u + 3u) & ~static_cast<std::size_t>(3u);
        std::size_t image_stride = 0;
        std::size_t image_size = 0;
        if (!checked_image_size(width, height, 3, &image_stride, &image_size)) {
            DeleteObject(bitmap);
            DeleteDC(dc);
            DeleteEnhMetaFile(meta);
            return PILLOW_C_INVALID_ARGUMENT;
        }
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_RGB,
            3,
            image_stride,
            std::vector<std::uint8_t>(image_size)};
        const std::uint8_t* src = static_cast<const std::uint8_t*>(bits);
        // the DIB is bottom-up BGR with the padded stride; convert to
        // top-down RGB rows
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* src_row = src + static_cast<std::size_t>(y) * stride;
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(height - 1 - y) * image_stride;
            for (int x = 0; x < width; ++x) {
                dst_row[3 * x + 0] = src_row[3 * x + 2];
                dst_row[3 * x + 1] = src_row[3 * x + 1];
                dst_row[3 * x + 2] = src_row[3 * x + 0];
            }
        }
        image->has_dpi = true;
        image->dpi_x = dpi_x;
        image->dpi_y = dpi_y;

        DeleteObject(bitmap);
        DeleteDC(dc);
        DeleteEnhMetaFile(meta);
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_open_wmf(
    const char* path,
    PillowCImage** out_image)
{
    return open_wmf_image(path, out_image);
}
