#include "pillow_c_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {
constexpr double PILLOW_C_PI = 3.1415926535897932384626433832795;
constexpr int PILLOW_C_FONT_DEFAULT = 1;
constexpr int PILLOW_C_TEXT_ALIGN_LEFT = 0;
constexpr int PILLOW_C_TEXT_ALIGN_CENTER = 1;
constexpr int PILLOW_C_TEXT_ALIGN_RIGHT = 2;
constexpr int PILLOW_C_TEXT_ALIGN_JUSTIFY = 3;
constexpr int PILLOW_C_DEFAULT_FONT_ASCENT = 10;
constexpr int PILLOW_C_DEFAULT_FONT_DESCENT = 3;

struct PillowCFont {
    int kind;
};

struct PolygonEdge {
    int x0;
    int y0;
    int xmin;
    int ymin;
    int xmax;
    int ymax;
    float dx;
};

struct QuarterState {
    std::int32_t a;
    std::int32_t b;
    std::int32_t cx;
    std::int32_t cy;
    std::int32_t ex;
    std::int32_t ey;
    std::int64_t a2;
    std::int64_t b2;
    std::int64_t a2b2;
    bool finished;
};

struct EllipseState {
    QuarterState outer;
    QuarterState inner;
    std::int32_t py;
    std::int32_t pl;
    std::int32_t pr;
    std::int32_t cy[4];
    std::int32_t cl[4];
    std::int32_t cr[4];
    int bufcnt;
    bool finished;
    bool leftmost;
};

enum class ClipNodeType {
    And,
    Or,
    Clip,
};

struct ClipNode {
    ClipNodeType type = ClipNodeType::Clip;
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    int left = -1;
    int right = -1;
};

struct ClipEvent {
    std::int32_t x;
    int type;
};

struct ClipEllipseState {
    EllipseState ellipse;
    int root = -1;
    ClipNode nodes[7];
    int node_count = 0;
    std::vector<ClipEvent> events;
    std::int32_t y = 0;
};

struct DefaultFontGlyph {
    unsigned char ch;
    int width;
    int height;
    int offset_x;
    int offset_y;
    int advance;
    int bbox_left;
    int bbox_top;
    int bbox_right;
    int bbox_bottom;
    const std::uint8_t* mask;
};

template <typename Func>
int with_detached_buffer_view(PillowCImage* image, Func func)
{
    const int status = pillow_c_detach_buffer_view_image(image);
    if (status != PILLOW_C_OK) {
        return status;
    }
    return func();
}

int normalize_coordinate(int value, int limit, int* out_value)
{
    return pillow_c_normalize_coordinate(value, limit, out_value);
}

int image_pixel_offset(const PillowCImage* image, int x, int y, std::size_t* out_offset)
{
    return pillow_c_image_pixel_offset(image, x, y, out_offset);
}

bool supported_bitmap_mask(const PillowCImage* mask)
{
    return pillow_c_supported_bitmap_mask(mask);
}

std::uint8_t mask_alpha_at(const PillowCImage* mask, const std::uint8_t* mask_row, int x)
{
    return pillow_c_mask_alpha_at(mask, mask_row, x);
}

std::uint32_t shift_for_div255(std::uint32_t value)
{
    return pillow_c_shift_for_div255(value);
}

std::int32_t read_i32_le(const std::uint8_t* data)
{
    return pillow_c_read_i32_le(data);
}

float read_f32_le(const std::uint8_t* data)
{
    return pillow_c_read_f32_le(data);
}
void fill_horizontal_span(
    PillowCImage* image,
    std::int64_t left,
    std::int64_t top,
    std::int64_t right_exclusive,
    const std::uint8_t* color)
{
    if (left < 0) {
        left = 0;
    }
    if (right_exclusive > image->width) {
        right_exclusive = image->width;
    }
    if (top < 0 || top >= image->height || right_exclusive <= left) {
        return;
    }

    const int clipped_left = static_cast<int>(left);
    const int clipped_top = static_cast<int>(top);
    const int clipped_right = static_cast<int>(right_exclusive);
    std::uint8_t* dst =
        image->pixels.data() +
        static_cast<std::size_t>(clipped_top) * image->stride +
        static_cast<std::size_t>(clipped_left) * image->channels;
    const std::size_t color_size = static_cast<std::size_t>(image->channels);
    const int pixel_count = clipped_right - clipped_left;
    if (image->channels == 1) {
        std::memset(dst, color[0], static_cast<std::size_t>(pixel_count));
        return;
    }

    std::memcpy(dst, color, color_size);
    std::size_t filled = color_size;
    const std::size_t total = static_cast<std::size_t>(pixel_count) * color_size;
    while (filled < total) {
        const std::size_t copy_size = std::min(filled, total - filled);
        std::memcpy(dst + filled, dst, copy_size);
        filled += copy_size;
    }
}

void fill_rectangle_region(
    PillowCImage* image,
    std::int64_t left,
    std::int64_t top,
    std::int64_t right_exclusive,
    std::int64_t bottom_exclusive,
    const std::uint8_t* color)
{
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right_exclusive > image->width) {
        right_exclusive = image->width;
    }
    if (bottom_exclusive > image->height) {
        bottom_exclusive = image->height;
    }
    if (right_exclusive <= left || bottom_exclusive <= top) {
        return;
    }

    const int clipped_left = static_cast<int>(left);
    const int clipped_top = static_cast<int>(top);
    const int clipped_right = static_cast<int>(right_exclusive);
    const int clipped_bottom = static_cast<int>(bottom_exclusive);

    fill_horizontal_span(image, clipped_left, clipped_top, clipped_right, color);
    if (image->channels == 1) {
        for (int y = clipped_top + 1; y < clipped_bottom; ++y) {
            std::uint8_t* dst = image->pixels.data() + static_cast<std::size_t>(y) * image->stride + clipped_left;
            std::memset(dst, color[0], static_cast<std::size_t>(clipped_right - clipped_left));
        }
        return;
    }

    const std::size_t row_bytes = static_cast<std::size_t>(clipped_right - clipped_left) * image->channels;
    const std::uint8_t* first_row =
        image->pixels.data() +
        static_cast<std::size_t>(clipped_top) * image->stride +
        static_cast<std::size_t>(clipped_left) * image->channels;
    for (int y = clipped_top + 1; y < clipped_bottom; ++y) {
        std::uint8_t* dst =
            image->pixels.data() +
            static_cast<std::size_t>(y) * image->stride +
            static_cast<std::size_t>(clipped_left) * image->channels;
        std::memcpy(dst, first_row, row_bytes);
    }
}

void fill_vertical_span(
    PillowCImage* image,
    std::int64_t x,
    std::int64_t top,
    std::int64_t bottom_exclusive,
    const std::uint8_t* color)
{
    if (x < 0 || x >= image->width) {
        return;
    }
    if (top < 0) {
        top = 0;
    }
    if (bottom_exclusive > image->height) {
        bottom_exclusive = image->height;
    }
    if (bottom_exclusive <= top) {
        return;
    }

    const int clipped_x = static_cast<int>(x);
    const int clipped_top = static_cast<int>(top);
    const int clipped_bottom = static_cast<int>(bottom_exclusive);
    const std::size_t color_size = static_cast<std::size_t>(image->channels);
    for (int y = clipped_top; y < clipped_bottom; ++y) {
        std::uint8_t* dst =
            image->pixels.data() +
            static_cast<std::size_t>(y) * image->stride +
            static_cast<std::size_t>(clipped_x) * image->channels;
        std::memcpy(dst, color, color_size);
    }
}

double color_diff_1norm(const std::uint8_t* left, const std::uint8_t* right, int channels)
{
    double diff = 0.0;
    for (int channel = 0; channel < channels; ++channel) {
        diff += std::abs(static_cast<int>(left[channel]) - static_cast<int>(right[channel]));
    }
    return diff;
}

double floodfill_color_diff_1norm(
    const PillowCImage* image,
    const std::uint8_t* left,
    const std::uint8_t* right)
{
    if (image->mode == PILLOW_C_MODE_I) {
        return std::abs(
            static_cast<double>(read_i32_le(left)) -
            static_cast<double>(read_i32_le(right)));
    }
    if (image->mode == PILLOW_C_MODE_F) {
        return std::abs(
            static_cast<double>(read_f32_le(left)) -
            static_cast<double>(read_f32_le(right)));
    }
    return color_diff_1norm(left, right, image->channels);
}

bool pixel_color_equal(const std::uint8_t* left, const std::uint8_t* right, int channels)
{
    return std::memcmp(left, right, static_cast<std::size_t>(channels)) == 0;
}

int draw_floodfill_image(
    PillowCImage* image,
    int seed_x,
    int seed_y,
    const std::uint8_t* value,
    std::size_t value_size,
    const std::uint8_t* border,
    std::size_t border_size,
    double thresh)
{
    if (!image || !value) {
        return PILLOW_C_NULL_POINTER;
    }
    const bool empty_value_noop = value_size == 0;
    if (empty_value_noop) {
        return PILLOW_C_OK;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if (value_size != channels || (border_size != 0 && border_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    const bool incomparable_border = border != nullptr && border_size == 0;
    if (image->width <= 0 || image->height <= 0) {
        return PILLOW_C_OK;
    }

    std::size_t seed_offset = 0;
    const int seed_status = image_pixel_offset(image, seed_x, seed_y, &seed_offset);
    if (seed_status == PILLOW_C_INVALID_ARGUMENT) {
        return PILLOW_C_OK;
    }
    if (seed_status != PILLOW_C_OK) {
        return seed_status;
    }

    try {
        std::vector<std::uint8_t> background(channels);
        std::memcpy(background.data(), image->pixels.data() + seed_offset, channels);
        if (floodfill_color_diff_1norm(image, value, background.data()) <= thresh) {
            return PILLOW_C_OK;
        }

        std::vector<std::uint8_t> seen(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height), 0);
        std::vector<int> edge;
        std::vector<int> next_edge;
        edge.reserve(64);
        next_edge.reserve(64);

        int normalized_seed_x = 0;
        int normalized_seed_y = 0;
        if (normalize_coordinate(seed_x, image->width, &normalized_seed_x) != PILLOW_C_OK ||
            normalize_coordinate(seed_y, image->height, &normalized_seed_y) != PILLOW_C_OK) {
            return PILLOW_C_OK;
        }

        std::memcpy(image->pixels.data() + seed_offset, value, channels);
        edge.push_back(normalized_seed_y * image->width + normalized_seed_x);

        while (!edge.empty()) {
            next_edge.clear();
            for (const int index : edge) {
                const int x = index % image->width;
                const int y = index / image->width;
                const int neighbors[4][2] = {
                    {x + 1, y},
                    {x - 1, y},
                    {x, y + 1},
                    {x, y - 1},
                };

                for (const auto& neighbor : neighbors) {
                    const int nx = neighbor[0];
                    const int ny = neighbor[1];
                    if (nx < 0 || ny < 0 || nx >= image->width || ny >= image->height) {
                        continue;
                    }
                    const int neighbor_index = ny * image->width + nx;
                    std::uint8_t& visited = seen[static_cast<std::size_t>(neighbor_index)];
                    if (visited) {
                        continue;
                    }
                    visited = 1;

                    std::uint8_t* pixel =
                        image->pixels.data() +
                        static_cast<std::size_t>(ny) * image->stride +
                        static_cast<std::size_t>(nx) * channels;
                    bool should_fill = false;
                    if (border) {
                        should_fill =
                            !pixel_color_equal(pixel, value, image->channels) &&
                            (incomparable_border || !pixel_color_equal(pixel, border, image->channels));
                    } else {
                        should_fill = floodfill_color_diff_1norm(image, pixel, background.data()) <= thresh;
                    }
                    if (should_fill) {
                        std::memcpy(pixel, value, channels);
                        next_edge.push_back(neighbor_index);
                    }
                }
            }
            edge.swap(next_edge);
        }
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

void quarter_init(QuarterState* state, std::int32_t a, std::int32_t b)
{
    if (a < 0 || b < 0) {
        state->finished = true;
        return;
    }
    state->a = a;
    state->b = b;
    state->cx = a;
    state->cy = b % 2;
    state->ex = a % 2;
    state->ey = b;
    state->a2 = static_cast<std::int64_t>(a) * a;
    state->b2 = static_cast<std::int64_t>(b) * b;
    state->a2b2 = state->a2 * state->b2;
    state->finished = false;
}

std::int64_t quarter_delta(const QuarterState* state, std::int64_t x, std::int64_t y)
{
    return std::llabs(state->a2 * y * y + state->b2 * x * x - state->a2b2);
}

bool quarter_next(QuarterState* state, std::int32_t* out_x, std::int32_t* out_y)
{
    if (state->finished) {
        return false;
    }
    *out_x = state->cx;
    *out_y = state->cy;
    if (state->cx == state->ex && state->cy == state->ey) {
        state->finished = true;
        return true;
    }

    std::int32_t nx = state->cx;
    std::int32_t ny = state->cy + 2;
    std::int64_t ndelta = quarter_delta(state, nx, ny);
    if (nx > 1) {
        std::int64_t new_delta = quarter_delta(state, state->cx - 2, state->cy + 2);
        if (ndelta > new_delta) {
            nx = state->cx - 2;
            ny = state->cy + 2;
            ndelta = new_delta;
        }
        new_delta = quarter_delta(state, state->cx - 2, state->cy);
        if (ndelta > new_delta) {
            nx = state->cx - 2;
            ny = state->cy;
        }
    }
    state->cx = nx;
    state->cy = ny;
    return true;
}

void ellipse_init(EllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width)
{
    state->bufcnt = 0;
    state->leftmost = (a % 2) != 0;
    quarter_init(&state->outer, a, b);
    if (width < 1 || !quarter_next(&state->outer, &state->pr, &state->py)) {
        state->finished = true;
        return;
    }
    state->finished = false;
    quarter_init(&state->inner, a - 2 * (width - 1), b - 2 * (width - 1));
    state->pl = state->leftmost ? 1 : 0;
}

bool ellipse_next(EllipseState* state, std::int32_t* out_x0, std::int32_t* out_y, std::int32_t* out_x1)
{
    if (state->bufcnt == 0) {
        if (state->finished) {
            return false;
        }

        const std::int32_t y = state->py;
        std::int32_t l = state->pl;
        const std::int32_t r = state->pr;
        std::int32_t cx = 0;
        std::int32_t cy = 0;
        bool has_next = false;

        while ((has_next = quarter_next(&state->outer, &cx, &cy)) && cy <= y) {
        }
        if (!has_next) {
            state->finished = true;
        } else {
            state->pr = cx;
            state->py = cy;
        }

        while ((has_next = quarter_next(&state->inner, &cx, &cy)) && cy <= y) {
            l = cx;
        }
        state->pl = has_next ? cx : (state->leftmost ? 1 : 0);

        if ((l > 0 || l < r) && y > 0) {
            state->cl[state->bufcnt] = l == 0 ? 2 : l;
            state->cy[state->bufcnt] = y;
            state->cr[state->bufcnt] = r;
            ++state->bufcnt;
        }
        if (y > 0) {
            state->cl[state->bufcnt] = -r;
            state->cy[state->bufcnt] = y;
            state->cr[state->bufcnt] = -l;
            ++state->bufcnt;
        }
        if (l > 0 || l < r) {
            state->cl[state->bufcnt] = l == 0 ? 2 : l;
            state->cy[state->bufcnt] = -y;
            state->cr[state->bufcnt] = r;
            ++state->bufcnt;
        }
        state->cl[state->bufcnt] = -r;
        state->cy[state->bufcnt] = -y;
        state->cr[state->bufcnt] = -l;
        ++state->bufcnt;
    }

    --state->bufcnt;
    *out_x0 = state->cl[state->bufcnt];
    *out_y = state->cy[state->bufcnt];
    *out_x1 = state->cr[state->bufcnt];
    return true;
}

void draw_ellipse_spans(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    int width)
{
    const int a = right - left;
    const int b = bottom - top;
    EllipseState state{};
    ellipse_init(&state, a, b, width);

    std::int32_t x0 = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    while (ellipse_next(&state, &x0, &y, &x1)) {
        fill_horizontal_span(
            image,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x0) + a) / 2,
            static_cast<std::int64_t>(top) + (static_cast<std::int64_t>(y) + b) / 2,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x1) + a) / 2 + 1,
            color);
    }
}

int clip_node(ClipEllipseState* state, ClipNodeType type)
{
    const int index = state->node_count++;
    state->nodes[index].type = type;
    state->nodes[index].a = 0.0;
    state->nodes[index].b = 0.0;
    state->nodes[index].c = 0.0;
    state->nodes[index].left = -1;
    state->nodes[index].right = -1;
    return index;
}

void clip_tree_transpose(ClipEllipseState* state, int node_index)
{
    if (node_index < 0) {
        return;
    }
    ClipNode& node = state->nodes[node_index];
    if (node.type == ClipNodeType::Clip) {
        std::swap(node.a, node.b);
    } else {
        clip_tree_transpose(state, node.left);
        clip_tree_transpose(state, node.right);
    }
}

