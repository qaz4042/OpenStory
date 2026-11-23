#pragma once
#include "../Template/Point.h"
#include "../Template/Range.h"
#include "../Constants.h"
#include <algorithm>
#include <cmath>
namespace ms {
// Virtual flight accelerates toward screen edges; real flight is fixed at 2x.
inline double flight_speed(int mode, Point<int16_t> pointer, int width, int height) {
    if (mode == 2) return 960.0;
    const double cx = width / 2.0, cy = height / 2.0;
    const double edge = std::clamp(std::max(std::abs(pointer.x()-cx) / std::max(1.0,cx-1),
                                          std::abs(pointer.y()-cy) / std::max(1.0,cy-1)), 0.0, 1.0);
    return 480.0 * (1.0 + 4.0 * edge);
}
// Fixed simulation step, normalized diagonals, 8px dead zone.
inline Point<double> flight_next(Point<double> p, double dx, double dy,
                                  Range<int16_t> walls, Range<int16_t> borders, double speed = 480.0) {
    double distance = std::hypot(dx, dy);
    if (distance <= 8) return p;
    double step = std::min(distance-8, speed * Constants::TIMESTEP / 1000.0);
    return {std::clamp(p.x()+dx/distance*step, double(walls.smaller()), double(walls.greater())),
            std::clamp(p.y()+dy/distance*step, double(borders.smaller()), double(borders.greater()))};
}
}
