#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "pillow_c_internal.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// ---------------------------------------------------------------------------
// BEHAV-PDF-001: PDF save (Pillow's PdfImagePlugin is save-only).
//
// Pillow 11.3.0's PdfImagePlugin._save writes through PdfParser:
//   %PDF-1.4 header plus the "% created by Pillow 11.3.0 PDF driver"
//   comment, then the Catalog/Pages objects, then per page an image XObject
//   (DCTDecode JPEG payload for L/RGB/CMYK, ASCIIHexDecode hex indices with
//   an Indexed DeviceRGB palette for P), a Page object and a Contents
//   stream, then the Info object, then xref/trailer.
// The layout is fully deterministic except the CreationDate/ModDate
// timestamps (UTC, one read per field). P mode is byte-exact; the
// L/RGB/CMYK JPEG payloads use this runtime's WIC encoder instead of
// Pillow's libjpeg (structure-exact, pixel-exact on decode). Mode 1
// (CCITTFaxDecode group4) and LA/RGBA (JPXDecode JPEG2000) are facade-side
// documented boundaries (no Pillow error exists to match: the local Pillow
// build performs them). This export mirrors PdfParser's serialization:
// PdfDict "<<\n/key value...\n>>", PdfArray "[ a b ]", %010d/%05d xref
// lines, Python str(float) MediaBox numbers and %f contents transforms.
// ---------------------------------------------------------------------------

namespace {

constexpr int PILLOW_C_PDF_MODE = -28;

void append_text(std::vector<std::uint8_t>& out, const char* text)
{
    if (text) {
        out.insert(out.end(), text, text + std::char_traits<char>::length(text));
    }
}

// Python str(float) formatting: shortest round-trip repr with the ".0"
// suffix Python keeps for integral floats (str(4.0) == "4.0").
std::string python_float_repr(double value)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
    if (result.ec != std::errc()) {
        return "0.0";
    }
    std::string text(buffer, result.ptr);
    if (text.find('.') == std::string::npos &&
        text.find('e') == std::string::npos &&
        text.find('E') == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::string pdf_date_now()
{
    SYSTEMTIME st{};
    GetSystemTime(&st);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "D:%04u%02u%02u%02u%02u%02uZ",
                  static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth),
                  static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour),
                  static_cast<unsigned>(st.wMinute), static_cast<unsigned>(st.wSecond));
    return std::string(buffer);
}

bool utf8_to_wide_string(const char* text, std::wstring* out)
{
    if (!out) {
        return false;
    }
    if (!text || !*text) {
        out->clear();
        return true;
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 0) {
        return false;
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(size));
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer.data(), size) <= 0) {
        return false;
    }
    out->assign(buffer.data(), buffer.data() + static_cast<std::size_t>(size) - 1u);
    return true;
}

bool wide_path_to_utf8(const std::wstring& wide, std::string* out)
{
    if (!out) {
        return false;
    }
    if (wide.empty()) {
        out->clear();
        return true;
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return false;
    }
    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, buffer.data(), size, nullptr, nullptr) <= 0) {
        return false;
    }
    out->assign(buffer.data(), buffer.data() + static_cast<std::size_t>(size) - 1u);
    return true;
}

// Pillow pdf_repr(str): a UTF-16BE string with BOM inside parentheses,
// with the byte-level escapes Pillow applies to the encoded bytes
// (\\, \(, \)) over the whole encoded string including the BOM.
bool append_pdf_string(std::vector<std::uint8_t>& out, const char* text)
{
    if (!text || !*text) {
        out.push_back('(');
        out.push_back(')');
        return true;
    }
    std::wstring wide;
    if (!utf8_to_wide_string(text, &wide)) {
        return false;
    }
    out.push_back('(');
    out.push_back(0xFE);
    out.push_back(0xFF);
    for (const wchar_t ch : wide) {
        const std::uint8_t bytes[2] = {
            static_cast<std::uint8_t>((static_cast<std::uint32_t>(ch) >> 8) & 0xFFu),
            static_cast<std::uint8_t>(static_cast<std::uint32_t>(ch) & 0xFFu)};
        for (const std::uint8_t byte : bytes) {
            if (byte == '\\' || byte == '(' || byte == ')') {
                out.push_back('\\');
            }
            out.push_back(byte);
        }
    }
    out.push_back(')');
    return true;
}

// Pillow's title default: os.path.splitext(os.path.basename(filename))[0].
std::string title_stem_from_path(const char* path)
{
    const std::string full(path ? path : "");
    const std::size_t base = full.find_last_of("/\\");
    const std::string basename = base == std::string::npos ? full : full.substr(base + 1u);
    const std::size_t dot = basename.find_last_of('.');
    if (dot == std::string::npos || dot == 0u) {
        return basename;
    }
    return basename.substr(0u, dot);
}