bool clip_tree_do_clip(
    const ClipEllipseState* state,
    int node_index,
    std::int32_t x0,
    std::int32_t y,
    std::int32_t x1,
    std::vector<ClipEvent>* out_events)
{
    if (node_index < 0) {
        out_events->push_back({x0, 1});
        out_events->push_back({x1, -1});
        return true;
    }

    const ClipNode& node = state->nodes[node_index];
    if (node.type == ClipNodeType::Clip) {
        constexpr double eps = 1e-9;
        const double a = node.a;
        const double b = node.b;
        const double c = node.c;
        if (std::fabs(a) < eps) {
            if (b * y + c < -eps) {
                x0 = 1;
                x1 = 0;
            }
        } else {
            const double ix = -(b * y + c) / a;
            if (a * x0 + b * y + c < eps) {
                x0 = static_cast<std::int32_t>(std::lround(std::fmax(x0, ix)));
            }
            if (a * x1 + b * y + c < eps) {
                x1 = static_cast<std::int32_t>(std::lround(std::fmin(x1, ix)));
            }
        }
        if (x0 <= x1) {
            out_events->push_back({x0, 1});
            out_events->push_back({x1, -1});
        }
        return true;
    }

    std::vector<ClipEvent> left_events;
    std::vector<ClipEvent> right_events;
    try {
        left_events.reserve(8);
        right_events.reserve(8);
    } catch (const std::bad_alloc&) {
        return false;
    }
    if (!clip_tree_do_clip(state, node.left, x0, y, x1, &left_events) ||
        !clip_tree_do_clip(state, node.right, x0, y, x1, &right_events)) {
        return false;
    }

    std::size_t left_index = 0;
    std::size_t right_index = 0;
    int left_depth = 0;
    int right_depth = 0;
    bool has_tail = false;
    int tail_type = 0;

    while (left_index < left_events.size() || right_index < right_events.size()) {
        ClipEvent event{};
        if (right_index >= right_events.size() ||
            (left_index < left_events.size() &&
             (left_events[left_index].x < right_events[right_index].x ||
              (left_events[left_index].x == right_events[right_index].x &&
               left_events[left_index].type > right_events[right_index].type)))) {
            event = left_events[left_index++];
            left_depth += event.type;
        } else {
            event = right_events[right_index++];
            right_depth += event.type;
        }

        const bool take_or =
            node.type == ClipNodeType::Or &&
            ((event.type == 1 && (!has_tail || tail_type == -1)) ||
             (event.type == -1 && left_depth == 0 && right_depth == 0));
        const bool take_and =
            node.type == ClipNodeType::And &&
            ((event.type == 1 && (!has_tail || tail_type == -1) && left_depth > 0 && right_depth > 0) ||
             (event.type == -1 && has_tail && tail_type == 1 && (left_depth == 0 || right_depth == 0)));

        if (take_or || take_and) {
            out_events->push_back(event);
            has_tail = true;
            tail_type = event.type;
        }
    }

    return true;
}

void normalize_arc_angles(double* start, double* end)
{
    if (*end - *start >= 360.0) {
        *start = 0.0;
        *end = 360.0;
        return;
    }

    *start = std::fmod(*start < 0.0 ? 360.0 - std::fmod(-*start, 360.0) : *start, 360.0);
    *end = *start + std::fmod(*end < *start ? 360.0 - std::fmod(*start - *end, 360.0) : *end - *start, 360.0);
}

void arc_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    if (a < b) {
        arc_init(state, b, a, width, 90.0 - end, 90.0 - start);
        ellipse_init(&state->ellipse, a, b, width);
        clip_tree_transpose(state, state->root);
        return;
    }

    ellipse_init(&state->ellipse, a, b, width);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();
    normalize_arc_angles(&start, &end);

    if (end == start + 360.0) {
        return;
    }

    const int left_clip = clip_node(state, ClipNodeType::Clip);
    const int right_clip = clip_node(state, ClipNodeType::Clip);
    ClipNode& left = state->nodes[left_clip];
    ClipNode& right = state->nodes[right_clip];
    left.a = -a * std::sin(start * PILLOW_C_PI / 180.0);
    left.b = b * std::cos(start * PILLOW_C_PI / 180.0);
    left.c = (static_cast<double>(a) * a - static_cast<double>(b) * b) * std::sin(start * PILLOW_C_PI / 90.0) / 2.0;
    right.a = a * std::sin(end * PILLOW_C_PI / 180.0);
    right.b = -b * std::cos(end * PILLOW_C_PI / 180.0);
    right.c = (static_cast<double>(b) * b - static_cast<double>(a) * a) * std::sin(end * PILLOW_C_PI / 90.0) / 2.0;

    if (std::fmod(start, 180.0) == 0.0 || std::fmod(end, 180.0) == 0.0) {
        state->root = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
        state->nodes[state->root].left = left_clip;
        state->nodes[state->root].right = right_clip;
    } else if ((static_cast<int>(start / 180.0) + static_cast<int>(end / 180.0)) % 2 == 1) {
        state->root = clip_node(state, ClipNodeType::Or);
        const int left_and = clip_node(state, ClipNodeType::And);
        const int left_half = clip_node(state, ClipNodeType::Clip);
        const int right_and = clip_node(state, ClipNodeType::And);
        const int right_half = clip_node(state, ClipNodeType::Clip);
        state->nodes[state->root].left = left_and;
        state->nodes[state->root].right = right_and;
        state->nodes[left_and].left = left_half;
        state->nodes[left_and].right = left_clip;
        state->nodes[right_and].left = right_half;
        state->nodes[right_and].right = right_clip;
        state->nodes[left_half].b = static_cast<int>(start / 180.0) % 2 == 0 ? 1.0 : -1.0;
        state->nodes[right_half].b = static_cast<int>(end / 180.0) % 2 == 0 ? 1.0 : -1.0;
    } else {
        state->root = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
        const int combined = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
        const int half = clip_node(state, ClipNodeType::Clip);
        state->nodes[state->root].left = combined;
        state->nodes[state->root].right = half;
        state->nodes[combined].left = left_clip;
        state->nodes[combined].right = right_clip;
        state->nodes[half].b = end < 180.0 || end > 540.0 ? 1.0 : -1.0;
    }
}

void chord_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    ellipse_init(&state->ellipse, a, b, width);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double xl = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double xr = a * std::cos(end * PILLOW_C_PI / 180.0);
    const double yl = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double yr = b * std::sin(end * PILLOW_C_PI / 180.0);
    state->root = clip_node(state, ClipNodeType::Clip);
    ClipNode& root = state->nodes[state->root];
    root.a = yr - yl;
    root.b = xl - xr;
    root.c = -(root.a * xl + root.b * yl);
}

void chord_line_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    ellipse_init(&state->ellipse, a, b, a + b + 1);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double xl = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double xr = a * std::cos(end * PILLOW_C_PI / 180.0);
    const double yl = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double yr = b * std::sin(end * PILLOW_C_PI / 180.0);

    state->root = clip_node(state, ClipNodeType::And);
    const int left = clip_node(state, ClipNodeType::Clip);
    const int right = clip_node(state, ClipNodeType::Clip);
    state->nodes[state->root].left = left;
    state->nodes[state->root].right = right;

    ClipNode& left_node = state->nodes[left];
    left_node.a = yr - yl;
    left_node.b = xl - xr;
    left_node.c = -(left_node.a * xl + left_node.b * yl);

    ClipNode& right_node = state->nodes[right];
    right_node.a = -left_node.a;
    right_node.b = -left_node.b;
    right_node.c = 2.0 * width * std::sqrt(left_node.a * left_node.a + left_node.b * left_node.b) - left_node.c;
}

void pie_side_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double)
{
    ellipse_init(&state->ellipse, a, b, a + b + 1);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double x = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double y = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double side_a = -y;
    const double side_b = x;
    const double side_c = width * std::sqrt(side_a * side_a + side_b * side_b);

    state->root = clip_node(state, ClipNodeType::And);
    const int side_and = clip_node(state, ClipNodeType::And);
    const int left = clip_node(state, ClipNodeType::Clip);
    const int right = clip_node(state, ClipNodeType::Clip);
    const int half = clip_node(state, ClipNodeType::Clip);

    state->nodes[state->root].left = side_and;
    state->nodes[state->root].right = half;
    state->nodes[side_and].left = left;
    state->nodes[side_and].right = right;

    state->nodes[left].a = side_a;
    state->nodes[left].b = side_b;
    state->nodes[left].c = side_c;
    state->nodes[right].a = -side_a;
    state->nodes[right].b = -side_b;
    state->nodes[right].c = side_c;
    state->nodes[half].a = side_b;
    state->nodes[half].b = -side_a;
}

void pie_init(ClipEllipseState* state, std::int32_t a, std::int32_t b, std::int32_t width, double start, double end)
{
    ellipse_init(&state->ellipse, a, b, width);
    state->root = -1;
    state->node_count = 0;
    state->events.clear();

    const double xl = a * std::cos(start * PILLOW_C_PI / 180.0);
    const double xr = a * std::cos(end * PILLOW_C_PI / 180.0);
    const double yl = b * std::sin(start * PILLOW_C_PI / 180.0);
    const double yr = b * std::sin(end * PILLOW_C_PI / 180.0);

    const int left_clip = clip_node(state, ClipNodeType::Clip);
    const int right_clip = clip_node(state, ClipNodeType::Clip);
    state->nodes[left_clip].a = -yl;
    state->nodes[left_clip].b = xl;
    state->nodes[right_clip].a = yr;
    state->nodes[right_clip].b = -xr;

    state->root = clip_node(state, end - start < 180.0 ? ClipNodeType::And : ClipNodeType::Or);
    state->nodes[state->root].left = left_clip;
    state->nodes[state->root].right = right_clip;

    if (end - start < 90.0) {
        const int old_root = state->root;
        const int spike_clipper = clip_node(state, ClipNodeType::Clip);
        state->root = clip_node(state, ClipNodeType::And);
        state->nodes[state->root].left = old_root;
        state->nodes[state->root].right = spike_clipper;
        state->nodes[spike_clipper].a = (xl + xr) / 2.0;
        state->nodes[spike_clipper].b = (yl + yr) / 2.0;
    }
}

int clip_ellipse_next(ClipEllipseState* state, std::int32_t* out_x0, std::int32_t* out_y, std::int32_t* out_x1)
{
    std::int32_t x0 = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    while (state->events.empty() && ellipse_next(&state->ellipse, &x0, &y, &x1)) {
        try {
            if (!clip_tree_do_clip(state, state->root, x0, y, x1, &state->events)) {
                return PILLOW_C_ALLOCATION_FAILED;
            }
        } catch (const std::bad_alloc&) {
            return PILLOW_C_ALLOCATION_FAILED;
        }
        state->y = y;
    }

    if (!state->events.empty()) {
        *out_y = state->y;
        const ClipEvent left = state->events.front();
        state->events.erase(state->events.begin());
        const ClipEvent right = state->events.front();
        state->events.erase(state->events.begin());
        *out_x0 = left.x;
        *out_x1 = right.x;
        return PILLOW_C_OK;
    }

    return PILLOW_C_INVALID_LENGTH;
}

int round_up_away_from_zero(float value)
{
    return value >= 0.0f
        ? static_cast<int>(std::floor(value + 0.5f))
        : -static_cast<int>(std::floor(std::fabs(value) + 0.5f));
}

int round_down_toward_zero(float value)
{
    return value >= 0.0f
        ? static_cast<int>(std::ceil(value - 0.5f))
        : -static_cast<int>(std::ceil(std::fabs(value) - 0.5f));
}

void add_polygon_edge(PolygonEdge* edge, int x0, int y0, int x1, int y1)
{
    if (x0 <= x1) {
        edge->xmin = x0;
        edge->xmax = x1;
    } else {
        edge->xmin = x1;
        edge->xmax = x0;
    }

    if (y0 <= y1) {
        edge->ymin = y0;
        edge->ymax = y1;
    } else {
        edge->ymin = y1;
        edge->ymax = y0;
    }

    edge->dx = y0 == y1 ? 0.0f : static_cast<float>(x1 - x0) / static_cast<float>(y1 - y0);
    edge->x0 = x0;
    edge->y0 = y0;
}

bool polygon_mask_allows(const PillowCImage* mask, int x, int y)
{
    if (!mask) {
        return true;
    }
    if (x < 0 || x >= mask->width || y < 0 || y >= mask->height || mask->channels != 1) {
        return false;
    }
    const std::uint8_t* pixel =
        mask->pixels.data() +
        static_cast<std::size_t>(y) * mask->stride +
        static_cast<std::size_t>(x);
    return *pixel != 0;
}

void fill_horizontal_span_masked(
    PillowCImage* image,
    int left,
    int y,
    std::int64_t right_exclusive,
    const std::uint8_t* color,
    const PillowCImage* mask)
{
    if (!mask) {
        fill_horizontal_span(image, left, y, right_exclusive, color);
        return;
    }
    if (y < 0 || y >= image->height || right_exclusive <= 0 || left >= image->width) {
        return;
    }
    int clipped_left = left < 0 ? 0 : left;
    int clipped_right = right_exclusive > image->width ? image->width : static_cast<int>(right_exclusive);
    if (clipped_right <= clipped_left) {
        return;
    }

    std::uint8_t* dst =
        image->pixels.data() +
        static_cast<std::size_t>(y) * image->stride +
        static_cast<std::size_t>(clipped_left) * image->channels;
    for (int x = clipped_left; x < clipped_right; ++x) {
        if (polygon_mask_allows(mask, x, y)) {
            std::memcpy(dst, color, static_cast<std::size_t>(image->channels));
        }
        dst += image->channels;
    }
}

