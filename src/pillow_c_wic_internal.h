#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <wincodec.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

template <typename T>
struct ComPtr {
    T* ptr = nullptr;

    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ~ComPtr()
    {
        reset();
    }

    T** put()
    {
        reset();
        return &ptr;
    }

    void reset(T* value = nullptr)
    {
        if (ptr) {
            ptr->Release();
        }
        ptr = value;
    }

    T* get() const
    {
        return ptr;
    }

    T* operator->() const
    {
        return ptr;
    }
};

struct ComInitScope {
    HRESULT hr;
    bool initialized;

    ComInitScope()
        : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
          initialized(SUCCEEDED(hr))
    {
    }

    ~ComInitScope()
    {
        if (initialized) {
            CoUninitialize();
        }
    }

    bool usable() const
    {
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
};

struct PillowCImage;

int create_wic_factory(ComPtr<IWICImagingFactory>* factory);
int wic_format_to_mode(
    const WICPixelFormatGUID& format,
    int* mode,
    int* channels,
    WICPixelFormatGUID* target_format);
int copy_wic_palette_rgb(
    IWICBitmapSource* source,
    IWICImagingFactory* factory,
    std::vector<std::uint8_t>* out_palette);
int create_wic_palette_from_rgb(
    IWICImagingFactory* factory,
    const std::vector<std::uint8_t>& palette_rgb,
    ComPtr<IWICPalette>* out_palette);
int wic_container_frame_count(
    const char* path,
    const GUID& container_format,
    int* out_count);
