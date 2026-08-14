#include "pillow_c_internal.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace {

int allocate_rgb_image(int width, int height, PillowCImage** out_image)
{
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(width, height, 3, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        auto* image = new PillowCImage{
            width,
            height,
            PILLOW_C_MODE_RGB,
            3,
            stride,
            std::vector<std::uint8_t>(size)};
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int allocate_mode_image(int width, int height, int mode, int channels, PillowCImage** out_image)
{
    std::size_t stride = 0;
    std::size_t size = 0;
    if (!checked_image_size_allow_empty(width, height, channels, &stride, &size)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        auto* image = new PillowCImage{
            width,
            height,
            mode,
            channels,
            stride,
            std::vector<std::uint8_t>(size)};
        *out_image = image;
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int grab_screen_into(
    int left,
    int top,
    int width,
    int height,
    int all_screens,
    int include_layered,
    PillowCImage* target)
{
    (void)all_screens; // virtual-screen bboxes are normalized by the caller
    if (width <= 0 || height <= 0) {
        return PILLOW_C_OK;
    }
    if (width > 32767 || height > 32767) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    HDC screen_dc = GetDC(nullptr);
    if (!screen_dc) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    if (!mem_dc) {
        ReleaseDC(nullptr, screen_dc);
        return PILLOW_C_INVALID_ARGUMENT;
    }
    HBITMAP bitmap = CreateCompatibleBitmap(screen_dc, width, height);
    if (!bitmap) {
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, screen_dc);
        return PILLOW_C_ALLOCATION_FAILED;
    }
    HGDIOBJ old = SelectObject(mem_dc, bitmap);
    const DWORD rop = SRCCOPY | (include_layered ? CAPTUREBLT : 0);
    const BOOL ok = BitBlt(mem_dc, 0, 0, width, height, screen_dc, left, top, rop);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = width;
    bi.biHeight = -height; // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    std::vector<std::uint8_t> dib;
    int rows = 0;
    if (ok) {
        dib.resize(static_cast<std::size_t>(width) * height * 3u);
        rows = GetDIBits(
            mem_dc,
            bitmap,
            0,
            static_cast<UINT>(height),
            dib.data(),
            reinterpret_cast<BITMAPINFO*>(&bi),
            DIB_RGB_COLORS);
    }
    SelectObject(mem_dc, old);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    if (!ok || rows != height) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    // BGR (GDI DIB) -> RGB Pillow byte order.
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    for (std::size_t i = 0; i < pixels; ++i) {
        target->pixels[i * 3u + 0u] = dib[i * 3u + 2u];
        target->pixels[i * 3u + 1u] = dib[i * 3u + 1u];
        target->pixels[i * 3u + 2u] = dib[i * 3u + 0u];
    }
    return PILLOW_C_OK;
}

int decode_clipboard_dib(const std::uint8_t* dib, std::size_t dib_size, PillowCImage** out_image)
{
    if (!dib || dib_size < sizeof(BITMAPINFOHEADER)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    BITMAPINFOHEADER header{};
    std::memcpy(&header, dib, sizeof(header));
    if (header.biSize < sizeof(BITMAPINFOHEADER) || header.biCompression != BI_RGB) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (header.biWidth < 0 || header.biWidth > 32767) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int width = header.biWidth;
    const int height = header.biHeight;
    const int abs_height = height < 0 ? -height : height;
    const bool top_down = height < 0;
    if (abs_height > 32767) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (width == 0 || abs_height == 0) {
        int mode = PILLOW_C_MODE_L;
        int channels = 1;
        if (header.biBitCount == 24 || header.biBitCount == 32) {
            mode = PILLOW_C_MODE_RGB;
            channels = 3;
        } else if (header.biBitCount == 1) {
            mode = PILLOW_C_MODE_1;
        }
        return allocate_mode_image(width, abs_height, mode, channels, out_image);
    }

    const std::size_t palette_entries = header.biBitCount <= 8
        ? (header.biClrUsed != 0 ? static_cast<std::size_t>(header.biClrUsed) : (std::size_t{1} << header.biBitCount))
        : 0u;
    const std::size_t palette_bytes = palette_entries * 4u;
    const std::size_t header_size = static_cast<std::size_t>(header.biSize);
    if (dib_size < header_size + palette_bytes) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::uint8_t* pixels = dib + header_size + palette_bytes;
    const std::size_t row_stride =
        ((static_cast<std::size_t>(width) * header.biBitCount + 31u) / 32u) * 4u;

    int mode = PILLOW_C_MODE_L;
    int channels = 1;
    switch (header.biBitCount) {
    case 1:
        mode = PILLOW_C_MODE_1;
        break;
    case 4:
    case 8:
        mode = PILLOW_C_MODE_L;
        break;
    case 24:
    case 32:
        mode = PILLOW_C_MODE_RGB;
        channels = 3;
        break;
    default:
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int status = allocate_mode_image(width, abs_height, mode, channels, out_image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    PillowCImage* image = *out_image;

    for (int out_y = 0; out_y < abs_height; ++out_y) {
        const int src_row = top_down ? out_y : (abs_height - 1 - out_y);
        const std::uint8_t* row = pixels + static_cast<std::size_t>(src_row) * row_stride;
        std::uint8_t* dst = image->pixels.data() + static_cast<std::size_t>(out_y) * image->stride;
        switch (header.biBitCount) {
        case 1:
            for (int x = 0; x < width; ++x) {
                dst[x] = (row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0;
            }
            break;
        case 4:
            for (int x = 0; x < width; ++x) {
                const std::uint8_t byte = row[x / 2];
                dst[x] = (x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
            }
            break;
        case 8:
            std::memcpy(dst, row, static_cast<std::size_t>(width));
            break;
        case 24:
            for (int x = 0; x < width; ++x) {
                dst[x * 3 + 0] = row[x * 3 + 2];
                dst[x * 3 + 1] = row[x * 3 + 1];
                dst[x * 3 + 2] = row[x * 3 + 0];
            }
            break;
        case 32:
            for (int x = 0; x < width; ++x) {
                dst[x * 3 + 0] = row[x * 4 + 2];
                dst[x * 3 + 1] = row[x * 4 + 1];
                dst[x * 3 + 2] = row[x * 4 + 0];
            }
            break;
        default:
            break;
        }
    }
    return PILLOW_C_OK;
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_grab(
    int left,
    int top,
    int right,
    int bottom,
    int all_screens,
    int include_layered,
    PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;
    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int status = allocate_rgb_image(right - left, bottom - top, out_image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    status = grab_screen_into(
        left,
        top,
        right - left,
        bottom - top,
        all_screens,
        include_layered,
        *out_image);
    if (status != PILLOW_C_OK) {
        delete *out_image;
        *out_image = nullptr;
    }
    return status;
}

extern "C" __declspec(dllexport) int pillow_c_image_grab_clipboard(PillowCImage** out_image)
{
    if (!out_image) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_image = nullptr;

    if (!OpenClipboard(nullptr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    HANDLE handle = GetClipboardData(CF_DIB);
    if (!handle) {
        CloseClipboard();
        return PILLOW_C_OK; // no image on the clipboard
    }
    const std::uint8_t* source =
        static_cast<const std::uint8_t*>(GlobalLock(handle));
    if (!source) {
        CloseClipboard();
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t source_size = GlobalSize(handle);
    const int status = decode_clipboard_dib(source, source_size, out_image);
    GlobalUnlock(handle);
    CloseClipboard();
    return status;
}
