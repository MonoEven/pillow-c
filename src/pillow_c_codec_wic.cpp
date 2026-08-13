#include <algorithm>
#include <limits>
#include <new>
#include <vector>

#include "pillow_c_internal.h"
#include "pillow_c_wic_internal.h"

int create_wic_factory(ComPtr<IWICImagingFactory>* factory)
{
    if (!factory) {
        return PILLOW_C_NULL_POINTER;
    }
    const HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory->put()));
    return SUCCEEDED(hr) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
}

int wic_format_to_mode(const WICPixelFormatGUID& format, int* mode, int* channels, WICPixelFormatGUID* target_format)
{
    if (!mode || !channels || !target_format) {
        return PILLOW_C_NULL_POINTER;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormatBlackWhite) ||
        IsEqualGUID(format, GUID_WICPixelFormat1bppIndexed)) {
        *mode = PILLOW_C_MODE_1;
        *channels = 1;
        *target_format = GUID_WICPixelFormat8bppGray;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat8bppGray)) {
        *mode = PILLOW_C_MODE_L;
        *channels = 1;
        *target_format = GUID_WICPixelFormat8bppGray;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat24bppRGB)) {
        *mode = PILLOW_C_MODE_RGB;
        *channels = 3;
        *target_format = GUID_WICPixelFormat24bppRGB;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppRGBA)) {
        *mode = PILLOW_C_MODE_RGBA;
        *channels = 4;
        *target_format = GUID_WICPixelFormat32bppRGBA;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) ||
        IsEqualGUID(format, GUID_WICPixelFormat32bppPBGRA) ||
        IsEqualGUID(format, GUID_WICPixelFormat32bppPRGBA)) {
        *mode = PILLOW_C_MODE_RGBA;
        *channels = 4;
        *target_format = GUID_WICPixelFormat32bppRGBA;
        return PILLOW_C_OK;
    }
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppCMYK)) {
        *mode = PILLOW_C_MODE_CMYK;
        *channels = 4;
        *target_format = GUID_WICPixelFormat32bppCMYK;
        return PILLOW_C_OK;
    }
    *mode = PILLOW_C_MODE_RGB;
    *channels = 3;
    *target_format = GUID_WICPixelFormat24bppRGB;
    return PILLOW_C_OK;
}

int copy_wic_palette_rgb(IWICBitmapSource* source, IWICImagingFactory* factory, std::vector<std::uint8_t>* out_palette)
{
    if (!source || !factory || !out_palette) {
        return PILLOW_C_NULL_POINTER;
    }
    ComPtr<IWICPalette> palette;
    HRESULT hr = factory->CreatePalette(palette.put());
    if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    hr = source->CopyPalette(palette.get());
    if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    UINT count = 0;
    hr = palette->GetColorCount(&count);
    if (FAILED(hr) || count == 0 || count > 256u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    std::vector<WICColor> colors(count);
    UINT actual = 0;
    hr = palette->GetColors(count, colors.data(), &actual);
    if (FAILED(hr) || actual == 0 || actual > 256u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    out_palette->assign(static_cast<std::size_t>(actual) * 3u, std::uint8_t{0});
    for (UINT i = 0; i < actual; ++i) {
        const WICColor color = colors[i];
        (*out_palette)[static_cast<std::size_t>(i) * 3u + 0u] = static_cast<std::uint8_t>((color >> 16) & 0xffu);
        (*out_palette)[static_cast<std::size_t>(i) * 3u + 1u] = static_cast<std::uint8_t>((color >> 8) & 0xffu);
        (*out_palette)[static_cast<std::size_t>(i) * 3u + 2u] = static_cast<std::uint8_t>(color & 0xffu);
    }
    return PILLOW_C_OK;
}

int create_wic_palette_from_rgb(
    IWICImagingFactory* factory,
    const std::vector<std::uint8_t>& palette_rgb,
    ComPtr<IWICPalette>* out_palette)
{
    if (!factory || !out_palette) {
        return PILLOW_C_NULL_POINTER;
    }
    if (palette_rgb.size() % 3u != 0u || palette_rgb.size() > 256u * 3u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const std::size_t source_count = palette_rgb.size() / 3u;
    const std::size_t color_count = std::max<std::size_t>(source_count, 2u);
    if (color_count > 256u) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    std::vector<WICColor> colors(color_count, 0xff000000u);
    for (std::size_t i = 0; i < source_count; ++i) {
        colors[i] = 0xff000000u |
                    (static_cast<std::uint32_t>(palette_rgb[i * 3u + 0u]) << 16) |
                    (static_cast<std::uint32_t>(palette_rgb[i * 3u + 1u]) << 8) |
                    static_cast<std::uint32_t>(palette_rgb[i * 3u + 2u]);
    }

    HRESULT hr = factory->CreatePalette(out_palette->put());
    if (FAILED(hr)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    hr = out_palette->get()->InitializeCustom(colors.data(), static_cast<UINT>(colors.size()));
    return SUCCEEDED(hr) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
}

int wic_container_frame_count(const char* path, const GUID& container_format, int* out_count)
{
    if (!path || !out_count) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_count = 0;
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
        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(
            wide_path.data(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        GUID actual_container = {};
        if (FAILED(decoder->GetContainerFormat(&actual_container)) || !IsEqualGUID(actual_container, container_format)) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        UINT frame_count = 0;
        hr = decoder->GetFrameCount(&frame_count);
        if (FAILED(hr) || frame_count > static_cast<UINT>(std::numeric_limits<int>::max())) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        *out_count = static_cast<int>(frame_count);
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