std::wstring build_temp_jpeg_path(std::uint64_t tick, int index)
{
    wchar_t temp_path[MAX_PATH];
    const DWORD length = GetTempPathW(MAX_PATH, temp_path);
    if (length == 0 || length >= MAX_PATH) {
        return std::wstring();
    }
    wchar_t name[64];
    std::swprintf(name, sizeof(name) / sizeof(name[0]), L"pillow-c-pdf-%llu-%d.jpg",
                  static_cast<unsigned long long>(tick), index);
    return std::wstring(temp_path) + name;
}

} // namespace

namespace {

struct PdfPageInfo {
    const PillowCImage* image;
    int procset;      // 0 = ImageB (L), 1 = ImageC (RGB/CMYK), 2 = ImageI (P)
    bool is_palette;
};

int save_pdf_document(const PillowCImage* const* images, int image_count,
                      const char* path, double x_resolution, double y_resolution,
                      const char* title, const char* author, const char* subject,
                      const char* keywords, const char* creator, const char* producer,
                      const char* creation_date, const char* mod_date)
{
    if (!images || !path || image_count <= 0) {
        return PILLOW_C_NULL_POINTER;
    }
    for (int i = 0; i < image_count; ++i) {
        if (!images[i]) {
            return PILLOW_C_NULL_POINTER;
        }
        if (images[i]->width <= 0 || images[i]->height <= 0) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const int refresh_status = pillow_c_refresh_const_buffer_view_image(images[i]);
        if (refresh_status != PILLOW_C_OK) {
            return refresh_status;
        }
    }

    // Pillow's per-page mode/filter mapping (PdfImagePlugin._write_image).
    std::vector<PdfPageInfo> pages;
    pages.reserve(static_cast<std::size_t>(image_count));
    for (int i = 0; i < image_count; ++i) {
        const PillowCImage* image = images[i];
        if (image->mode == PILLOW_C_MODE_P && image->channels == 1) {
            pages.push_back(PdfPageInfo{image, 2, true});
        } else if (image->mode == PILLOW_C_MODE_L && image->channels == 1) {
            pages.push_back(PdfPageInfo{image, 0, false});
        } else if (image->mode == PILLOW_C_MODE_RGB && image->channels == 3) {
            pages.push_back(PdfPageInfo{image, 1, false});
        } else if (image->mode == PILLOW_C_MODE_CMYK && image->channels == 4) {
            pages.push_back(PdfPageInfo{image, 1, false});
        } else {
            return PILLOW_C_PDF_MODE;
        }
    }

    try {
        const std::uint64_t tick = GetTickCount64();
        const int total_pages = image_count;
        const int root_id = 3 * total_pages + 1;
        const int pages_id = 3 * total_pages + 2;
        const int info_id = 3 * total_pages + 3;
        std::vector<std::uint64_t> offsets(static_cast<std::size_t>(3 * total_pages + 4), 0u);

        std::vector<std::uint8_t> out;
        out.reserve(4096u + static_cast<std::size_t>(total_pages) * 2048u);

        append_text(out, "%PDF-1.4\n");
        append_text(out, "% created by Pillow 11.3.0 PDF driver\n");

        auto append_integer = [&out](std::int64_t value) {
            char buffer[32];
            const int written = std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
            if (written > 0) {
                out.insert(out.end(), buffer, buffer + written);
            }
        };
        auto begin_object = [&out, &offsets](int object_id) {
            offsets[static_cast<std::size_t>(object_id)] = out.size();
            char header[32];
            const int written = std::snprintf(header, sizeof(header), "%d 0 obj", object_id);
            if (written > 0) {
                out.insert(out.end(), header, header + written);
            }
        };
        auto end_object = [&out]() {
            append_text(out, "endobj\n");
        };

        // Catalog.
        begin_object(root_id);
        append_text(out, "<<\n/Type /Catalog\n/Pages ");
        append_integer(pages_id);
        append_text(out, " 0 R\n>>");
        end_object();

        // Pages tree.
        begin_object(pages_id);
        append_text(out, "<<\n/Type /Pages\n/Count ");
        append_integer(total_pages);
        append_text(out, "\n/Kids [ ");
        for (int i = 0; i < total_pages; ++i) {
            if (i != 0) {
                append_text(out, " ");
            }
            append_integer(3 * i + 2);
            append_text(out, " 0 R");
        }
        append_text(out, " ]\n>>");
        end_object();

        // Per page: image XObject, page, contents.
        for (int i = 0; i < total_pages; ++i) {
            const PillowCImage* image = pages[static_cast<std::size_t>(i)].image;
            const int image_id = 3 * i + 1;
            const int page_id = 3 * i + 2;
            const int contents_id = 3 * i + 3;

            std::vector<std::uint8_t> jpeg_payload;
            std::wstring temp_wide;
            if (!pages[static_cast<std::size_t>(i)].is_palette) {
                temp_wide = build_temp_jpeg_path(tick, i);
                std::string temp_path;
                if (temp_wide.empty() || !wide_path_to_utf8(temp_wide, &temp_path)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
                const int jpeg_status = pillow_c_jpeg::save_jpeg_image(image, temp_path.c_str());
                if (jpeg_status != PILLOW_C_OK) {
                    DeleteFileW(temp_wide.c_str());
                    return jpeg_status;
                }
                if (!read_binary_file(temp_path.c_str(), &jpeg_payload)) {
                    DeleteFileW(temp_wide.c_str());
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }

            begin_object(image_id);
            append_text(out, "<<\n/Type /XObject\n/Subtype /Image\n/Width ");
            append_integer(image->width);
            append_text(out, "\n/Height ");
            append_integer(image->height);
            if (pages[static_cast<std::size_t>(i)].is_palette) {
                append_text(out, "\n/Filter /ASCIIHexDecode\n/BitsPerComponent 8\n/ColorSpace [ /Indexed /DeviceRGB 255 <");
                static constexpr char hex_digits_upper[] = "0123456789ABCDEF";
                const std::size_t stored = image->palette_rgb.size();
                for (int entry = 0; entry < 768; ++entry) {
                    const std::uint8_t value =
                        static_cast<std::size_t>(entry) < stored
                            ? image->palette_rgb[static_cast<std::size_t>(entry)]
                            : std::uint8_t{0};
                    out.push_back(static_cast<std::uint8_t>(hex_digits_upper[(value >> 4) & 15]));
                    out.push_back(static_cast<std::uint8_t>(hex_digits_upper[value & 15]));
                }
                const std::uint64_t hex_length =
                    static_cast<std::uint64_t>(image->width) * static_cast<std::uint64_t>(image->height) * 2u;
                append_text(out, "> ]\n/Length ");
                append_integer(static_cast<std::int64_t>(hex_length));
                append_text(out, "\n>>stream\n");
                static constexpr char hex_digits_lower[] = "0123456789abcdef";
                for (int y = 0; y < image->height; ++y) {
                    const std::uint8_t* row =
                        image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
                    for (int x = 0; x < image->width; ++x) {
                        const std::uint8_t value = row[x];
                        out.push_back(static_cast<std::uint8_t>(hex_digits_lower[(value >> 4) & 15]));
                        out.push_back(static_cast<std::uint8_t>(hex_digits_lower[value & 15]));
                    }
                }
                append_text(out, "\nendstream\n");
            } else {
                append_text(out, "\n/Filter /DCTDecode");
                if (image->mode == PILLOW_C_MODE_CMYK) {
                    append_text(out, "\n/Decode [ 1 0 1 0 1 0 1 0 ]");
                }
                append_text(out, "\n/BitsPerComponent 8\n/ColorSpace ");
                if (image->mode == PILLOW_C_MODE_L) {
                    append_text(out, "/DeviceGray");
                } else if (image->mode == PILLOW_C_MODE_CMYK) {
                    append_text(out, "/DeviceCMYK");
                } else {
                    append_text(out, "/DeviceRGB");
                }
                append_text(out, "\n/Length ");
                append_integer(static_cast<std::int64_t>(jpeg_payload.size()));
                append_text(out, "\n>>stream\n");
                out.insert(out.end(), jpeg_payload.begin(), jpeg_payload.end());
                append_text(out, "\nendstream\n");
            }
            end_object();

            // Page.
            const double media_width = static_cast<double>(image->width) * 72.0 / x_resolution;
            const double media_height = static_cast<double>(image->height) * 72.0 / y_resolution;
            begin_object(page_id);
            append_text(out, "<<\n/Resources <<\n/ProcSet [ /PDF /");
            if (pages[static_cast<std::size_t>(i)].procset == 0) {
                append_text(out, "ImageB");
            } else if (pages[static_cast<std::size_t>(i)].procset == 1) {
                append_text(out, "ImageC");
            } else {
                append_text(out, "ImageI");
            }
            append_text(out, " ]\n/XObject <<\n/image ");
            append_integer(image_id);
            append_text(out, " 0 R\n>>\n>>\n/MediaBox [ 0 0 ");
            append_text(out, python_float_repr(media_width).c_str());
            append_text(out, " ");
            append_text(out, python_float_repr(media_height).c_str());
            append_text(out, " ]\n/Contents ");
            append_integer(contents_id);
            append_text(out, " 0 R\n/Type /Page\n/Parent ");
            append_integer(pages_id);
            append_text(out, " 0 R\n>>");
            end_object();

            // Contents.
            char contents[128];
            const int contents_length = std::snprintf(
                contents, sizeof(contents), "q %f 0 0 %f 0 0 cm /image Do Q\n", media_width, media_height);
            if (contents_length <= 0 || contents_length >= static_cast<int>(sizeof(contents))) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            begin_object(contents_id);
            append_text(out, "<<\n/Length ");
            append_integer(contents_length);
            append_text(out, "\n>>stream\n");
            out.insert(out.end(), contents, contents + contents_length);
            append_text(out, "\nendstream\n");
            end_object();

            if (!temp_wide.empty()) {
                DeleteFileW(temp_wide.c_str());
            }
        }

        // Info object (Pillow's insertion order; only truthy entries).
        begin_object(info_id);
        append_text(out, "<<");
        if (title) {
            if (*title) {
                append_text(out, "\n/Title ");
                if (!append_pdf_string(out, title)) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
        } else {
            const std::string stem = title_stem_from_path(path);
            if (!stem.empty()) {
                append_text(out, "\n/Title ");
                if (!append_pdf_string(out, stem.c_str())) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
        }
        const char* string_entries[5][2] = {
            {"\n/Author ", author},
            {"\n/Subject ", subject},
            {"\n/Keywords ", keywords},
            {"\n/Creator ", creator},
            {"\n/Producer ", producer}};
        for (const auto& entry : string_entries) {
            if (entry[1] && *entry[1]) {
                append_text(out, entry[0]);
                if (!append_pdf_string(out, entry[1])) {
                    return PILLOW_C_INVALID_ARGUMENT;
                }
            }
        }
        if (creation_date && *creation_date) {
            append_text(out, "\n/CreationDate ");
            if (!append_pdf_string(out, creation_date)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        } else if (!creation_date) {
            append_text(out, "\n/CreationDate (");
            append_text(out, pdf_date_now().c_str());
            append_text(out, ")");
        }
        if (mod_date && *mod_date) {
            append_text(out, "\n/ModDate ");
            if (!append_pdf_string(out, mod_date)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
        } else if (!mod_date) {
            append_text(out, "\n/ModDate (");
            append_text(out, pdf_date_now().c_str());
            append_text(out, ")");
        }
        append_text(out, "\n>>");
        end_object();

        // Xref + trailer.
        const std::uint64_t xref_position = out.size();
        append_text(out, "xref\n0 ");
        append_integer(3 * total_pages + 4);
        append_text(out, "\n0000000000 65536 f \n");
        for (int object_id = 1; object_id <= 3 * total_pages + 3; ++object_id) {
            char entry[32];
            const int written = std::snprintf(
                entry, sizeof(entry), "%010llu %05d n \n",
                static_cast<unsigned long long>(offsets[static_cast<std::size_t>(object_id)]), 0);
            if (written > 0) {
                out.insert(out.end(), entry, entry + written);
            }
        }
        append_text(out, "trailer\n<<\n/Root ");
        append_integer(root_id);
        append_text(out, " 0 R\n/Size ");
        append_integer(3 * total_pages + 4);
        append_text(out, "\n/Info ");
        append_integer(info_id);
        append_text(out, " 0 R\n>>\nstartxref\n");
        append_integer(static_cast<std::int64_t>(xref_position));
        append_text(out, "\n%%EOF");

        return write_binary_file(path, out) ? PILLOW_C_OK : PILLOW_C_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

} // namespace

extern "C" __declspec(dllexport) int pillow_c_image_save_pdf(
    const PillowCImage* image,
    const char* path,
    double x_resolution,
    double y_resolution,
    const char* title,
    const char* author,
    const char* subject,
    const char* keywords,
    const char* creator,
    const char* producer,
    const char* creation_date,
    const char* mod_date)
{
    return save_pdf_document(&image, 1, path, x_resolution, y_resolution, title, author,
                             subject, keywords, creator, producer, creation_date, mod_date);
}

extern "C" __declspec(dllexport) int pillow_c_image_save_pdf_frames(
    const PillowCImage* const* images,
    int image_count,
    const char* path,
    double x_resolution,
    double y_resolution,
    const char* title,
    const char* author,
    const char* subject,
    const char* keywords,
    const char* creator,
    const char* producer,
    const char* creation_date,
    const char* mod_date)
{
    if (image_count <= 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return save_pdf_document(images, image_count, path, x_resolution, y_resolution, title, author,
                             subject, keywords, creator, producer, creation_date, mod_date);
}
