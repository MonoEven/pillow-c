#pragma once

#include "pillow_c_internal.h"

#include <cstddef>
#include <cstdint>

// Shared font-handle layout. Kind 1 is the built-in default bitmap font owned
// by pillow_c_draw.cpp; kind 2 is a loaded TrueType/OpenType face owned by
// pillow_c_codec_font.cpp (the `truetype` pointer holds a PillowCTtFont).
constexpr int PILLOW_C_FONT_DEFAULT = 1;
constexpr int PILLOW_C_FONT_TRUETYPE = 2;

constexpr int PILLOW_C_FONT_LAYOUT_BASIC = 0;
constexpr int PILLOW_C_FONT_LAYOUT_RAQM = 1;

struct PillowCFont {
    int kind;
    void* truetype;  // PillowCTtFont*, only when kind == PILLOW_C_FONT_TRUETYPE
};

// Kind-2 seams implemented by pillow_c_codec_font.cpp.
int font_tt_load_bytes(
    const std::uint8_t* data,
    std::size_t length,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font);
int font_tt_load_file(
    const char* path,
    double size,
    int index,
    const char* encoding,
    int layout_engine,
    PillowCFont** out_font);
int font_tt_free(PillowCFont* font);
int font_tt_variant(const PillowCFont* font, PillowCFont** out_font);
int font_tt_is_variable(const PillowCFont* font, int* out_variable);
int font_tt_getlength(const PillowCFont* font, const char* text, double* out_length);
int font_tt_getmetrics(const PillowCFont* font, int* out_ascent, int* out_descent);
int font_tt_getname(
    const PillowCFont* font,
    char* out_family,
    std::size_t family_size,
    std::size_t* out_family_required,
    char* out_style,
    std::size_t style_size,
    std::size_t* out_style_required);
int font_tt_getbbox(
    const PillowCFont* font,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom);
int font_tt_getbbox_anchor(
    const PillowCFont* font,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom);
int font_tt_getmask(
    const PillowCFont* font,
    const char* text,
    const char* mode,
    int ink,
    PillowCImage** out_image);
