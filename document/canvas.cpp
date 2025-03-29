#include "canvas.h"
#include <cmath>
#include <algorithm>

using std::max;
using std::min;

void buffer::set(unsigned x, unsigned y, color c) {
    unsigned index = (y * width + x) * 4;
    data[index] = c.r;
    data[index + 1] = c.g;
    data[index + 2] = c.b;
    data[index + 3] = c.a;
}

void canvas::add_stroke_point(stroke_point p, bool end) {
    unsigned top = max<unsigned>(p.y - p.radius, 0);
    unsigned bottom = min<unsigned>(p.y + p.radius, data.height - 1);

    for (unsigned y = top; y <= bottom; ++y) {
        float width = std::sqrt(p.radius * p.radius - (y - p.y) * (y - p.y));
        unsigned left = min<unsigned>(p.x - width, 0);
        unsigned right = max<unsigned>(p.x + width, data.width - 1);
        for (unsigned x = left; x <= right; ++x) {
            data.set(x, y, p.c);
        }
    }
    
    if (end) {
        last_stroke_point = std::nullopt;
    } else {
        last_stroke_point = p;
    }
}
