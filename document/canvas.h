#pragma once

#include <vector>
#include <cstdint>
#include <optional>

struct color {
    std::uint8_t r, g, b, a;
};

struct buffer {
    unsigned width, height;
    std::uint8_t *data;

    void set(unsigned x, unsigned y, color c);
};

struct stroke_point {
    float x, y;
    float radius;
    color c;
};

struct canvas {
    void add_stroke_point(stroke_point p, bool end);

    buffer data;

    std::optional<stroke_point> last_stroke_point;
};
