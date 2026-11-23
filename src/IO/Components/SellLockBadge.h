#pragma once
#include "../../Graphics/Geometry.h"

namespace ms {
inline void draw_sell_lock(Point<int16_t> at) {
    // Small gold padlock, legible over both inventory cells and item icons.
    ColorBox(8, 7, Color::Name::BLACK, 1).draw(at + Point<int16_t>(2, 0));
    ColorBox(4, 5, Color::Name::YELLOW, 1).draw(at + Point<int16_t>(4, 1));
    ColorBox(12, 9, Color::Name::BLACK, 1).draw(at + Point<int16_t>(0, 5));
    ColorBox(10, 7, Color::Name::YELLOW, 1).draw(at + Point<int16_t>(1, 6));
    ColorBox(2, 4, Color::Name::BLACK, 1).draw(at + Point<int16_t>(5, 7));
}
}
