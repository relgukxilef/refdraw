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
    color get(unsigned x, unsigned y) const;
};

struct stroke_point {
    float x, y;
    float radius;
    color c;
};

struct pixel {
    uint16_t x, y;
    color c;
};

struct canvas {
    void add_stroke_point(stroke_point p, bool end);

    void undo(unsigned stack = 0);
    void redo(unsigned stack = 0);

    buffer data;

    std::vector<bool> dirty;
    std::vector<pixel> stage;

    std::optional<stroke_point> last_stroke_point;
    std::vector<std::vector<pixel>> undo_stack;
    std::vector<std::vector<std::vector<pixel>>> redo_stacks;
};
