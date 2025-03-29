#include "canvas.h"
#include <cmath>
#include <algorithm>

using std::max;
using std::min;
using std::sqrt;

void buffer::set(unsigned x, unsigned y, color c) {
    unsigned index = (y * width + x) * 4;
    data[index] = c.r;
    data[index + 1] = c.g;
    data[index + 2] = c.b;
    data[index + 3] = c.a;
}

color buffer::get(unsigned x, unsigned y) const {
    unsigned index = (y * width + x) * 4;
    return {data[index], data[index + 1], data[index + 2], data[index + 3]};
}

void canvas::add_stroke_point(stroke_point p, bool end) {
    dirty.resize(data.width * data.height);
    unsigned top = max<unsigned>(p.y - p.radius, 0);
    unsigned bottom = min<unsigned>(p.y + p.radius, data.height - 1);

    for (uint16_t y = top; y <= bottom; ++y) {
        float width = 
            sqrt(max(0.f, p.radius * p.radius - (y - p.y) * (y - p.y)));
        unsigned left = max<unsigned>(p.x - width, 0);
        unsigned right = min<unsigned>(p.x + width, data.width - 1);

        for (uint16_t x = left; x <= right; ++x) {
            if (!dirty[y * data.width + x]) {
                dirty[y * data.width + x] = true;
                stage.push_back({x, y, data.get(x, y)});
            }
            data.set(x, y, p.c);
        }
    }
    
    if (end) {
        last_stroke_point = std::nullopt;
        undo_stack.push_back(std::move(stage));
        fill(dirty.begin(), dirty.end(), false);
    } else {
        last_stroke_point = p;
    }
}

void canvas::undo(unsigned stack) {
    if (undo_stack.empty())
        return;
    auto undo = std::move(undo_stack.back());
    undo_stack.pop_back();
    std::vector<pixel> redo;
    for (auto& p : undo) {
        redo.push_back({p.x, p.y, data.get(p.x, p.y)});
        data.set(p.x, p.y, p.c);
    }
    redo_stacks.resize(max<size_t>(redo_stacks.size(), stack + 1));
    redo_stacks[stack].push_back(std::move(redo));
}

void canvas::redo(unsigned stack) {
    redo_stacks.resize(max<size_t>(redo_stacks.size(), stack + 1));
    if (redo_stacks[stack].empty())
        return;
    auto redo = std::move(redo_stacks[stack].back());
    redo_stacks[stack].pop_back();
    std::vector<pixel> undo;
    for (auto& p : redo) {
        undo.push_back({p.x, p.y, data.get(p.x, p.y)});
        data.set(p.x, p.y, p.c);
    }
    undo_stack.push_back(std::move(undo));
}