int fill_polygon_edges(
    PillowCImage* image,
    const std::vector<PolygonEdge>& edges,
    const std::uint8_t* color,
    const PillowCImage* mask = nullptr)
{
    if (edges.empty()) {
        return PILLOW_C_OK;
    }

    int ymin = image->height - 1;
    int ymax = 0;
    std::vector<const PolygonEdge*> edge_table;
    try {
        edge_table.reserve(edges.size());
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    for (const PolygonEdge& edge : edges) {
        ymin = std::min(ymin, edge.ymin);
        ymax = std::max(ymax, edge.ymax);
        if (edge.ymin == edge.ymax) {
            fill_horizontal_span_masked(image, edge.xmin, edge.ymin, static_cast<std::int64_t>(edge.xmax) + 1, color, mask);
            continue;
        }
        edge_table.push_back(&edge);
    }

    if (ymin < 0) {
        ymin = 0;
    }
    if (ymax > image->height) {
        ymax = image->height;
    }

    std::vector<float> intersections;
    try {
        intersections.resize(edge_table.size() * 2u);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    for (; ymin <= ymax; ++ymin) {
        std::size_t count = 0;
        for (std::size_t index = 0; index < edge_table.size(); ++index) {
            const PolygonEdge* current = edge_table[index];
            if (ymin < current->ymin || ymin > current->ymax) {
                continue;
            }

            intersections[count++] =
                static_cast<float>(ymin - current->y0) * current->dx +
                static_cast<float>(current->x0);

            if (ymin == current->ymax && ymin < ymax) {
                intersections[count] = intersections[count - 1u];
                ++count;
            } else if ((ymin == current->ymin || ymin == current->ymax) && current->dx != 0.0f) {
                for (std::size_t other_index = 0; other_index < index; ++other_index) {
                    const PolygonEdge* other = edge_table[other_index];
                    if ((ymin != other->ymin && ymin != other->ymax) || other->dx == 0.0f) {
                        continue;
                    }
                    const float other_x =
                        static_cast<float>(ymin - other->y0) * other->dx +
                        static_cast<float>(other->x0);
                    if (std::round(intersections[count - 1u]) != std::round(other_x)) {
                        continue;
                    }

                    const int offset = ymin == current->ymax ? -1 : 1;
                    const float adjacent_current =
                        static_cast<float>(ymin + offset - current->y0) * current->dx +
                        static_cast<float>(current->x0);
                    if (ymin + offset >= other->ymin && ymin + offset <= other->ymax) {
                        const float adjacent_other =
                            static_cast<float>(ymin + offset - other->y0) * other->dx +
                            static_cast<float>(other->x0);
                        if (intersections[count - 1u] > adjacent_current + 1.0f &&
                            intersections[count - 1u] > adjacent_other + 1.0f) {
                            intersections[count - 1u] = std::round(std::max(adjacent_current, adjacent_other)) + 1.0f;
                        } else if (intersections[count - 1u] < adjacent_current - 1.0f &&
                                   intersections[count - 1u] < adjacent_other - 1.0f) {
                            intersections[count - 1u] = std::round(std::min(adjacent_current, adjacent_other)) - 1.0f;
                        }
                        break;
                    }
                }
            }
        }

        std::sort(intersections.begin(), intersections.begin() + static_cast<std::ptrdiff_t>(count));
        for (std::size_t index = 1; index < count; index += 2u) {
            fill_horizontal_span_masked(
                image,
                round_up_away_from_zero(intersections[index - 1u]),
                ymin,
                static_cast<std::int64_t>(round_down_toward_zero(intersections[index])) + 1,
                color,
                mask);
        }
    }

    return PILLOW_C_OK;
}

void draw_point_image(PillowCImage* image, int x, int y, const std::uint8_t* color)
{
    if (x < 0 || x >= image->width || y < 0 || y >= image->height) {
        return;
    }
    std::uint8_t* dst =
        image->pixels.data() +
        static_cast<std::size_t>(y) * image->stride +
        static_cast<std::size_t>(x) * image->channels;
    std::memcpy(dst, color, static_cast<std::size_t>(image->channels));
}

int draw_pieslice_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width);

void draw_line_segment_image(
    PillowCImage* image,
    int x0,
    int y0,
    int x1,
    int y1,
    const std::uint8_t* color)
{
    int dx = x1 - x0;
    int xs = 1;
    if (dx < 0) {
        dx = -dx;
        xs = -1;
    }
    int dy = y1 - y0;
    int ys = 1;
    if (dy < 0) {
        dy = -dy;
        ys = -1;
    }

    if (dx == 0) {
        for (int i = 0; i < dy; ++i) {
            draw_point_image(image, x0, y0, color);
            y0 += ys;
        }
    } else if (dy == 0) {
        for (int i = 0; i < dx; ++i) {
            draw_point_image(image, x0, y0, color);
            x0 += xs;
        }
    } else if (dx > dy) {
        const int n = dx;
        dy += dy;
        int e = dy - dx;
        dx += dx;
        for (int i = 0; i < n; ++i) {
            draw_point_image(image, x0, y0, color);
            if (e >= 0) {
                y0 += ys;
                e -= dx;
            }
            e += dy;
            x0 += xs;
        }
    } else {
        const int n = dy;
        dx += dx;
        int e = dx - dy;
        dy += dy;
        for (int i = 0; i < n; ++i) {
            draw_point_image(image, x0, y0, color);
            if (e >= 0) {
                x0 += xs;
                e -= dy;
            }
            e += dx;
            y0 += ys;
        }
    }
}

int draw_wide_line_segment_image(
    PillowCImage* image,
    int x0,
    int y0,
    int x1,
    int y1,
    const std::uint8_t* color,
    int width,
    const PillowCImage* mask = nullptr)
{
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    if (dx == 0 && dy == 0) {
        if (polygon_mask_allows(mask, x0, y0)) {
            draw_point_image(image, x0, y0, color);
        }
        return PILLOW_C_OK;
    }

    const double big_hypotenuse = std::hypot(static_cast<double>(dx), static_cast<double>(dy));
    const double small_hypotenuse = (width - 1) / 2.0;
    const double ratio_max = static_cast<double>(round_up_away_from_zero(static_cast<float>(small_hypotenuse))) / big_hypotenuse;
    const double ratio_min = static_cast<double>(round_down_toward_zero(static_cast<float>(small_hypotenuse))) / big_hypotenuse;

    const int dxmin = round_down_toward_zero(static_cast<float>(ratio_min * dy));
    const int dxmax = round_down_toward_zero(static_cast<float>(ratio_max * dy));
    const int dymin = round_down_toward_zero(static_cast<float>(ratio_min * dx));
    const int dymax = round_down_toward_zero(static_cast<float>(ratio_max * dx));

    const int vertices[4][2] = {
        {x0 - dxmin, y0 + dymax},
        {x1 - dxmin, y1 + dymax},
        {x1 + dxmax, y1 - dymin},
        {x0 + dxmax, y0 - dymin},
    };

    std::vector<PolygonEdge> edges;
    try {
        edges.resize(4);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    add_polygon_edge(&edges[0], vertices[0][0], vertices[0][1], vertices[1][0], vertices[1][1]);
    add_polygon_edge(&edges[1], vertices[1][0], vertices[1][1], vertices[2][0], vertices[2][1]);
    add_polygon_edge(&edges[2], vertices[2][0], vertices[2][1], vertices[3][0], vertices[3][1]);
    add_polygon_edge(&edges[3], vertices[3][0], vertices[3][1], vertices[0][0], vertices[0][1]);
    return fill_polygon_edges(image, edges, color, mask);
}

int draw_line_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    if (!image || !points || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (point_count < 2 || point_count > static_cast<std::size_t>(INT_MAX)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (std::size_t index = 0; index + 1 < point_count; ++index) {
        const int* current = points + index * 2u;
        if (width <= 1) {
            draw_line_segment_image(image, current[0], current[1], current[2], current[3], color);
        } else {
            const int status = draw_wide_line_segment_image(image, current[0], current[1], current[2], current[3], color, width);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }
    }
    if (width <= 1) {
        const int* last = points + (point_count - 1u) * 2u;
        draw_point_image(image, last[0], last[1], color);
    }
    return PILLOW_C_OK;
}

double line_joint_angle_degrees(const int* start, const int* end)
{
    double angle =
        std::atan2(
            static_cast<double>(end[0] - start[0]),
            static_cast<double>(start[1] - end[1])) *
        180.0 / PILLOW_C_PI;
    angle = std::fmod(angle, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}

int line_joint_coordinate_delta(double delta)
{
    return static_cast<int>(delta > 0.0 ? std::floor(delta) : std::ceil(delta));
}

int line_joint_x_at_angle(int x, double angle, int width)
{
    angle -= 90.0;
    const double distance = static_cast<double>(width) / 2.0 - 1.0;
    const double delta = distance * std::cos(angle * PILLOW_C_PI / 180.0);
    return x + line_joint_coordinate_delta(delta);
}

int line_joint_y_at_angle(int y, double angle, int width)
{
    angle -= 90.0;
    const double distance = static_cast<double>(width) / 2.0 - 1.0;
    const double delta = distance * std::sin(angle * PILLOW_C_PI / 180.0);
    return y + line_joint_coordinate_delta(delta);
}

int draw_line_curve_joints_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    for (std::size_t index = 1; index + 1u < point_count; ++index) {
        const int* previous = points + (index - 1u) * 2u;
        const int* point = points + index * 2u;
        const int* next = points + (index + 1u) * 2u;

        const double previous_angle = line_joint_angle_degrees(previous, point);
        const double next_angle = line_joint_angle_degrees(point, next);
        if (previous_angle == next_angle) {
            continue;
        }

        const bool flipped =
            (next_angle > previous_angle && next_angle - 180.0 > previous_angle) ||
            (next_angle < previous_angle && next_angle + 180.0 > previous_angle);

        const double half_width = static_cast<double>(width) / 2.0;
        const int left = static_cast<int>(static_cast<double>(point[0]) - half_width + 1.0);
        const int top = static_cast<int>(static_cast<double>(point[1]) - half_width + 1.0);
        const int right = static_cast<int>(static_cast<double>(point[0]) + half_width - 1.0);
        const int bottom = static_cast<int>(static_cast<double>(point[1]) + half_width - 1.0);

        double start = 0.0;
        double end = 0.0;
        if (flipped) {
            start = next_angle + 90.0;
            end = previous_angle + 90.0;
        } else {
            start = previous_angle - 90.0;
            end = next_angle - 90.0;
        }

        int status = draw_pieslice_image(
            image,
            left,
            top,
            right,
            bottom,
            start - 90.0,
            end - 90.0,
            color,
            color_size,
            nullptr,
            0,
            1);
        if (status != PILLOW_C_OK) {
            return status;
        }

        if (width > 8) {
            int gap_points[6] = {};
            if (flipped) {
                gap_points[0] = line_joint_x_at_angle(point[0], previous_angle + 90.0, width);
                gap_points[1] = line_joint_y_at_angle(point[1], previous_angle + 90.0, width);
                gap_points[4] = line_joint_x_at_angle(point[0], next_angle + 90.0, width);
                gap_points[5] = line_joint_y_at_angle(point[1], next_angle + 90.0, width);
            } else {
                gap_points[0] = line_joint_x_at_angle(point[0], previous_angle - 90.0, width);
                gap_points[1] = line_joint_y_at_angle(point[1], previous_angle - 90.0, width);
                gap_points[4] = line_joint_x_at_angle(point[0], next_angle - 90.0, width);
                gap_points[5] = line_joint_y_at_angle(point[1], next_angle - 90.0, width);
            }
            gap_points[2] = point[0];
            gap_points[3] = point[1];

            status = draw_line_image(image, gap_points, 3, color, color_size, 3);
            if (status != PILLOW_C_OK) {
                return status;
            }
        }
    }

    return PILLOW_C_OK;
}

int draw_line_joint_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width,
    int joint_curve)
{
    const int status = draw_line_image(image, points, point_count, color, color_size, width);
    if (status != PILLOW_C_OK || !joint_curve || width <= 4 || point_count < 3 || !image || image->pixels.empty()) {
        return status;
    }
    return draw_line_curve_joints_image(image, points, point_count, color, color_size, width);
}

int draw_points_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !color || (!points && point_count != 0)) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (point_count > static_cast<std::size_t>(INT_MAX)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    for (std::size_t index = 0; index < point_count; ++index) {
        const int* current = points + index * 2u;
        draw_point_image(image, current[0], current[1], color);
    }
    return PILLOW_C_OK;
}

int build_polygon_edges(const int* points, std::size_t point_count, std::vector<PolygonEdge>* edges)
{
    try {
        edges->resize(point_count);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    std::size_t edge_count = 0;
    for (std::size_t index = 0; index + 1u < point_count; ++index) {
        const int* current = points + index * 2u;
        const int* next = current + 2;
        if (current[1] == next[1] && index != 0 && current[1] == points[index * 2u - 1u]) {
            PolygonEdge* last = &(*edges)[edge_count - 1u];
            if (next[0] > current[0] && current[0] > points[index * 2u - 2u]) {
                last->xmax = next[0];
                continue;
            }
            if (next[0] < current[0] && current[0] < points[index * 2u - 2u]) {
                last->xmin = next[0];
                continue;
            }
        }
        add_polygon_edge(&(*edges)[edge_count++], current[0], current[1], next[0], next[1]);
    }

    const int* last = points + (point_count - 1u) * 2u;
    if (last[0] != points[0] || last[1] != points[1]) {
        add_polygon_edge(&(*edges)[edge_count++], last[0], last[1], points[0], points[1]);
    }
    edges->resize(edge_count);
    return PILLOW_C_OK;
}

int draw_polygon_image(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image || !points) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (point_count < 2 || point_count > static_cast<std::size_t>(INT_MAX)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (outline_size != 0 && width > 1 && width > INT_MAX / 2) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    if (fill_size != 0) {
        std::vector<PolygonEdge> edges;
        const int edge_status = build_polygon_edges(points, point_count, &edges);
        if (edge_status != PILLOW_C_OK) {
            return edge_status;
        }
        const int fill_status = fill_polygon_edges(image, edges, fill);
        if (fill_status != PILLOW_C_OK) {
            return fill_status;
        }
    }

    if (outline_size != 0 && width != 0) {
        if (width <= 1) {
            for (std::size_t index = 0; index + 1u < point_count; ++index) {
                const int* current = points + index * 2u;
                draw_line_segment_image(image, current[0], current[1], current[2], current[3], outline);
            }
            const int* last = points + (point_count - 1u) * 2u;
            draw_line_segment_image(image, last[0], last[1], points[0], points[1], outline);
            draw_point_image(image, points[0], points[1], outline);
        } else {
            std::size_t mask_stride = 0;
            std::size_t mask_size = 0;
            if (!checked_image_size_allow_empty(image->width, image->height, 1, &mask_stride, &mask_size)) {
                return PILLOW_C_INVALID_ARGUMENT;
            }

            try {
                PillowCImage mask{
                    image->width,
                    image->height,
                    PILLOW_C_MODE_1,
                    1,
                    mask_stride,
                    std::vector<std::uint8_t>(mask_size)};
                std::vector<PolygonEdge> mask_edges;
                const int edge_status = build_polygon_edges(points, point_count, &mask_edges);
                if (edge_status != PILLOW_C_OK) {
                    return edge_status;
                }
                const std::uint8_t mask_value = 255;
                const int mask_status = fill_polygon_edges(&mask, mask_edges, &mask_value);
                if (mask_status != PILLOW_C_OK) {
                    return mask_status;
                }

                const int wide_width = width * 2 - 1;
                for (std::size_t index = 0; index + 1u < point_count; ++index) {
                    const int* current = points + index * 2u;
                    const int status = draw_wide_line_segment_image(
                        image,
                        current[0],
                        current[1],
                        current[2],
                        current[3],
                        outline,
                        wide_width,
                        &mask);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                }
                const int* last = points + (point_count - 1u) * 2u;
                const int status = draw_wide_line_segment_image(
                    image,
                    last[0],
                    last[1],
                    points[0],
                    points[1],
                    outline,
                    wide_width,
                    &mask);
                if (status != PILLOW_C_OK) {
                    return status;
                }
            } catch (const std::bad_alloc&) {
                return PILLOW_C_ALLOCATION_FAILED;
            }
        }
    }

    return PILLOW_C_OK;
}

int draw_rectangle_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::int64_t x0 = left;
    const std::int64_t y0 = top;
    const std::int64_t x1 = right;
    const std::int64_t y1 = bottom;
    if (fill_size != 0) {
        fill_rectangle_region(image, x0, y0, x1 + 1, y1 + 1, fill);
    }
    if (outline_size == 0 || width <= 0) {
        return PILLOW_C_OK;
    }

    for (int inset = 0; inset < width; ++inset) {
        fill_horizontal_span(image, x0, y0 + inset, x1 + 1, outline);
        fill_horizontal_span(image, x0, y1 - inset, x1 + 1, outline);
        fill_vertical_span(image, x1 - inset, y0 + width, y1 - width + 2, outline);
        fill_vertical_span(image, x0 + inset, y0 + width, y1 - width + 2, outline);
    }
    return PILLOW_C_OK;
}

int draw_ellipse_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const int span_width = right - left;
    const int span_height = bottom - top;
    if (fill_size != 0) {
        draw_ellipse_spans(image, left, top, right, bottom, fill, span_width + span_height);
    }
    if (outline_size != 0 && width != 0) {
        draw_ellipse_spans(image, left, top, right, bottom, outline, width);
    }
    return PILLOW_C_OK;
}

int draw_clip_ellipse_spans(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color,
    void (*init)(ClipEllipseState*, std::int32_t, std::int32_t, std::int32_t, double, double),
    int width,
    double start,
    double end)
{
    const int a = right - left;
    const int b = bottom - top;
    ClipEllipseState state{};
    try {
        state.events.reserve(8);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
    init(&state, a, b, width, start, end);

    std::int32_t x0 = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    while (true) {
        const int next_status = clip_ellipse_next(&state, &x0, &y, &x1);
        if (next_status == PILLOW_C_INVALID_LENGTH) {
            return PILLOW_C_OK;
        }
        if (next_status != PILLOW_C_OK) {
            return next_status;
        }
        fill_horizontal_span(
            image,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x0) + a) / 2,
            static_cast<std::int64_t>(top) + (static_cast<std::int64_t>(y) + b) / 2,
            static_cast<std::int64_t>(left) + (static_cast<std::int64_t>(x1) + a) / 2 + 1,
            color);
    }
}

int draw_arc_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(start) || !std::isfinite(end)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty() || width <= 0) {
        return PILLOW_C_OK;
    }

    normalize_arc_angles(&start, &end);
    if (start + 360.0 == end) {
        return draw_ellipse_image(image, left, top, right, bottom, nullptr, 0, color, color_size, width);
    }
    if (start == end) {
        return PILLOW_C_OK;
    }

    return draw_clip_ellipse_spans(image, left, top, right, bottom, color, arc_init, width, start, end);
}

