#pragma once
#include "../../Graphics/Geometry.h"

namespace ms {
inline void draw_sell_lock(Point<int16_t> at) {
    // Quiet 8 x 10 sale-protection badge, without the old bright yellow block.
    auto box=[&](int x,int y,int w,int h) {
        ColorBox(w,h,Color::TOBACCOBROWN,0.92f).draw(at+Point<int16_t>(x,y));
    };
    box(2,0,4,1); box(1,1,1,3); box(6,1,1,3);
    box(0,4,8,6);
    ColorBox(1,3,Color::GALLERY,0.9f).draw(at+Point<int16_t>(3,5));
}
}
