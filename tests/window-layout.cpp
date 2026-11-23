#include "../src/IO/WindowLayout.h"
#include <cassert>
#include <cmath>

int main()
{
    for (const auto size : {ms::Viewport{0, 0, 1920, 1080},
                           ms::Viewport{0, 0, 1512, 982},
                           ms::Viewport{0, 0, 3440, 1440},
                           ms::Viewport{0, 0, 800, 1000},
                           ms::Viewport{0, 0, 3024, 1964}})
    {
        const auto v = ms::fit_viewport(size.width, size.height, 1280, 720);
        assert(v.x >= 0 && v.y >= 0);
        assert(v.x + v.width <= size.width && v.y + v.height <= size.height);
        assert(std::abs(v.width - v.height * 1280.0 / 720) < 2);
        assert(v.width == size.width || v.height == size.height);
    }
    const auto wide = ms::fit_viewport(1920, 1080, 1024, 768);
    assert(wide.x == 240 && wide.y == 0 && wide.width == 1440 && wide.height == 1080);
    assert(ms::fit_viewport(0, 0, 1280, 720).width == 0);
}