int draw_chord_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(start) || !std::isfinite(end)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    normalize_arc_angles(&start, &end);
    if (start + 360.0 == end) {
        return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    }
    if (start == end) {
        return PILLOW_C_OK;
    }

    if (fill_size != 0) {
        const int fill_width = right - left + bottom - top + 1;
        const int status = draw_clip_ellipse_spans(image, left, top, right, bottom, fill, chord_init, fill_width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    if (outline_size != 0 && width != 0) {
        int status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, chord_line_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, chord_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    return PILLOW_C_OK;
}

int draw_pieslice_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(start) || !std::isfinite(end)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    normalize_arc_angles(&start, &end);
    if (start + 360.0 == end) {
        return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    }
    if (start == end) {
        return PILLOW_C_OK;
    }

    if (fill_size != 0) {
        const int fill_width = right + bottom - left - top;
        const int status = draw_clip_ellipse_spans(image, left, top, right, bottom, fill, pie_init, fill_width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    if (outline_size != 0 && width != 0) {
        int status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, pie_side_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, pie_side_init, width, end, 0.0);
        if (status != PILLOW_C_OK) {
            return status;
        }
        const int center_left = static_cast<int>(std::lround((left + right - width) / 2.0));
        const int center_top = static_cast<int>(std::lround((top + bottom - width) / 2.0));
        status = draw_ellipse_image(
            image,
            center_left,
            center_top,
            center_left + width - 1,
            center_top + width - 1,
            outline,
            outline_size,
            nullptr,
            0,
            0);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_clip_ellipse_spans(image, left, top, right, bottom, outline, pie_init, width, start, end);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    return PILLOW_C_OK;
}

void fill_rounded_rectangle_bar(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* color)
{
    fill_rectangle_region(
        image,
        left,
        top,
        static_cast<std::int64_t>(right) + 1,
        static_cast<std::int64_t>(bottom) + 1,
        color);
}

int draw_rounded_rectangle_part(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* color,
    std::size_t color_size,
    int width,
    bool fill)
{
    return fill
        ? draw_pieslice_image(image, left, top, right, bottom, start, end, color, color_size, nullptr, 0, 1)
        : draw_arc_image(image, left, top, right, bottom, start, end, color, color_size, width);
}

bool rounded_rectangle_colors_match(
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size)
{
    return fill_size != 0 &&
        outline_size == fill_size &&
        std::memcmp(fill, outline, fill_size) == 0;
}

int draw_rounded_rectangle_image(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double radius,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width,
    int corners_mask)
{
    if (!image) {
        return PILLOW_C_NULL_POINTER;
    }
    if ((fill_size > 0 && !fill) || (outline_size > 0 && !outline)) {
        return PILLOW_C_NULL_POINTER;
    }
    const std::size_t channels = static_cast<std::size_t>(image->channels);
    if ((fill_size != 0 && fill_size != channels) || (outline_size != 0 && outline_size != channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if ((fill && fill_size == 0) || (outline && outline_size == 0)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (right < left || bottom < top || !std::isfinite(radius) || radius < 0.0 || corners_mask < 0 || corners_mask > 15) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const bool corners[4] = {
        (corners_mask & 1) != 0,
        (corners_mask & 2) != 0,
        (corners_mask & 4) != 0,
        (corners_mask & 8) != 0,
    };
    const bool any_corners = corners_mask != 0;
    const bool all_corners = corners_mask == 15;

    double diameter = radius * 2.0;
    bool full_x = false;
    bool full_y = false;
    if (all_corners) {
        full_x = diameter >= static_cast<double>(right - left - 1);
        if (full_x) {
            diameter = static_cast<double>(right - left);
        }
        full_y = diameter >= static_cast<double>(bottom - top - 1);
        if (full_y) {
            diameter = static_cast<double>(bottom - top);
        }
        if (full_x && full_y) {
            return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
        }
    }

    if (diameter == 0.0 || !any_corners) {
        return draw_rectangle_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    }

    const int d = static_cast<int>(std::floor(diameter));
    const int r = static_cast<int>(std::floor(diameter / 2.0));

    if (fill_size != 0) {
        if (full_x) {
            int status = draw_rounded_rectangle_part(
                image, left, top, left + d, top + d, 180.0, 360.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            status = draw_rounded_rectangle_part(
                image, left, bottom - d, left + d, bottom, 0.0, 180.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            fill_rounded_rectangle_bar(image, left, top + r + 1, right, bottom - r - 1, fill);
        } else if (full_y) {
            int status = draw_rounded_rectangle_part(
                image, left, top, left + d, top + d, 90.0, 270.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            status = draw_rounded_rectangle_part(
                image, right - d, top, right, top + d, 270.0, 90.0, fill, fill_size, 1, true);
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (right - r - 1 > left + r + 1) {
                fill_rounded_rectangle_bar(image, left + r + 1, top, right - r - 1, bottom, fill);
            }
        } else {
            int status = PILLOW_C_OK;
            if (corners[0]) {
                status = draw_rounded_rectangle_part(
                    image, left, top, left + d, top + d, 180.0, 270.0, fill, fill_size, 1, true);
            }
            if (status == PILLOW_C_OK && corners[1]) {
                status = draw_rounded_rectangle_part(
                    image, right - d, top, right, top + d, 270.0, 360.0, fill, fill_size, 1, true);
            }
            if (status == PILLOW_C_OK && corners[2]) {
                status = draw_rounded_rectangle_part(
                    image, right - d, bottom - d, right, bottom, 0.0, 90.0, fill, fill_size, 1, true);
            }
            if (status == PILLOW_C_OK && corners[3]) {
                status = draw_rounded_rectangle_part(
                    image, left, bottom - d, left + d, bottom, 90.0, 180.0, fill, fill_size, 1, true);
            }
            if (status != PILLOW_C_OK) {
                return status;
            }
            if (right - r - 1 > left + r + 1) {
                fill_rounded_rectangle_bar(image, left + r + 1, top, right - r - 1, bottom, fill);
            }

            int side_top = top;
            int side_bottom = bottom;
            if (corners[0]) {
                side_top += r + 1;
            }
            if (corners[3]) {
                side_bottom -= r + 1;
            }
            fill_rounded_rectangle_bar(image, left, side_top, left + r, side_bottom, fill);

            side_top = top;
            side_bottom = bottom;
            if (corners[1]) {
                side_top += r + 1;
            }
            if (corners[2]) {
                side_bottom -= r + 1;
            }
            fill_rounded_rectangle_bar(image, right - r, side_top, right, side_bottom, fill);
        }
    }

    if (outline_size == 0 || width == 0 || rounded_rectangle_colors_match(fill, fill_size, outline, outline_size)) {
        return PILLOW_C_OK;
    }

    if (full_x) {
        int status = draw_rounded_rectangle_part(
            image, left, top, left + d, top + d, 180.0, 360.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_rounded_rectangle_part(
            image, left, bottom - d, left + d, bottom, 0.0, 180.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
    } else if (full_y) {
        int status = draw_rounded_rectangle_part(
            image, left, top, left + d, top + d, 90.0, 270.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
        status = draw_rounded_rectangle_part(
            image, right - d, top, right, top + d, 270.0, 90.0, outline, outline_size, width, false);
        if (status != PILLOW_C_OK) {
            return status;
        }
    } else {
        int status = PILLOW_C_OK;
        if (corners[0]) {
            status = draw_rounded_rectangle_part(
                image, left, top, left + d, top + d, 180.0, 270.0, outline, outline_size, width, false);
        }
        if (status == PILLOW_C_OK && corners[1]) {
            status = draw_rounded_rectangle_part(
                image, right - d, top, right, top + d, 270.0, 360.0, outline, outline_size, width, false);
        }
        if (status == PILLOW_C_OK && corners[2]) {
            status = draw_rounded_rectangle_part(
                image, right - d, bottom - d, right, bottom, 0.0, 90.0, outline, outline_size, width, false);
        }
        if (status == PILLOW_C_OK && corners[3]) {
            status = draw_rounded_rectangle_part(
                image, left, bottom - d, left + d, bottom, 90.0, 180.0, outline, outline_size, width, false);
        }
        if (status != PILLOW_C_OK) {
            return status;
        }
    }

    if (!full_x) {
        int edge_left = left;
        int edge_right = right;
        if (corners[0]) {
            edge_left += r + 1;
        }
        if (corners[1]) {
            edge_right -= r + 1;
        }
        fill_rounded_rectangle_bar(image, edge_left, top, edge_right, top + width - 1, outline);

        edge_left = left;
        edge_right = right;
        if (corners[3]) {
            edge_left += r + 1;
        }
        if (corners[2]) {
            edge_right -= r + 1;
        }
        fill_rounded_rectangle_bar(image, edge_left, bottom - width + 1, edge_right, bottom, outline);
    }
    if (!full_y) {
        int edge_top = top;
        int edge_bottom = bottom;
        if (corners[0]) {
            edge_top += r + 1;
        }
        if (corners[3]) {
            edge_bottom -= r + 1;
        }
        fill_rounded_rectangle_bar(image, left, edge_top, left + width - 1, edge_bottom, outline);

        edge_top = top;
        edge_bottom = bottom;
        if (corners[1]) {
            edge_top += r + 1;
        }
        if (corners[2]) {
            edge_bottom -= r + 1;
        }
        fill_rounded_rectangle_bar(image, right - width + 1, edge_top, right, edge_bottom, outline);
    }

    return PILLOW_C_OK;
}

int draw_bitmap_image(
    PillowCImage* image,
    int left,
    int top,
    const PillowCImage* mask,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !mask || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!supported_bitmap_mask(mask)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (image->pixels.empty() || mask->pixels.empty()) {
        return PILLOW_C_OK;
    }

    const std::int64_t dst_left_i64 = left < 0 ? 0 : left;
    const std::int64_t dst_top_i64 = top < 0 ? 0 : top;
    const std::int64_t mask_right_i64 = static_cast<std::int64_t>(left) + mask->width;
    const std::int64_t mask_bottom_i64 = static_cast<std::int64_t>(top) + mask->height;
    const std::int64_t dst_right_i64 = mask_right_i64 > image->width ? image->width : mask_right_i64;
    const std::int64_t dst_bottom_i64 = mask_bottom_i64 > image->height ? image->height : mask_bottom_i64;
    if (dst_right_i64 <= dst_left_i64 || dst_bottom_i64 <= dst_top_i64) {
        return PILLOW_C_OK;
    }

    const int dst_left = static_cast<int>(dst_left_i64);
    const int dst_top = static_cast<int>(dst_top_i64);
    const int dst_right = static_cast<int>(dst_right_i64);
    const int dst_bottom = static_cast<int>(dst_bottom_i64);
    const int src_left = dst_left - left;
    const int src_top = dst_top - top;
    const int channels = image->channels;

    for (int y = 0; y < dst_bottom - dst_top; ++y) {
        const std::uint8_t* mask_row =
            mask->pixels.data() +
            static_cast<std::size_t>(src_top + y) * mask->stride +
            static_cast<std::size_t>(src_left) * mask->channels;
        std::uint8_t* dst_row =
            image->pixels.data() +
            static_cast<std::size_t>(dst_top + y) * image->stride +
            static_cast<std::size_t>(dst_left) * image->channels;

        for (int x = 0; x < dst_right - dst_left; ++x) {
            const std::uint8_t alpha = mask_alpha_at(mask, mask_row, x);
            if (alpha == 0) {
                continue;
            }

            const std::size_t pixel_offset = static_cast<std::size_t>(x) * channels;
            if (alpha == 255) {
                std::memcpy(dst_row + pixel_offset, color, color_size);
                continue;
            }

            for (int channel = 0; channel < channels; ++channel) {
                const std::uint8_t dst = dst_row[pixel_offset + channel];
                const std::uint8_t src = color[channel];
                const std::uint32_t blended =
                    static_cast<std::uint32_t>(dst) * (255u - alpha) +
                    static_cast<std::uint32_t>(src) * alpha +
                    128u;
                dst_row[pixel_offset + channel] = static_cast<std::uint8_t>(shift_for_div255(blended));
            }
        }
    }

    return PILLOW_C_OK;
}

// Generated from local Pillow 11.3.0 ImageFont.load_default() masks for printable ASCII.
static constexpr std::uint8_t DEFAULT_FONT_MASK_DATA[] = {
    0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0,
    240, 0, 0, 11, 0, 0, 184, 0, 0, 240, 240, 0, 240, 240, 0, 202,
    202, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 123, 30, 145, 0, 0, 0, 149, 34, 115, 0, 0, 0, 149, 70,
    77, 0, 0, 160, 215, 201, 173, 15, 0, 36, 112, 147, 0, 0, 57, 214,
    207, 229, 153, 0, 0, 137, 27, 133, 0, 0, 0, 149, 68, 80, 0, 0,
    0, 0, 0, 120, 0, 0, 0, 0, 14, 124, 250, 123, 7, 0, 0, 181,
    107, 241, 121, 145, 0, 0, 236, 12, 240, 10, 106, 0, 0, 171, 170, 245,
    20, 0, 0, 0, 6, 109, 251, 232, 84, 0, 0, 0, 0, 240, 43, 225,
    0, 0, 192, 6, 240, 16, 219, 0, 0, 82, 193, 250, 195, 68, 0, 0,
    0, 0, 240, 0, 0, 0, 120, 212, 117, 0, 2, 181, 16, 231, 14, 231,
    0, 102, 98, 0, 226, 36, 225, 18, 181, 1, 0, 93, 200, 90, 150, 52,
    0, 0, 0, 0, 51, 150, 86, 181, 84, 0, 1, 181, 19, 224, 24, 222,
    0, 99, 104, 0, 233, 25, 232, 16, 184, 2, 0, 120, 213, 117, 0, 48,
    184, 189, 193, 39, 0, 0, 214, 39, 0, 0, 0, 0, 0, 211, 43, 0,
    0, 221, 0, 0, 45, 236, 213, 192, 252, 177, 0, 172, 111, 1, 0, 240,
    0, 0, 237, 3, 0, 0, 240, 0, 0, 208, 59, 0, 0, 240, 0, 0,
    53, 192, 192, 190, 202, 0, 240, 240, 202, 0, 0, 0, 0, 0, 0, 0,
    75, 5, 0, 23, 165, 0, 0, 122, 90, 0, 0, 194, 33, 0, 0, 232,
    3, 0, 0, 237, 0, 0, 0, 218, 13, 0, 0, 162, 57, 0, 0, 72,
    128, 0, 0, 1, 165, 5, 5, 73, 0, 0, 0, 165, 23, 0, 0, 91,
    121, 0, 0, 35, 193, 0, 0, 4, 231, 0, 0, 0, 237, 0, 0, 14,
    218, 0, 0, 57, 162, 0, 0, 129, 71, 0, 5, 163, 0, 0, 0, 0,
    0, 240, 0, 0, 0, 0, 90, 68, 240, 68, 88, 0, 0, 9, 118, 232,
    115, 8, 0, 0, 10, 176, 21, 175, 10, 0, 0, 1, 25, 0, 25, 1,
    0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0,
    150, 192, 252, 192, 150, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0,
    240, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 38, 0, 45, 191,
    0, 138, 80, 0, 132, 192, 81, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    12, 0, 186, 0, 0, 0, 0, 157, 2, 0, 0, 7, 151, 0, 0, 0,
    72, 86, 0, 0, 0, 143, 15, 0, 0, 0, 157, 0, 0, 0, 42, 115,
    0, 0, 0, 118, 39, 0, 0, 0, 156, 0, 0, 0, 0, 76, 0, 0,
    0, 15, 176, 196, 176, 15, 0, 138, 114, 0, 115, 136, 0, 212, 26, 0,
    27, 211, 0, 235, 2, 0, 2, 234, 0, 235, 2, 0, 2, 234, 0, 212,
    26, 0, 27, 211, 0, 138, 114, 0, 115, 137, 0, 15, 176, 196, 176, 15,
    0, 0, 16, 160, 241, 0, 0, 0, 195, 109, 240, 0, 0, 0, 24, 0,
    240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0,
    0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0,
    0, 0, 98, 195, 208, 70, 0, 21, 193, 2, 40, 215, 0, 40, 85, 0,
    11, 234, 0, 0, 0, 0, 95, 157, 0, 0, 0, 35, 212, 21, 0, 0,
    12, 204, 55, 0, 0, 1, 173, 89, 0, 0, 0, 97, 244, 192, 192, 180,
    0, 10, 169, 192, 205, 75, 0, 118, 116, 0, 25, 224, 0, 17, 6, 0,
    68, 219, 0, 0, 0, 163, 245, 67, 0, 0, 0, 0, 79, 162, 0, 147,
    4, 0, 3, 237, 0, 162, 84, 0, 61, 198, 0, 30, 191, 193, 187, 40,
    0, 0, 0, 0, 74, 247, 0, 0, 0, 13, 198, 242, 0, 0, 0, 149,
    86, 240, 0, 0, 60, 175, 0, 240, 0, 7, 200, 26, 0, 240, 0, 90,
    213, 192, 192, 252, 162, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 69, 222, 192, 192, 132, 0, 93, 104, 0, 0, 0, 0, 118, 79, 0,
    0, 0, 0, 143, 154, 196, 178, 35, 0, 142, 107, 0, 82, 190, 0, 32,
    3, 0, 3, 235, 0, 168, 79, 0, 58, 190, 0, 37, 196, 192, 184, 35,
    0, 2, 146, 199, 194, 34, 0, 106, 133, 0, 81, 174, 0, 197, 36, 0,
    6, 118, 0, 235, 114, 192, 175, 32, 0, 241, 83, 0, 83, 189, 0, 218,
    3, 0, 3, 235, 0, 152, 65, 0, 65, 188, 0, 22, 179, 191, 187, 35,
    0, 168, 192, 192, 198, 236, 0, 0, 0, 0, 100, 130, 0, 0, 0, 1,
    207, 22, 0, 0, 0, 78, 154, 0, 0, 0, 0, 193, 39, 0, 0, 0,
    55, 177, 0, 0, 0, 0, 173, 62, 0, 0, 0, 36, 199, 0, 0, 0,
    0, 67, 198, 191, 195, 57, 0, 224, 33, 0, 35, 214, 0, 205, 52, 0,
    54, 225, 0, 53, 248, 211, 248, 76, 0, 193, 83, 0, 85, 158, 0, 238,
    2, 0, 3, 236, 0, 199, 61, 0, 60, 204, 0, 47, 193, 193, 192, 48,
    0, 33, 185, 192, 180, 23, 0, 187, 67, 0, 67, 151, 0, 235, 3, 0,
    3, 217, 0, 189, 81, 0, 84, 240, 0, 33, 175, 191, 115, 234, 0, 122,
    6, 0, 37, 196, 0, 174, 80, 0, 133, 107, 0, 36, 196, 199, 147, 2,
    0, 185, 0, 13, 0, 0, 0, 0, 0, 12, 0, 186, 0, 0, 185, 0,
    0, 13, 0, 0, 0, 0, 0, 0, 0, 6, 147, 0, 65, 125, 0, 134,
    63, 0, 0, 0, 0, 0, 60, 0, 0, 0, 37, 161, 129, 0, 0, 140,
    142, 22, 0, 0, 0, 170, 115, 7, 0, 0, 0, 0, 74, 182, 96, 0,
    0, 0, 0, 3, 102, 0, 39, 192, 192, 192, 192, 0, 0, 0, 0, 0,
    0, 0, 39, 192, 192, 192, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 61, 0, 0, 0, 0, 0, 133, 163, 37, 0, 0,
    0, 0, 26, 149, 142, 0, 0, 0, 5, 107, 168, 0, 0, 88, 176, 74,
    0, 0, 0, 101, 3, 0, 0, 0, 60, 200, 205, 76, 206, 38, 25, 218,
    96, 0, 6, 231, 0, 0, 52, 178, 0, 0, 131, 73, 0, 0, 168, 1,
    0, 0, 24, 0, 0, 0, 186, 0, 0, 0, 0, 89, 182, 196, 184, 63,
    0, 0, 0, 0, 158, 162, 19, 0, 43, 214, 52, 0, 0, 90, 169, 16,
    169, 187, 145, 78, 182, 0, 0, 193, 51, 156, 98, 37, 220, 11, 230, 0,
    0, 235, 3, 228, 8, 44, 167, 5, 225, 0, 0, 236, 14, 228, 11, 139,
    122, 81, 153, 0, 0, 184, 80, 92, 169, 104, 171, 144, 13, 0, 0, 58,
    220, 56, 0, 5, 84, 66, 0, 0, 0, 0, 54, 178, 201, 188, 103, 3,
    0, 0, 0, 0, 100, 249, 23, 0, 0, 0, 0, 177, 139, 97, 0, 0,
    0, 10, 188, 39, 173, 0, 0, 0, 81, 116, 0, 205, 6, 0, 0, 160,
    208, 200, 231, 69, 0, 3, 198, 0, 0, 71, 145, 0, 62, 151, 0, 0,
    10, 215, 0, 141, 85, 0, 0, 0, 193, 41, 0, 246, 192, 193, 207, 77,
    0, 240, 0, 0, 47, 225, 0, 240, 0, 0, 1, 221, 0, 240, 0, 0,
    67, 77, 0, 246, 192, 199, 252, 186, 0, 240, 0, 0, 45, 240, 0, 240,
    0, 0, 26, 205, 0, 246, 192, 190, 190, 52, 0, 101, 206, 193, 181, 29,
    0, 69, 190, 9, 0, 73, 189, 0, 185, 56, 0, 0, 0, 154, 5, 228,
    7, 0, 0, 0, 0, 0, 231, 7, 0, 0, 0, 0, 0, 193, 55, 0,
    0, 1, 161, 0, 87, 187, 7, 0, 89, 168, 0, 0, 124, 212, 191, 169,
    20, 0, 0, 246, 192, 191, 197, 96, 0, 0, 240, 0, 0, 11, 189, 85,
    0, 240, 0, 0, 0, 51, 196, 0, 240, 0, 0, 0, 5, 232, 0, 240,
    0, 0, 0, 9, 227, 0, 240, 0, 0, 0, 63, 182, 0, 240, 0, 0,
    17, 200, 64, 0, 246, 192, 191, 196, 81, 0, 0, 246, 192, 192, 192, 114,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 0, 246, 192, 192, 192, 57, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 246, 192, 192, 192, 135, 0, 246, 192, 192, 192, 114,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 0, 246, 192, 192, 192, 39, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 102, 202, 198, 182, 28,
    0, 69, 183, 5, 0, 119, 185, 0, 185, 53, 0, 0, 13, 176, 3, 228,
    7, 0, 0, 0, 0, 0, 232, 7, 0, 114, 192, 216, 0, 195, 53, 0,
    0, 16, 247, 0, 93, 183, 5, 0, 123, 244, 0, 1, 134, 215, 189, 97,
    240, 0, 0, 240, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 240, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 246, 192, 192, 192, 192, 246, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 240, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0,
    0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    240, 0, 0, 0, 0, 0, 240, 0, 0, 194, 0, 0, 240, 0, 0, 213,
    29, 35, 216, 0, 0, 75, 205, 204, 74, 0, 0, 240, 0, 0, 29, 207,
    24, 0, 240, 0, 8, 196, 56, 0, 0, 240, 0, 160, 100, 0, 0, 0,
    240, 110, 152, 0, 0, 0, 0, 246, 217, 176, 0, 0, 0, 0, 243, 14,
    187, 104, 0, 0, 0, 240, 0, 28, 229, 41, 0, 0, 240, 0, 0, 89,
    208, 7, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 246, 196, 196,
    196, 137, 0, 247, 170, 0, 0, 0, 164, 247, 0, 0, 240, 209, 3, 0,
    1, 199, 240, 0, 0, 240, 159, 56, 0, 49, 153, 240, 0, 0, 240, 88,
    127, 0, 120, 82, 240, 0, 0, 240, 18, 195, 0, 186, 14, 240, 0, 0,
    240, 0, 193, 29, 186, 0, 240, 0, 0, 240, 0, 121, 162, 118, 0, 240,
    0, 0, 240, 0, 46, 254, 44, 0, 240, 0, 0, 247, 191, 0, 0, 0,
    240, 0, 0, 240, 195, 64, 0, 0, 240, 0, 0, 240, 66, 193, 0, 0,
    240, 0, 0, 240, 0, 193, 65, 0, 240, 0, 0, 240, 0, 64, 194, 0,
    240, 0, 0, 240, 0, 0, 191, 66, 240, 0, 0, 240, 0, 0, 62, 195,
    240, 0, 0, 240, 0, 0, 0, 189, 247, 0, 0, 97, 205, 199, 205, 95,
    0, 71, 196, 16, 0, 16, 196, 68, 188, 57, 0, 0, 0, 58, 187, 229,
    8, 0, 0, 0, 8, 229, 229, 8, 0, 0, 0, 9, 228, 187, 58, 0,
    0, 0, 59, 185, 70, 196, 15, 0, 16, 196, 67, 0, 98, 206, 199, 205,
    96, 0, 0, 246, 192, 191, 196, 53, 0, 240, 0, 0, 45, 203, 0, 240,
    0, 0, 0, 236, 0, 240, 0, 0, 67, 188, 0, 246, 192, 193, 178, 32,
    0, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 0, 97, 205, 199, 204, 91, 0, 71, 196, 16, 0, 16, 196, 62,
    188, 57, 0, 0, 0, 58, 182, 229, 8, 0, 0, 0, 8, 225, 229, 8,
    0, 0, 0, 9, 237, 186, 58, 0, 0, 0, 59, 195, 67, 196, 15, 0,
    16, 195, 75, 0, 95, 204, 201, 242, 242, 109, 0, 0, 0, 0, 0, 20,
    64, 0, 246, 192, 193, 206, 63, 0, 0, 240, 0, 0, 45, 210, 0, 0,
    240, 0, 0, 1, 237, 0, 0, 240, 0, 0, 71, 171, 0, 0, 246, 192,
    197, 237, 24, 0, 0, 240, 0, 0, 118, 120, 0, 0, 240, 0, 0, 49,
    174, 0, 0, 240, 0, 0, 10, 220, 2, 0, 50, 186, 191, 187, 29, 0,
    208, 28, 0, 83, 169, 0, 230, 36, 0, 5, 61, 0, 96, 231, 157, 65,
    0, 0, 0, 17, 97, 209, 128, 21, 58, 0, 0, 21, 232, 20, 228, 29,
    0, 40, 202, 0, 74, 200, 191, 188, 44, 81, 192, 192, 252, 192, 192, 84,
    0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 240, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0,
    240, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0, 240, 0,
    0, 0, 240, 0, 0, 238, 3, 0, 4, 236, 0, 0, 185, 72, 0, 75,
    183, 0, 0, 38, 193, 193, 193, 38, 0, 138, 99, 0, 0, 0, 186, 47,
    62, 172, 0, 0, 12, 216, 0, 3, 221, 4, 0, 80, 144, 0, 0, 165,
    60, 0, 155, 65, 0, 0, 88, 133, 0, 211, 4, 0, 0, 16, 200, 49,
    162, 0, 0, 0, 0, 190, 147, 82, 0, 0, 0, 0, 114, 243, 11, 0,
    0, 160, 85, 0, 0, 209, 143, 0, 0, 144, 84, 102, 138, 0, 13, 205,
    198, 0, 0, 199, 23, 44, 190, 0, 69, 131, 208, 7, 5, 209, 0, 1,
    224, 1, 127, 70, 157, 56, 53, 156, 0, 0, 184, 39, 181, 13, 99, 112,
    108, 94, 0, 0, 126, 93, 190, 0, 41, 168, 163, 33, 0, 0, 68, 188,
    146, 0, 1, 205, 190, 0, 0, 0, 12, 251, 86, 0, 0, 180, 165, 0,
    0, 71, 174, 0, 0, 43, 202, 3, 0, 180, 62, 0, 186, 56, 0, 0,
    39, 199, 83, 155, 0, 0, 0, 0, 144, 226, 19, 0, 0, 0, 0, 158,
    222, 31, 0, 0, 0, 59, 173, 69, 177, 0, 0, 4, 202, 34, 0, 173,
    79, 0, 114, 134, 0, 0, 30, 215, 11, 42, 211, 2, 0, 0, 204, 44,
    0, 167, 88, 0, 75, 169, 0, 0, 41, 211, 2, 196, 42, 0, 0, 0,
    165, 155, 166, 0, 0, 0, 0, 39, 253, 39, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 141, 192, 192, 195, 250, 0, 0, 0, 0, 0, 111, 135, 0, 0,
    0, 0, 26, 209, 10, 0, 0, 0, 0, 169, 75, 0, 0, 0, 0, 71,
    172, 0, 0, 0, 0, 8, 207, 28, 0, 0, 0, 0, 131, 114, 0, 0,
    0, 0, 0, 249, 195, 192, 192, 192, 12, 0, 216, 192, 3, 0, 240, 0,
    0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0,
    0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 216, 192,
    3, 1, 154, 0, 0, 0, 0, 146, 10, 0, 0, 0, 80, 77, 0, 0,
    0, 12, 145, 0, 0, 0, 0, 157, 0, 0, 0, 0, 110, 48, 0, 0,
    0, 35, 123, 0, 0, 0, 0, 159, 0, 0, 0, 0, 77, 2, 3, 192,
    216, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0,
    240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0,
    240, 0, 3, 192, 216, 0, 0, 0, 68, 42, 0, 0, 0, 0, 158, 142,
    0, 0, 0, 57, 103, 142, 15, 0, 0, 151, 17, 59, 105, 0, 13, 162,
    0, 0, 171, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    96, 192, 192, 192, 96, 52, 119, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 43, 196, 212, 113, 0, 151,
    61, 18, 230, 0, 13, 101, 137, 244, 0, 184, 101, 28, 242, 0, 236, 15,
    36, 249, 0, 130, 213, 166, 233, 7, 0, 240, 0, 0, 0, 0, 0, 240,
    0, 0, 0, 0, 0, 240, 139, 195, 194, 33, 0, 247, 87, 0, 92, 175,
    0, 246, 6, 0, 7, 232, 0, 244, 3, 0, 7, 225, 0, 248, 77, 0,
    92, 160, 0, 240, 160, 194, 178, 21, 20, 174, 192, 186, 22, 161, 89, 0,
    97, 151, 228, 6, 0, 3, 15, 231, 7, 0, 0, 0, 177, 86, 0, 100,
    135, 35, 192, 193, 178, 16, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    240, 0, 23, 178, 195, 160, 240, 0, 163, 91, 0, 80, 248, 0, 227, 7,
    0, 4, 245, 0, 233, 7, 0, 6, 246, 0, 175, 86, 0, 87, 247, 0,
    33, 194, 194, 136, 240, 0, 17, 164, 188, 194, 54, 0, 156, 55, 0, 30,
    208, 0, 227, 192, 192, 192, 219, 3, 231, 8, 0, 2, 62, 0, 172, 89,
    0, 75, 183, 0, 29, 183, 193, 189, 34, 0, 0, 40, 119, 15, 0, 215,
    110, 11, 0, 239, 0, 0, 105, 252, 192, 12, 0, 240, 0, 0, 0, 240,
    0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 23, 178,
    195, 163, 236, 0, 163, 91, 0, 80, 248, 0, 227, 7, 0, 4, 245, 0,
    233, 7, 0, 6, 246, 0, 175, 86, 0, 87, 247, 0, 33, 194, 194, 137,
    236, 0, 160, 46, 0, 53, 194, 0, 51, 194, 185, 191, 44, 0, 0, 240,
    0, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0, 0, 240, 133, 193,
    211, 81, 0, 0, 247, 113, 0, 38, 226, 0, 0, 248, 16, 0, 0, 241,
    0, 0, 242, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0,
    240, 0, 0, 0, 240, 0, 43, 105, 0, 0, 0, 0, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 54, 95,
    0, 0, 0, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0,
    0, 240, 0, 0, 240, 0, 3, 239, 0, 169, 165, 0, 0, 240, 0, 0,
    0, 0, 0, 240, 0, 0, 0, 0, 0, 240, 0, 22, 210, 43, 0, 240,
    6, 190, 66, 0, 0, 240, 155, 99, 0, 0, 0, 247, 199, 155, 0, 0,
    0, 240, 2, 187, 101, 0, 0, 240, 0, 21, 221, 54, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 203, 121, 0, 240, 158, 221, 104, 164, 222, 113, 0, 0, 248, 55,
    30, 254, 54, 31, 230, 0, 0, 246, 3, 0, 248, 2, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 133, 193, 209, 81,
    0, 0, 246, 113, 0, 34, 226, 0, 0, 248, 16, 0, 0, 241, 0, 0,
    242, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0, 240, 0,
    0, 0, 240, 0, 24, 182, 194, 182, 24, 165, 93, 0, 93, 164, 229, 7,
    0, 7, 229, 230, 7, 0, 7, 229, 165, 91, 0, 91, 165, 26, 183, 194,
    183, 25, 0, 240, 139, 195, 194, 33, 0, 247, 87, 0, 92, 175, 0, 246,
    6, 0, 7, 232, 0, 244, 3, 0, 7, 225, 0, 248, 77, 0, 92, 160,
    0, 240, 160, 194, 178, 21, 0, 240, 0, 0, 0, 0, 0, 240, 0, 0,
    0, 0, 23, 178, 195, 160, 240, 0, 163, 91, 0, 80, 248, 0, 227, 7,
    0, 4, 245, 0, 233, 7, 0, 6, 246, 0, 175, 86, 0, 87, 247, 0,
    33, 194, 194, 136, 240, 0, 0, 0, 0, 0, 240, 0, 0, 0, 0, 0,
    240, 0, 0, 240, 159, 110, 0, 247, 71, 0, 0, 244, 2, 0, 0, 240,
    0, 0, 0, 240, 0, 0, 0, 240, 0, 0, 0, 109, 207, 206, 70, 0,
    234, 13, 40, 162, 0, 160, 175, 69, 0, 0, 0, 62, 174, 159, 15, 193,
    5, 11, 233, 0, 117, 205, 207, 108, 0, 120, 0, 0, 240, 0, 129, 252,
    186, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 242, 2, 0, 181, 180,
    0, 240, 0, 0, 0, 240, 0, 0, 240, 0, 0, 0, 240, 0, 0, 240,
    0, 0, 0, 242, 0, 0, 242, 0, 0, 16, 248, 0, 0, 227, 43, 0,
    107, 247, 0, 0, 81, 213, 191, 137, 240, 0, 208, 27, 0, 7, 218, 2,
    124, 105, 0, 72, 146, 0, 39, 184, 0, 149, 60, 0, 0, 203, 14, 198,
    1, 0, 0, 124, 134, 144, 0, 0, 0, 39, 249, 58, 0, 0, 217, 31,
    0, 185, 145, 0, 64, 167, 145, 93, 2, 187, 193, 0, 126, 94, 73, 155,
    46, 138, 185, 13, 188, 23, 9, 208, 104, 78, 129, 77, 195, 0, 0, 184,
    186, 19, 69, 191, 132, 0, 0, 112, 213, 0, 13, 252, 60, 0, 0, 174,
    80, 0, 154, 93, 0, 28, 207, 53, 183, 0, 0, 0, 114, 230, 33, 0,
    0, 0, 126, 226, 45, 0, 0, 38, 194, 46, 193, 1, 0, 190, 58, 0,
    150, 100, 207, 36, 0, 14, 219, 1, 118, 116, 0, 87, 137, 0, 29, 194,
    0, 166, 48, 0, 0, 192, 27, 198, 0, 0, 0, 105, 169, 125, 0, 0,
    0, 20, 250, 36, 0, 0, 0, 29, 188, 0, 0, 0, 78, 207, 49, 0,
    0, 0, 0, 150, 192, 196, 251, 0, 0, 0, 0, 124, 129, 0, 0, 0,
    45, 202, 5, 0, 0, 4, 200, 48, 0, 0, 0, 125, 128, 0, 0, 0,
    0, 250, 197, 192, 192, 9, 0, 145, 127, 0, 240, 4, 0, 240, 0, 0,
    240, 0, 75, 171, 0, 74, 176, 0, 0, 240, 0, 0, 240, 0, 0, 241,
    3, 0, 145, 126, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0,
    0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0, 240, 0, 0,
    240, 0, 0, 240, 0, 127, 143, 0, 4, 240, 0, 0, 240, 0, 0, 240,
    0, 0, 170, 75, 0, 166, 74, 0, 240, 0, 0, 240, 0, 3, 240, 0,
    126, 143, 0, 0, 137, 190, 79, 45, 101, 0, 132, 14, 136, 189, 33, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const DefaultFontGlyph* default_font_glyph(unsigned char ch)
{
    static constexpr DefaultFontGlyph glyphs[] = {
        {32, 2, 0, 0, 10, 2, 0, 10, 2, 10, DEFAULT_FONT_MASK_DATA + 0},
        {33, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 0},
        {34, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 24},
        {35, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 48},
        {36, 7, 10, 0, 1, 7, 0, 1, 7, 11, DEFAULT_FONT_MASK_DATA + 96},
        {37, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 166},
        {38, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 222},
        {39, 1, 8, 0, 2, 1, 0, 2, 1, 10, DEFAULT_FONT_MASK_DATA + 278},
        {40, 4, 10, 0, 1, 4, 0, 1, 4, 11, DEFAULT_FONT_MASK_DATA + 286},
        {41, 4, 10, -1, 1, 3, -1, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 326},
        {42, 7, 5, 0, 5, 7, 0, 5, 7, 10, DEFAULT_FONT_MASK_DATA + 366},
        {43, 7, 6, 0, 4, 7, 0, 4, 7, 10, DEFAULT_FONT_MASK_DATA + 401},
        {44, 3, 3, -1, 8, 2, -1, 8, 2, 11, DEFAULT_FONT_MASK_DATA + 443},
        {45, 3, 4, 0, 6, 3, 0, 6, 3, 10, DEFAULT_FONT_MASK_DATA + 452},
        {46, 2, 2, 0, 8, 2, 0, 8, 2, 10, DEFAULT_FONT_MASK_DATA + 464},
        {47, 5, 9, -1, 2, 3, -1, 2, 4, 11, DEFAULT_FONT_MASK_DATA + 468},
        {48, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 513},
        {49, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 561},
        {50, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 609},
        {51, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 657},
        {52, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 705},
        {53, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 753},
        {54, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 801},
        {55, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 849},
        {56, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 897},
        {57, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 945},
        {58, 2, 6, 0, 4, 2, 0, 4, 2, 10, DEFAULT_FONT_MASK_DATA + 993},
        {59, 3, 7, -1, 4, 2, -1, 4, 2, 11, DEFAULT_FONT_MASK_DATA + 1005},
        {60, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 1026},
        {61, 6, 5, 0, 5, 6, 0, 5, 6, 10, DEFAULT_FONT_MASK_DATA + 1062},
        {62, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 1092},
        {63, 4, 8, 0, 2, 4, 0, 2, 4, 10, DEFAULT_FONT_MASK_DATA + 1128},
        {64, 10, 9, 0, 2, 10, 0, 2, 10, 11, DEFAULT_FONT_MASK_DATA + 1160},
        {65, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1250},
        {66, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1306},
        {67, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1354},
        {68, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1410},
        {69, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1466},
        {70, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1514},
        {71, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1562},
        {72, 8, 8, 0, 2, 8, 0, 2, 8, 10, DEFAULT_FONT_MASK_DATA + 1618},
        {73, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 1682},
        {74, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1706},
        {75, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1754},
        {76, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 1810},
        {77, 9, 8, 0, 2, 9, 0, 2, 9, 10, DEFAULT_FONT_MASK_DATA + 1858},
        {78, 8, 8, 0, 2, 8, 0, 2, 8, 10, DEFAULT_FONT_MASK_DATA + 1930},
        {79, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 1994},
        {80, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2050},
        {81, 7, 9, 0, 2, 7, 0, 2, 7, 11, DEFAULT_FONT_MASK_DATA + 2098},
        {82, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2161},
        {83, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2217},
        {84, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2265},
        {85, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2321},
        {86, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2377},
        {87, 10, 8, 0, 2, 10, 0, 2, 10, 10, DEFAULT_FONT_MASK_DATA + 2433},
        {88, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2513},
        {89, 7, 8, 0, 2, 6, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2569},
        {90, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 2625},
        {91, 4, 10, 0, 1, 3, 0, 1, 4, 11, DEFAULT_FONT_MASK_DATA + 2681},
        {92, 5, 9, -1, 2, 3, -1, 2, 4, 11, DEFAULT_FONT_MASK_DATA + 2721},
        {93, 4, 10, -1, 1, 3, -1, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 2766},
        {94, 6, 7, 0, 3, 6, 0, 3, 6, 10, DEFAULT_FONT_MASK_DATA + 2806},
        {95, 5, 1, 0, 10, 5, 0, 10, 5, 11, DEFAULT_FONT_MASK_DATA + 2848},
        {96, 3, 7, 0, 3, 3, 0, 3, 3, 10, DEFAULT_FONT_MASK_DATA + 2853},
        {97, 5, 6, 0, 4, 5, 0, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 2874},
        {98, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2904},
        {99, 5, 6, 0, 4, 5, 0, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 2952},
        {100, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 2982},
        {101, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 3030},
        {102, 4, 9, 0, 1, 3, 0, 1, 4, 10, DEFAULT_FONT_MASK_DATA + 3066},
        {103, 6, 8, 0, 4, 6, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3102},
        {104, 7, 8, 0, 2, 7, 0, 2, 7, 10, DEFAULT_FONT_MASK_DATA + 3150},
        {105, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 3206},
        {106, 3, 10, 0, 2, 3, 0, 2, 3, 12, DEFAULT_FONT_MASK_DATA + 3230},
        {107, 6, 8, 0, 2, 6, 0, 2, 6, 10, DEFAULT_FONT_MASK_DATA + 3260},
        {108, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 3308},
        {109, 9, 6, 0, 4, 9, 0, 4, 9, 10, DEFAULT_FONT_MASK_DATA + 3332},
        {110, 7, 6, 0, 4, 7, 0, 4, 7, 10, DEFAULT_FONT_MASK_DATA + 3386},
        {111, 5, 6, 0, 4, 5, 0, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 3428},
        {112, 6, 8, 0, 4, 6, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3458},
        {113, 6, 8, 0, 4, 6, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3506},
        {114, 4, 6, 0, 4, 4, 0, 4, 4, 10, DEFAULT_FONT_MASK_DATA + 3554},
        {115, 5, 6, -1, 4, 4, -1, 4, 4, 10, DEFAULT_FONT_MASK_DATA + 3578},
        {116, 3, 8, 0, 2, 3, 0, 2, 3, 10, DEFAULT_FONT_MASK_DATA + 3608},
        {117, 7, 6, 0, 4, 7, 0, 4, 7, 10, DEFAULT_FONT_MASK_DATA + 3632},
        {118, 6, 6, 0, 4, 5, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 3674},
        {119, 8, 6, 0, 4, 8, 0, 4, 8, 10, DEFAULT_FONT_MASK_DATA + 3710},
        {120, 6, 6, -1, 4, 5, -1, 4, 5, 10, DEFAULT_FONT_MASK_DATA + 3758},
        {121, 6, 8, 0, 4, 5, 0, 4, 6, 12, DEFAULT_FONT_MASK_DATA + 3794},
        {122, 6, 6, 0, 4, 6, 0, 4, 6, 10, DEFAULT_FONT_MASK_DATA + 3842},
        {123, 3, 10, 0, 1, 3, 0, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 3878},
        {124, 3, 11, 0, 1, 3, 0, 1, 3, 12, DEFAULT_FONT_MASK_DATA + 3908},
        {125, 3, 10, 0, 1, 3, 0, 1, 3, 11, DEFAULT_FONT_MASK_DATA + 3941},
        {126, 6, 4, 0, 6, 6, 0, 6, 6, 10, DEFAULT_FONT_MASK_DATA + 3971},
    };
    static_assert(sizeof(glyphs) / sizeof(glyphs[0]) == 95, "default font glyph table must cover printable ASCII");
    if (ch < 32u || ch > 126u) {
        return nullptr;
    }
    return &glyphs[ch - 32u];
}

int default_font_text_metrics_span(
    const char* text,
    std::size_t text_length,
    int* out_length,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!text || !out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    int cursor = 0;
    bool has_bbox = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    for (std::size_t index = 0; index < text_length; ++index) {
        const auto ch = static_cast<unsigned char>(text[index]);
        if (ch < 32u || ch > 126u) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        const DefaultFontGlyph* glyph = default_font_glyph(ch);
        if (!glyph) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        if (!has_bbox) {
            left = cursor + glyph->bbox_left;
            top = glyph->bbox_top;
            right = cursor + glyph->bbox_right;
            bottom = glyph->bbox_bottom;
            has_bbox = true;
        } else {
            left = std::min(left, cursor + glyph->bbox_left);
            top = std::min(top, glyph->bbox_top);
            right = std::max(right, cursor + glyph->bbox_right);
            bottom = std::max(bottom, glyph->bbox_bottom);
        }
        cursor += glyph->advance;
    }

    *out_length = cursor;
    if (out_left) {
        *out_left = has_bbox ? left : 0;
    }
    if (out_top) {
        *out_top = has_bbox ? top : 0;
    }
    if (out_right) {
        *out_right = has_bbox ? right : 0;
    }
    if (out_bottom) {
        *out_bottom = has_bbox ? bottom : 0;
    }
    return PILLOW_C_OK;
}

int default_font_text_metrics(
    const char* text,
    int* out_length,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!text) {
        return PILLOW_C_NULL_POINTER;
    }
    return default_font_text_metrics_span(
        text,
        std::strlen(text),
        out_length,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int font_text_metrics(
    const PillowCFont* font,
    const char* text,
    int* out_length,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_text_metrics(text, out_length, out_left, out_top, out_right, out_bottom);
}

struct DefaultFontLineMetrics {
    std::size_t start;
    std::size_t length;
    int advance;
    int left;
    int top;
    int right;
    int bottom;
};

bool valid_text_align(int align)
{
    return align == PILLOW_C_TEXT_ALIGN_LEFT ||
        align == PILLOW_C_TEXT_ALIGN_CENTER ||
        align == PILLOW_C_TEXT_ALIGN_RIGHT ||
        align == PILLOW_C_TEXT_ALIGN_JUSTIFY;
}

int collect_default_font_multiline_metrics(
    const char* text,
    std::vector<DefaultFontLineMetrics>* out_lines,
    int* out_max_width)
{
    if (!text || !out_lines || !out_max_width) {
        return PILLOW_C_NULL_POINTER;
    }

    out_lines->clear();
    *out_max_width = 0;
    const std::size_t total_length = std::strlen(text);
    std::size_t line_start = 0;
    while (line_start <= total_length) {
        std::size_t line_end = line_start;
        while (line_end < total_length && text[line_end] != '\n') {
            ++line_end;
        }

        DefaultFontLineMetrics line{};
        line.start = line_start;
        line.length = line_end - line_start;
        const int status = default_font_text_metrics_span(
            text + line.start,
            line.length,
            &line.advance,
            &line.left,
            &line.top,
            &line.right,
            &line.bottom);
        if (status != PILLOW_C_OK) {
            return status;
        }
        *out_max_width = std::max(*out_max_width, line.advance);
        out_lines->push_back(line);

        if (line_end == total_length) {
            break;
        }
        line_start = line_end + 1;
    }

    return PILLOW_C_OK;
}

double default_font_multiline_align_offset(int max_width, int line_width, int align)
{
    const int width_difference = max_width - line_width;
    if (align == PILLOW_C_TEXT_ALIGN_CENTER) {
        return static_cast<double>(width_difference) / 2.0;
    }
    if (align == PILLOW_C_TEXT_ALIGN_RIGHT) {
        return static_cast<double>(width_difference);
    }
    return 0.0;
}

struct DefaultFontJustifyWordMetrics {
    std::size_t start;
    std::size_t length;
    int advance;
    int left;
    int top;
    int right;
    int bottom;
};

int collect_default_font_justify_words(
    const char* text,
    const DefaultFontLineMetrics& line,
    std::vector<DefaultFontJustifyWordMetrics>* out_words,
    int* out_word_width_sum)
{
    if (!text || !out_words || !out_word_width_sum) {
        return PILLOW_C_NULL_POINTER;
    }

    out_words->clear();
    *out_word_width_sum = 0;
    std::size_t word_offset = 0;
    while (word_offset <= line.length) {
        std::size_t word_end = word_offset;
        while (word_end < line.length && text[line.start + word_end] != ' ') {
            ++word_end;
        }

        DefaultFontJustifyWordMetrics word{};
        word.start = line.start + word_offset;
        word.length = word_end - word_offset;
        const int status = default_font_text_metrics_span(
            text + word.start,
            word.length,
            &word.advance,
            &word.left,
            &word.top,
            &word.right,
            &word.bottom);
        if (status != PILLOW_C_OK) {
            return status;
        }
        *out_word_width_sum += word.advance;
        out_words->push_back(word);

        if (word_end == line.length) {
            break;
        }
        word_offset = word_end + 1;
    }

    return PILLOW_C_OK;
}

bool default_font_should_justify_line(
    int align,
    const DefaultFontLineMetrics& line,
    int max_width,
    std::size_t line_index,
    std::size_t line_count)
{
    return align == PILLOW_C_TEXT_ALIGN_JUSTIFY &&
        max_width - line.advance != 0 &&
        line_index + 1u != line_count;
}

double default_font_justify_start_left(int left, int max_width, char horizontal)
{
    double line_left = static_cast<double>(left);
    if (horizontal == 'm') {
        line_left -= static_cast<double>(max_width) / 2.0;
    } else if (horizontal == 'r') {
        line_left -= static_cast<double>(max_width);
    }
    return line_left;
}

double default_font_justify_gap(int max_width, int word_width_sum, std::size_t word_count)
{
    return static_cast<double>(max_width - word_width_sum) /
        static_cast<double>(word_count - 1u);
}

int pillow_round_text_origin(double value)
{
    return static_cast<int>(std::floor(value + 0.5));
}

bool parse_default_font_anchor(const char* anchor, char* out_horizontal, char* out_vertical)
{
    if (!anchor || !out_horizontal || !out_vertical) {
        return false;
    }
    if (std::strlen(anchor) != 2u) {
        return false;
    }

    const char horizontal = anchor[0];
    const char vertical = anchor[1];
    const bool horizontal_ok = horizontal == 'l' || horizontal == 'm' || horizontal == 'r';
    const bool vertical_ok = vertical == 'a' ||
        vertical == 't' ||
        vertical == 'm' ||
        vertical == 'b' ||
        vertical == 'd' ||
        vertical == 's';
    if (!horizontal_ok || !vertical_ok) {
        return false;
    }
    *out_horizontal = horizontal;
    *out_vertical = vertical;
    return true;
}

bool parse_default_font_multiline_anchor(const char* anchor, char* out_horizontal, char* out_vertical)
{
    if (!parse_default_font_anchor(anchor, out_horizontal, out_vertical)) {
        return false;
    }
    return *out_vertical != 't' && *out_vertical != 'b';
}

int default_font_anchor_x_offset(int length, char horizontal)
{
    if (horizontal == 'm') {
        return -pillow_round_text_origin(static_cast<double>(length) / 2.0);
    }
    if (horizontal == 'r') {
        return -length;
    }
    return 0;
}

int default_font_anchor_y_offset(int bbox_top, int bbox_bottom, char vertical)
{
    if (vertical == 't') {
        return -bbox_top;
    }
    if (vertical == 'm') {
        return -((PILLOW_C_DEFAULT_FONT_ASCENT + PILLOW_C_DEFAULT_FONT_DESCENT) / 2);
    }
    if (vertical == 'b') {
        return -bbox_bottom;
    }
    if (vertical == 'd') {
        return -(PILLOW_C_DEFAULT_FONT_ASCENT + PILLOW_C_DEFAULT_FONT_DESCENT);
    }
    if (vertical == 's') {
        return -PILLOW_C_DEFAULT_FONT_ASCENT;
    }
    return 0;
}

int default_font_anchor_origin_offset(
    const char* text,
    const char* anchor,
    int* out_x_offset,
    int* out_y_offset)
{
    if (!text || !anchor || !out_x_offset || !out_y_offset) {
        return PILLOW_C_NULL_POINTER;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (length == 0 && text[0] == '\0') {
        *out_x_offset = 0;
        *out_y_offset = 0;
        return PILLOW_C_OK;
    }

    *out_x_offset = default_font_anchor_x_offset(length, horizontal);
    *out_y_offset = default_font_anchor_y_offset(bbox_top, bbox_bottom, vertical);
    return PILLOW_C_OK;
}

int default_font_textbbox_anchor(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    if (length == 0 && text[0] == '\0') {
        *out_left = left;
        *out_top = top;
        *out_right = left;
        *out_bottom = top;
        return PILLOW_C_OK;
    }

    const int x_offset = default_font_anchor_x_offset(length, horizontal);
    const int y_offset = default_font_anchor_y_offset(bbox_top, bbox_bottom, vertical);
    *out_left = left + x_offset + bbox_left;
    *out_top = top + y_offset + bbox_top;
    *out_right = left + x_offset + bbox_right;
    *out_bottom = top + y_offset + bbox_bottom;
    return PILLOW_C_OK;
}

void expand_i32_bbox(int stroke_width, int* left, int* top, int* right, int* bottom)
{
    *left -= stroke_width;
    *top -= stroke_width;
    *right += stroke_width;
    *bottom += stroke_width;
}

void expand_f64_bbox(int stroke_width, double* left, double* top, double* right, double* bottom)
{
    const double stroke = static_cast<double>(stroke_width);
    *left -= stroke;
    *top -= stroke;
    *right += stroke;
    *bottom += stroke;
}

int default_font_textbbox_stroke(
    int left,
    int top,
    const char* text,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = left + bbox_left;
    *out_top = top + bbox_top;
    *out_right = left + bbox_right;
    *out_bottom = top + bbox_bottom;
    expand_i32_bbox(stroke_width, out_left, out_top, out_right, out_bottom);
    return PILLOW_C_OK;
}

int default_font_textbbox_anchor_stroke(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    const int status =
        default_font_textbbox_anchor(left, top, text, anchor, out_left, out_top, out_right, out_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    expand_i32_bbox(stroke_width, out_left, out_top, out_right, out_bottom);
    return PILLOW_C_OK;
}

void default_font_line_textbbox_anchor(
    double left,
    double top,
    const DefaultFontLineMetrics& line,
    char horizontal,
    char vertical,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (line.advance == 0 && line.length == 0) {
        *out_left = left;
        *out_top = top;
        *out_right = left;
        *out_bottom = top;
        return;
    }

    const int x_offset = default_font_anchor_x_offset(line.advance, horizontal);
    const int y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
    *out_left = left + static_cast<double>(x_offset + line.left);
    *out_top = top + static_cast<double>(y_offset + line.top);
    *out_right = left + static_cast<double>(x_offset + line.right);
    *out_bottom = top + static_cast<double>(y_offset + line.bottom);
}

void default_font_justify_word_textbbox_anchor(
    double left,
    double top,
    const DefaultFontJustifyWordMetrics& word,
    char horizontal,
    char vertical,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (word.advance == 0 && word.length == 0) {
        *out_left = left;
        *out_top = top;
        *out_right = left;
        *out_bottom = top;
        return;
    }

    const int x_offset = default_font_anchor_x_offset(word.advance, horizontal);
    const int y_offset = default_font_anchor_y_offset(word.top, word.bottom, vertical);
    *out_left = left + static_cast<double>(x_offset + word.left);
    *out_top = top + static_cast<double>(y_offset + word.top);
    *out_right = left + static_cast<double>(x_offset + word.right);
    *out_bottom = top + static_cast<double>(y_offset + word.bottom);
}

double default_font_multiline_anchor_top(
    int top,
    std::size_t line_count,
    int line_spacing,
    char vertical)
{
    const double line_span =
        static_cast<double>(line_count == 0 ? 0 : line_count - 1) * static_cast<double>(line_spacing);
    if (vertical == 'm') {
        return static_cast<double>(top) - line_span / 2.0;
    }
    if (vertical == 'd') {
        return static_cast<double>(top) - line_span;
    }
    return static_cast<double>(top);
}

double default_font_multiline_anchor_line_left(
    int left,
    int max_width,
    int line_width,
    int align,
    char horizontal)
{
    const int width_difference = max_width - line_width;
    double line_left = static_cast<double>(left) +
        default_font_multiline_align_offset(max_width, line_width, align);
    if (horizontal == 'm') {
        line_left -= static_cast<double>(width_difference) / 2.0;
    } else if (horizontal == 'r') {
        line_left -= static_cast<double>(width_difference);
    }
    return line_left;
}

int default_font_multiline_textbbox_anchor(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double word_bbox_left = 0.0;
                    double word_bbox_top = 0.0;
                    double word_bbox_right = 0.0;
                    double word_bbox_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        vertical,
                        &word_bbox_left,
                        &word_bbox_top,
                        &word_bbox_right,
                        &word_bbox_bottom);
                    if (!has_bbox) {
                        bbox_left = word_bbox_left;
                        bbox_top = word_bbox_top;
                        bbox_right = word_bbox_right;
                        bbox_bottom = word_bbox_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, word_bbox_left);
                        bbox_top = std::min(bbox_top, word_bbox_top);
                        bbox_right = std::max(bbox_right, word_bbox_right);
                        bbox_bottom = std::max(bbox_bottom, word_bbox_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        double line_bbox_left = 0.0;
        double line_bbox_top = 0.0;
        double line_bbox_right = 0.0;
        double line_bbox_bottom = 0.0;
        default_font_line_textbbox_anchor(
            line_left,
            line_top,
            line,
            horizontal,
            vertical,
            &line_bbox_left,
            &line_bbox_top,
            &line_bbox_right,
            &line_bbox_bottom);
        if (!has_bbox) {
            bbox_left = line_bbox_left;
            bbox_top = line_bbox_top;
            bbox_right = line_bbox_right;
            bbox_bottom = line_bbox_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, line_bbox_left);
            bbox_top = std::min(bbox_top, line_bbox_top);
            bbox_right = std::max(bbox_right, line_bbox_right);
            bbox_bottom = std::max(bbox_bottom, line_bbox_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_line_spacing(int spacing, int stroke_width)
{
    constexpr int default_line_height = 10;
    return default_line_height + spacing + stroke_width * 2;
}

std::uint8_t blend_text_channel(std::uint8_t dst, std::uint8_t src, std::uint8_t alpha)
{
    if (alpha == 255) {
        return src;
    }
    const std::uint32_t blended =
        static_cast<std::uint32_t>(dst) * (255u - alpha) +
        static_cast<std::uint32_t>(src) * alpha +
        128u;
    return static_cast<std::uint8_t>(shift_for_div255(blended));
}

int draw_text_image_span(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* fill,
    std::size_t fill_size);

bool colors_equal(const std::uint8_t* left, std::size_t left_size, const std::uint8_t* right, std::size_t right_size)
{
    return left_size == right_size &&
        (left_size == 0 || std::memcmp(left, right, left_size) == 0);
}

int blend_text_mask_region(
    PillowCImage* image,
    int mask_left,
    int mask_top,
    int mask_width,
    int mask_height,
    const std::vector<std::uint8_t>& mask,
    const std::uint8_t* color,
    std::size_t color_size)
{
    if (!image || !color) {
        return PILLOW_C_NULL_POINTER;
    }
    if (color_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (image->pixels.empty() || mask.empty() || mask_width <= 0 || mask_height <= 0) {
        return PILLOW_C_OK;
    }

    const int dst_left = std::max(mask_left, 0);
    const int dst_top = std::max(mask_top, 0);
    const int dst_right = std::min(mask_left + mask_width, image->width);
    const int dst_bottom = std::min(mask_top + mask_height, image->height);
    if (dst_right <= dst_left || dst_bottom <= dst_top) {
        return PILLOW_C_OK;
    }

    const int channels = image->channels;
    for (int y = dst_top; y < dst_bottom; ++y) {
        const int mask_y = y - mask_top;
        const std::uint8_t* mask_row =
            mask.data() + static_cast<std::size_t>(mask_y) * static_cast<std::size_t>(mask_width);
        std::uint8_t* dst_row =
            image->pixels.data() + static_cast<std::size_t>(y) * image->stride;
        for (int x = dst_left; x < dst_right; ++x) {
            const std::uint8_t alpha = mask_row[x - mask_left];
            if (alpha == 0) {
                continue;
            }
            std::uint8_t* dst =
                dst_row + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels);
            for (int channel = 0; channel < channels; ++channel) {
                dst[channel] = blend_text_channel(dst[channel], color[channel], alpha);
            }
        }
    }
    return PILLOW_C_OK;
}

int draw_text_stroke_mask_image_span(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size,
    int stroke_width)
{
    if (!image || !text || !stroke_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stroke_fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_text_image_span(image, left, top, text, text_length, stroke_fill, stroke_fill_size);
    }

    int text_pixel_length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int metrics_status = default_font_text_metrics_span(
        text,
        text_length,
        &text_pixel_length,
        &bbox_left,
        &bbox_top,
        &bbox_right,
        &bbox_bottom);
    if (metrics_status != PILLOW_C_OK) {
        return metrics_status;
    }
    if (image->pixels.empty() || text_pixel_length == 0) {
        return PILLOW_C_OK;
    }

    const int mask_left = left + bbox_left - stroke_width;
    const int mask_top = top + bbox_top - stroke_width;
    const int mask_right = left + bbox_right + stroke_width;
    const int mask_bottom = top + bbox_bottom + stroke_width;
    const int mask_width = mask_right - mask_left;
    const int mask_height = mask_bottom - mask_top;
    if (mask_width <= 0 || mask_height <= 0) {
        return PILLOW_C_OK;
    }

    const std::uint64_t mask_area =
        static_cast<std::uint64_t>(mask_width) * static_cast<std::uint64_t>(mask_height);
    if (mask_area > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return PILLOW_C_ALLOCATION_FAILED;
    }

    try {
        std::vector<std::uint8_t> mask(static_cast<std::size_t>(mask_area), 0);
        int cursor = 0;
        for (std::size_t index = 0; index < text_length; ++index) {
            const DefaultFontGlyph* glyph = default_font_glyph(static_cast<unsigned char>(text[index]));
            if (!glyph) {
                return PILLOW_C_INVALID_ARGUMENT;
            }
            for (int glyph_y = 0; glyph_y < glyph->height; ++glyph_y) {
                const std::uint8_t* glyph_row =
                    glyph->mask + static_cast<std::size_t>(glyph_y) * static_cast<std::size_t>(glyph->width);
                for (int glyph_x = 0; glyph_x < glyph->width; ++glyph_x) {
                    const std::uint8_t alpha = glyph_row[glyph_x];
                    if (alpha == 0) {
                        continue;
                    }
                    const int base_x = left + cursor + glyph->offset_x + glyph_x;
                    const int base_y = top + glyph->offset_y + glyph_y;
                    for (int offset_y = -stroke_width; offset_y <= stroke_width; ++offset_y) {
                        const int mask_y = base_y + offset_y - mask_top;
                        if (mask_y < 0 || mask_y >= mask_height) {
                            continue;
                        }
                        std::uint8_t* mask_row =
                            mask.data() + static_cast<std::size_t>(mask_y) * static_cast<std::size_t>(mask_width);
                        for (int offset_x = -stroke_width; offset_x <= stroke_width; ++offset_x) {
                            const int mask_x = base_x + offset_x - mask_left;
                            if (mask_x < 0 || mask_x >= mask_width) {
                                continue;
                            }
                            std::uint8_t& current = mask_row[mask_x];
                            current = std::max(current, alpha);
                        }
                    }
                }
            }
            cursor += glyph->advance;
        }
        return blend_text_mask_region(
            image,
            mask_left,
            mask_top,
            mask_width,
            mask_height,
            mask,
            stroke_fill,
            stroke_fill_size);
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

int draw_text_image_span(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    if (!image || !text || !fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    int text_pixel_length = 0;
    const int metrics_status =
        default_font_text_metrics_span(text, text_length, &text_pixel_length, nullptr, nullptr, nullptr, nullptr);
    if (metrics_status != PILLOW_C_OK) {
        return metrics_status;
    }
    if (image->pixels.empty() || text_pixel_length == 0) {
        return PILLOW_C_OK;
    }

    int cursor = 0;
    for (std::size_t index = 0; index < text_length; ++index) {
        const DefaultFontGlyph* glyph = default_font_glyph(static_cast<unsigned char>(text[index]));
        if (!glyph) {
            return PILLOW_C_INVALID_ARGUMENT;
        }
        for (int glyph_y = 0; glyph_y < glyph->height; ++glyph_y) {
            const int dst_y = top + glyph->offset_y + glyph_y;
            if (dst_y < 0 || dst_y >= image->height) {
                continue;
            }
            const std::uint8_t* mask_row =
                glyph->mask + static_cast<std::size_t>(glyph_y) * static_cast<std::size_t>(glyph->width);
            std::uint8_t* dst_row = image->pixels.data() + static_cast<std::size_t>(dst_y) * image->stride;
            for (int glyph_x = 0; glyph_x < glyph->width; ++glyph_x) {
                const std::uint8_t alpha = mask_row[glyph_x];
                if (alpha == 0) {
                    continue;
                }
                const int dst_x = left + cursor + glyph->offset_x + glyph_x;
                if (dst_x < 0 || dst_x >= image->width) {
                    continue;
                }
                std::uint8_t* dst =
                    dst_row + static_cast<std::size_t>(dst_x) * static_cast<std::size_t>(image->channels);
                for (int channel = 0; channel < image->channels; ++channel) {
                    dst[channel] = blend_text_channel(dst[channel], fill[channel], alpha);
                }
            }
        }
        cursor += glyph->advance;
    }
    return PILLOW_C_OK;
}

int draw_text_image(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    if (!text) {
        return PILLOW_C_NULL_POINTER;
    }
    return draw_text_image_span(image, left, top, text, std::strlen(text), fill, fill_size);
}

int draw_text_image_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!text) {
        return PILLOW_C_NULL_POINTER;
    }
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_text_image(image, left, top, text, fill, fill_size);
    }
    const std::size_t text_length = std::strlen(text);
    const int stroke_status = draw_text_stroke_mask_image_span(
        image,
        left,
        top,
        text,
        text_length,
        stroke_fill,
        stroke_fill_size,
        stroke_width);
    if (stroke_status != PILLOW_C_OK) {
        return stroke_status;
    }
    if (colors_equal(fill, fill_size, stroke_fill, stroke_fill_size)) {
        return PILLOW_C_OK;
    }
    return draw_text_image_span(image, left, top, text, text_length, fill, fill_size);
}

int draw_text_image_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    int x_offset = 0;
    int y_offset = 0;
    const int anchor_status = default_font_anchor_origin_offset(text, anchor, &x_offset, &y_offset);
    if (anchor_status != PILLOW_C_OK) {
        return anchor_status;
    }
    return draw_text_image(image, left + x_offset, top + y_offset, text, fill, fill_size);
}

int draw_text_image_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    int x_offset = 0;
    int y_offset = 0;
    const int anchor_status = default_font_anchor_origin_offset(text, anchor, &x_offset, &y_offset);
    if (anchor_status != PILLOW_C_OK) {
        return anchor_status;
    }
    return draw_text_image_stroke(
        image,
        left + x_offset,
        top + y_offset,
        text,
        fill,
        fill_size,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int default_font_multiline_textbbox_align(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top =
                    static_cast<double>(top + static_cast<int>(line_index) * line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double absolute_left = 0.0;
                    double absolute_top = 0.0;
                    double absolute_right = 0.0;
                    double absolute_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        'a',
                        &absolute_left,
                        &absolute_top,
                        &absolute_right,
                        &absolute_bottom);
                    if (!has_bbox) {
                        bbox_left = absolute_left;
                        bbox_top = absolute_top;
                        bbox_right = absolute_right;
                        bbox_bottom = absolute_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, absolute_left);
                        bbox_top = std::min(bbox_top, absolute_top);
                        bbox_right = std::max(bbox_right, absolute_right);
                        bbox_bottom = std::max(bbox_bottom, absolute_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_x = static_cast<double>(left) +
            default_font_multiline_align_offset(max_width, line.advance, align);
        const int line_y = static_cast<int>(line_index) * line_spacing;
        const double absolute_left = line_x + line.left;
        const double absolute_top = static_cast<double>(top + line_y + line.top);
        const double absolute_right = line_x + line.right;
        const double absolute_bottom = static_cast<double>(top + line_y + line.bottom);
        if (!has_bbox) {
            bbox_left = absolute_left;
            bbox_top = absolute_top;
            bbox_right = absolute_right;
            bbox_bottom = absolute_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, absolute_left);
            bbox_top = std::min(bbox_top, absolute_top);
            bbox_right = std::max(bbox_right, absolute_right);
            bbox_bottom = std::max(bbox_bottom, absolute_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_align_stroke(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align) || stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top =
                    static_cast<double>(top + static_cast<int>(line_index) * line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double absolute_left = 0.0;
                    double absolute_top = 0.0;
                    double absolute_right = 0.0;
                    double absolute_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        'a',
                        &absolute_left,
                        &absolute_top,
                        &absolute_right,
                        &absolute_bottom);
                    expand_f64_bbox(stroke_width, &absolute_left, &absolute_top, &absolute_right, &absolute_bottom);
                    if (!has_bbox) {
                        bbox_left = absolute_left;
                        bbox_top = absolute_top;
                        bbox_right = absolute_right;
                        bbox_bottom = absolute_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, absolute_left);
                        bbox_top = std::min(bbox_top, absolute_top);
                        bbox_right = std::max(bbox_right, absolute_right);
                        bbox_bottom = std::max(bbox_bottom, absolute_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_x = static_cast<double>(left) +
            default_font_multiline_align_offset(max_width, line.advance, align);
        const int line_y = static_cast<int>(line_index) * line_spacing;
        double absolute_left = line_x + static_cast<double>(line.left);
        double absolute_top = static_cast<double>(top + line_y + line.top);
        double absolute_right = line_x + static_cast<double>(line.right);
        double absolute_bottom = static_cast<double>(top + line_y + line.bottom);
        expand_f64_bbox(stroke_width, &absolute_left, &absolute_top, &absolute_right, &absolute_bottom);
        if (!has_bbox) {
            bbox_left = absolute_left;
            bbox_top = absolute_top;
            bbox_right = absolute_right;
            bbox_bottom = absolute_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, absolute_left);
            bbox_top = std::min(bbox_top, absolute_top);
            bbox_right = std::max(bbox_right, absolute_right);
            bbox_bottom = std::max(bbox_bottom, absolute_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_align_stroke_i32(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;
    const int status = default_font_multiline_textbbox_align_stroke(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        &bbox_left,
        &bbox_top,
        &bbox_right,
        &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = static_cast<int>(std::floor(bbox_left));
    *out_top = static_cast<int>(std::floor(bbox_top));
    *out_right = static_cast<int>(std::ceil(bbox_right));
    *out_bottom = static_cast<int>(std::ceil(bbox_bottom));
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_anchor_stroke(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!text || !anchor || !out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    if (!valid_text_align(align) || stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    bool has_bbox = false;
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    double line_bbox_left = 0.0;
                    double line_bbox_top = 0.0;
                    double line_bbox_right = 0.0;
                    double line_bbox_bottom = 0.0;
                    default_font_justify_word_textbbox_anchor(
                        word_left,
                        line_top,
                        word,
                        'l',
                        vertical,
                        &line_bbox_left,
                        &line_bbox_top,
                        &line_bbox_right,
                        &line_bbox_bottom);
                    expand_f64_bbox(stroke_width, &line_bbox_left, &line_bbox_top, &line_bbox_right, &line_bbox_bottom);
                    if (!has_bbox) {
                        bbox_left = line_bbox_left;
                        bbox_top = line_bbox_top;
                        bbox_right = line_bbox_right;
                        bbox_bottom = line_bbox_bottom;
                        has_bbox = true;
                    } else {
                        bbox_left = std::min(bbox_left, line_bbox_left);
                        bbox_top = std::min(bbox_top, line_bbox_top);
                        bbox_right = std::max(bbox_right, line_bbox_right);
                        bbox_bottom = std::max(bbox_bottom, line_bbox_bottom);
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        int x_offset = 0;
        int y_offset = 0;
        if (!(line.advance == 0 && line.length == 0)) {
            x_offset = default_font_anchor_x_offset(line.advance, horizontal);
            y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
        }

        double line_bbox_left = line_left + static_cast<double>(x_offset + line.left);
        double line_bbox_top = line_top + static_cast<double>(y_offset + line.top);
        double line_bbox_right = line_left + static_cast<double>(x_offset + line.right);
        double line_bbox_bottom = line_top + static_cast<double>(y_offset + line.bottom);
        expand_f64_bbox(stroke_width, &line_bbox_left, &line_bbox_top, &line_bbox_right, &line_bbox_bottom);
        if (!has_bbox) {
            bbox_left = line_bbox_left;
            bbox_top = line_bbox_top;
            bbox_right = line_bbox_right;
            bbox_bottom = line_bbox_bottom;
            has_bbox = true;
        } else {
            bbox_left = std::min(bbox_left, line_bbox_left);
            bbox_top = std::min(bbox_top, line_bbox_top);
            bbox_right = std::max(bbox_right, line_bbox_right);
            bbox_bottom = std::max(bbox_bottom, line_bbox_bottom);
        }
    }

    *out_left = has_bbox ? bbox_left : static_cast<double>(left);
    *out_top = has_bbox ? bbox_top : static_cast<double>(top);
    *out_right = has_bbox ? bbox_right : static_cast<double>(left);
    *out_bottom = has_bbox ? bbox_bottom : static_cast<double>(top);
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox_align_i32(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    double bbox_left = 0.0;
    double bbox_top = 0.0;
    double bbox_right = 0.0;
    double bbox_bottom = 0.0;
    const int status = default_font_multiline_textbbox_align(
        left,
        top,
        text,
        spacing,
        align,
        &bbox_left,
        &bbox_top,
        &bbox_right,
        &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = static_cast<int>(std::floor(bbox_left));
    *out_top = static_cast<int>(std::floor(bbox_top));
    *out_right = static_cast<int>(std::ceil(bbox_right));
    *out_bottom = static_cast<int>(std::ceil(bbox_bottom));
    return PILLOW_C_OK;
}

int default_font_multiline_textbbox(
    int left,
    int top,
    const char* text,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_align_i32(
        left,
        top,
        text,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int draw_multiline_text_image_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    if (!image || !text || !fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const int line_top = top + static_cast<int>(line_index) * line_spacing;
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    const int status = draw_text_image_span(
                        image,
                        pillow_round_text_origin(word_left),
                        line_top,
                        text + word.start,
                        word.length,
                        fill,
                        fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const int line_left = left + pillow_round_text_origin(
            default_font_multiline_align_offset(max_width, line.advance, align));
        const int status = draw_text_image_span(
            image,
            line_left,
            top + static_cast<int>(line_index) * line_spacing,
            text + line.start,
            line.length,
            fill,
            fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_image_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    if (!image || !text || !fill || !anchor) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr int default_line_height = 10;
    const int line_spacing = default_line_height + spacing;
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    int y_offset = 0;
                    if (!(word.advance == 0 && word.length == 0)) {
                        y_offset = default_font_anchor_y_offset(word.top, word.bottom, vertical);
                    }
                    const int status = draw_text_image_span(
                        image,
                        pillow_round_text_origin(word_left),
                        pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
                        text + word.start,
                        word.length,
                        fill,
                        fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        int x_offset = 0;
        int y_offset = 0;
        if (!(line.advance == 0 && line.length == 0)) {
            x_offset = default_font_anchor_x_offset(line.advance, horizontal);
            y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
        }

        const int status = draw_text_image_span(
            image,
            pillow_round_text_origin(line_left + static_cast<double>(x_offset)),
            pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
            text + line.start,
            line.length,
            fill,
            fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_line_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    std::size_t text_length,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    const int stroke_status = draw_text_stroke_mask_image_span(
        image,
        left,
        top,
        text,
        text_length,
        stroke_fill,
        stroke_fill_size,
        stroke_width);
    if (stroke_status != PILLOW_C_OK) {
        return stroke_status;
    }
    if (colors_equal(fill, fill_size, stroke_fill, stroke_fill_size)) {
        return PILLOW_C_OK;
    }
    return draw_text_image_span(image, left, top, text, text_length, fill, fill_size);
}

int draw_multiline_text_image_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_multiline_text_image_align(image, left, top, text, fill, fill_size, spacing, align);
    }
    if (!image || !text || !fill || !stroke_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels) ||
        stroke_fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = static_cast<double>(left);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const int line_top = top + static_cast<int>(line_index) * line_spacing;
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    const int status = draw_multiline_text_line_stroke(
                        image,
                        pillow_round_text_origin(word_left),
                        line_top,
                        text + word.start,
                        word.length,
                        fill,
                        fill_size,
                        stroke_width,
                        stroke_fill,
                        stroke_fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const int line_left = left + pillow_round_text_origin(
            default_font_multiline_align_offset(max_width, line.advance, align));
        const int status = draw_multiline_text_line_stroke(
            image,
            line_left,
            top + static_cast<int>(line_index) * line_spacing,
            text + line.start,
            line.length,
            fill,
            fill_size,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_image_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (stroke_width < 0) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    if (stroke_width == 0) {
        return draw_multiline_text_image_anchor(image, left, top, text, fill, fill_size, spacing, align, anchor);
    }
    if (!image || !text || !fill || !anchor || !stroke_fill) {
        return PILLOW_C_NULL_POINTER;
    }
    if (fill_size != static_cast<std::size_t>(image->channels) ||
        stroke_fill_size != static_cast<std::size_t>(image->channels)) {
        return PILLOW_C_INVALID_LENGTH;
    }
    if (!valid_text_align(align)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    char horizontal = '\0';
    char vertical = '\0';
    if (!parse_default_font_multiline_anchor(anchor, &horizontal, &vertical)) {
        return PILLOW_C_INVALID_ARGUMENT;
    }

    const int line_spacing = default_font_multiline_line_spacing(spacing, stroke_width);
    std::vector<DefaultFontLineMetrics> lines;
    int max_width = 0;
    const int collect_status = collect_default_font_multiline_metrics(text, &lines, &max_width);
    if (collect_status != PILLOW_C_OK) {
        return collect_status;
    }

    const double first_line_top =
        default_font_multiline_anchor_top(top, lines.size(), line_spacing, vertical);
    for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
        const DefaultFontLineMetrics& line = lines[line_index];
        if (default_font_should_justify_line(align, line, max_width, line_index, lines.size())) {
            std::vector<DefaultFontJustifyWordMetrics> words;
            int word_width_sum = 0;
            const int word_status =
                collect_default_font_justify_words(text, line, &words, &word_width_sum);
            if (word_status != PILLOW_C_OK) {
                return word_status;
            }
            if (words.size() > 1u) {
                double word_left = default_font_justify_start_left(left, max_width, horizontal);
                const double word_gap = default_font_justify_gap(max_width, word_width_sum, words.size());
                const double line_top = first_line_top +
                    static_cast<double>(line_index) * static_cast<double>(line_spacing);
                for (const DefaultFontJustifyWordMetrics& word : words) {
                    int y_offset = 0;
                    if (!(word.advance == 0 && word.length == 0)) {
                        y_offset = default_font_anchor_y_offset(word.top, word.bottom, vertical);
                    }
                    const int status = draw_multiline_text_line_stroke(
                        image,
                        pillow_round_text_origin(word_left),
                        pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
                        text + word.start,
                        word.length,
                        fill,
                        fill_size,
                        stroke_width,
                        stroke_fill,
                        stroke_fill_size);
                    if (status != PILLOW_C_OK) {
                        return status;
                    }
                    word_left += static_cast<double>(word.advance) + word_gap;
                }
                continue;
            }
        }
        const double line_left = default_font_multiline_anchor_line_left(
            left,
            max_width,
            line.advance,
            align,
            horizontal);
        const double line_top = first_line_top +
            static_cast<double>(line_index) * static_cast<double>(line_spacing);
        int x_offset = 0;
        int y_offset = 0;
        if (!(line.advance == 0 && line.length == 0)) {
            x_offset = default_font_anchor_x_offset(line.advance, horizontal);
            y_offset = default_font_anchor_y_offset(line.top, line.bottom, vertical);
        }

        const int status = draw_multiline_text_line_stroke(
            image,
            pillow_round_text_origin(line_left + static_cast<double>(x_offset)),
            pillow_round_text_origin(line_top + static_cast<double>(y_offset)),
            text + line.start,
            line.length,
            fill,
            fill_size,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
        if (status != PILLOW_C_OK) {
            return status;
        }
    }
    return PILLOW_C_OK;
}

int draw_multiline_text_image(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return draw_multiline_text_image_align(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT);
}

int draw_multiline_text_image_font_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_align(image, left, top, text, fill, fill_size, spacing, align);
}

int draw_multiline_text_image_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_anchor(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        anchor);
}

int draw_multiline_text_image_font_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_align_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int draw_multiline_text_image_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_multiline_text_image_anchor_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        spacing,
        align,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int draw_multiline_text_image_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return draw_multiline_text_image_font_align(
        image,
        left,
        top,
        text,
        font,
        fill,
        fill_size,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT);
}

int default_font_multiline_textbbox_font_align(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align_i32(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_align_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_anchor_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_anchor(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_align_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align_stroke_i32(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_align_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_align_stroke(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font_anchor_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_multiline_textbbox_anchor_stroke(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int default_font_multiline_textbbox_font(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font_align(
        left,
        top,
        text,
        font,
        spacing,
        PILLOW_C_TEXT_ALIGN_LEFT,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

int draw_text_image_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image(image, left, top, text, fill, fill_size);
}

int draw_text_image_font_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int draw_text_image_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image_anchor(image, left, top, text, fill, fill_size, anchor);
}

int draw_text_image_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return draw_text_image_anchor_stroke(
        image,
        left,
        top,
        text,
        fill,
        fill_size,
        anchor,
        stroke_width,
        stroke_fill,
        stroke_fill_size);
}

int default_font_textbbox_font_anchor(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_textbbox_anchor(left, top, text, anchor, out_left, out_top, out_right, out_bottom);
}

int default_font_textbbox_font_anchor_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!font) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    return default_font_textbbox_anchor_stroke(
        left,
        top,
        text,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_font_load_default(PillowCFont** out_font)
{
    if (!out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    try {
        *out_font = new PillowCFont{PILLOW_C_FONT_DEFAULT};
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        *out_font = nullptr;
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_font_free(PillowCFont* font)
{
    delete font;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_getmetrics(
    const PillowCFont* font,
    int* out_ascent,
    int* out_descent)
{
    if (!font || !out_ascent || !out_descent) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    *out_ascent = 10;
    *out_descent = 3;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_getname(
    const PillowCFont* font,
    char* out_family,
    std::size_t family_size,
    std::size_t* out_family_required,
    char* out_style,
    std::size_t style_size,
    std::size_t* out_style_required)
{
    if (!font || !out_family_required || !out_style_required) {
        return PILLOW_C_NULL_POINTER;
    }
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        *out_family_required = 0;
        *out_style_required = 0;
        return PILLOW_C_INVALID_ARGUMENT;
    }

    constexpr const char* family = "Aileron";
    constexpr const char* style = "Regular";
    const std::size_t family_required = std::strlen(family) + 1;
    const std::size_t style_required = std::strlen(style) + 1;
    *out_family_required = family_required;
    *out_style_required = style_required;

    if (!out_family || !out_style) {
        return PILLOW_C_NULL_POINTER;
    }
    if (family_size < family_required || style_size < style_required) {
        return PILLOW_C_INVALID_LENGTH;
    }
    std::memcpy(out_family, family, family_required);
    std::memcpy(out_style, style, style_required);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_variant(
    const PillowCFont* font,
    PillowCFont** out_font)
{
    if (!font || !out_font) {
        return PILLOW_C_NULL_POINTER;
    }
    *out_font = nullptr;
    if (font->kind != PILLOW_C_FONT_DEFAULT) {
        return PILLOW_C_INVALID_ARGUMENT;
    }
    try {
        *out_font = new PillowCFont{font->kind};
        return PILLOW_C_OK;
    } catch (const std::bad_alloc&) {
        return PILLOW_C_ALLOCATION_FAILED;
    }
}

extern "C" __declspec(dllexport) int pillow_c_font_getlength(
    const PillowCFont* font,
    const char* text,
    double* out_length)
{
    if (!out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    const int status = font_text_metrics(font, text, &length, nullptr, nullptr, nullptr, nullptr);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_length = static_cast<double>(length);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_font_getbbox(
    const PillowCFont* font,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    return font_text_metrics(font, text, &length, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_font_getbbox_anchor(
    const PillowCFont* font,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_font_anchor(0, 0, text, font, anchor, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_rectangle(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_rectangle_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_ellipse(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_ellipse_image(image, left, top, right, bottom, fill, fill_size, outline, outline_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_arc(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_arc_image(image, left, top, right, bottom, start, end, color, color_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_chord(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_chord_image(image, left, top, right, bottom, start, end, fill, fill_size, outline, outline_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_pieslice(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double start,
    double end,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_pieslice_image(image, left, top, right, bottom, start, end, fill, fill_size, outline, outline_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_rounded_rectangle(
    PillowCImage* image,
    int left,
    int top,
    int right,
    int bottom,
    double radius,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width,
    int corners_mask)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_rounded_rectangle_image(
            image,
            left,
            top,
            right,
            bottom,
            radius,
            fill,
            fill_size,
            outline,
            outline_size,
            width,
            corners_mask);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_bitmap(
    PillowCImage* image,
    int left,
    int top,
    const PillowCImage* mask,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_bitmap_image(image, left, top, mask, color, color_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_floodfill(
    PillowCImage* image,
    int seed_x,
    int seed_y,
    const std::uint8_t* value,
    std::size_t value_size,
    const std::uint8_t* border,
    std::size_t border_size,
    double thresh)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_floodfill_image(image, seed_x, seed_y, value, value_size, border, border_size, thresh);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_line(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_line_image(image, points, point_count, color, color_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_line_joint(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size,
    int width,
    int joint_curve)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_line_joint_image(image, points, point_count, color, color_size, width, joint_curve);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_points(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* color,
    std::size_t color_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_points_image(image, points, point_count, color, color_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_polygon(
    PillowCImage* image,
    const int* points,
    std::size_t point_count,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const std::uint8_t* outline,
    std::size_t outline_size,
    int width)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_polygon_image(image, points, point_count, fill, fill_size, outline, outline_size, width);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image(image, left, top, text, fill, fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_anchor(image, left, top, text, fill, fill_size, anchor);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_stroke(
            image,
            left,
            top,
            text,
            fill,
            fill_size,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_anchor_stroke(
            image,
            left,
            top,
            text,
            fill,
            fill_size,
            anchor,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_font(image, left, top, text, font, fill, fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_font_stroke(
            image,
            left,
            top,
            text,
            font,
            fill,
            fill_size,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_font_anchor(image, left, top, text, font, fill, fill_size, anchor);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_text_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_text_image_font_anchor_stroke(
            image,
            left,
            top,
            text,
            font,
            fill,
            fill_size,
            anchor,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image(image, left, top, text, fill, fill_size, spacing);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_align(image, left, top, text, fill, fill_size, spacing, align);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_anchor(
            image,
            left,
            top,
            text,
            fill,
            fill_size,
            spacing,
            align,
            anchor);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_align_stroke(
            image,
            left,
            top,
            text,
            fill,
            fill_size,
            spacing,
            align,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_anchor_stroke(
            image,
            left,
            top,
            text,
            fill,
            fill_size,
            spacing,
            align,
            anchor,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_font(image, left, top, text, font, fill, fill_size, spacing);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_align(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_font_align(
            image,
            left,
            top,
            text,
            font,
            fill,
            fill_size,
            spacing,
            align);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_align_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_font_align_stroke(
            image,
            left,
            top,
            text,
            font,
            fill,
            fill_size,
            spacing,
            align,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_anchor(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_font_anchor(
            image,
            left,
            top,
            text,
            font,
            fill,
            fill_size,
            spacing,
            align,
            anchor);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_draw_multiline_text_font_anchor_stroke(
    PillowCImage* image,
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const std::uint8_t* fill,
    std::size_t fill_size,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    const std::uint8_t* stroke_fill,
    std::size_t stroke_fill_size)
{
    return with_detached_buffer_view(image, [&]() {
        return draw_multiline_text_image_font_anchor_stroke(
            image,
            left,
            top,
            text,
            font,
            fill,
            fill_size,
            spacing,
            align,
            anchor,
            stroke_width,
            stroke_fill,
            stroke_fill_size);
    });
}

extern "C" __declspec(dllexport) int pillow_c_image_textlength(
    const char* text,
    double* out_length)
{
    if (!out_length) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    const int status = default_font_text_metrics(text, &length, nullptr, nullptr, nullptr, nullptr);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_length = static_cast<double>(length);
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox(
    int left,
    int top,
    const char* text,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox(left, top, text, spacing, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_align_i32(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_align(
        left,
        top,
        text,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align_stroke(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_align_stroke_i32(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_align_stroke_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_align_stroke(
        left,
        top,
        text,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_anchor_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_anchor(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_anchor_stroke_f64(
    int left,
    int top,
    const char* text,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_anchor_stroke(
        left,
        top,
        text,
        spacing,
        align,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font(
        left,
        top,
        text,
        font,
        spacing,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font_align(
        left,
        top,
        text,
        font,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_multiline_textbbox_font_align_stroke(
        left,
        top,
        text,
        font,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_align_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_align_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_align_stroke_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_anchor_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_anchor_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        anchor,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_multiline_textbbox_font_anchor_stroke_f64(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    int spacing,
    int align,
    const char* anchor,
    int stroke_width,
    double* out_left,
    double* out_top,
    double* out_right,
    double* out_bottom)
{
    return default_font_multiline_textbbox_font_anchor_stroke_f64(
        left,
        top,
        text,
        font,
        spacing,
        align,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox(
    int left,
    int top,
    const char* text,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    if (!out_left || !out_top || !out_right || !out_bottom) {
        return PILLOW_C_NULL_POINTER;
    }
    int length = 0;
    int bbox_left = 0;
    int bbox_top = 0;
    int bbox_right = 0;
    int bbox_bottom = 0;
    const int status =
        default_font_text_metrics(text, &length, &bbox_left, &bbox_top, &bbox_right, &bbox_bottom);
    if (status != PILLOW_C_OK) {
        return status;
    }
    *out_left = left + bbox_left;
    *out_top = top + bbox_top;
    *out_right = left + bbox_right;
    *out_bottom = top + bbox_bottom;
    return PILLOW_C_OK;
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_stroke(
    int left,
    int top,
    const char* text,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_stroke(
        left,
        top,
        text,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_anchor(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_anchor(left, top, text, anchor, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_anchor_stroke(
    int left,
    int top,
    const char* text,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_anchor_stroke(
        left,
        top,
        text,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_font_anchor(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_font_anchor(left, top, text, font, anchor, out_left, out_top, out_right, out_bottom);
}

extern "C" __declspec(dllexport) int pillow_c_image_textbbox_font_anchor_stroke(
    int left,
    int top,
    const char* text,
    const PillowCFont* font,
    const char* anchor,
    int stroke_width,
    int* out_left,
    int* out_top,
    int* out_right,
    int* out_bottom)
{
    return default_font_textbbox_font_anchor_stroke(
        left,
        top,
        text,
        font,
        anchor,
        stroke_width,
        out_left,
        out_top,
        out_right,
        out_bottom);
}

}
