#pragma once
#include <algorithm>

namespace ms
{
    struct Viewport { int x, y, width, height; };

    inline Viewport fit_viewport(int width, int height, int game_width, int game_height)
    {
        if (width <= 0 || height <= 0 || game_width <= 0 || game_height <= 0)
            return {0, 0, 0, 0};
        const double scale = std::min(double(width) / game_width, double(height) / game_height);
        const int w = std::max(1, int(game_width * scale));
        const int h = std::max(1, int(game_height * scale));
        return {(width - w) / 2, (height - h) / 2, w, h};
    }
}
