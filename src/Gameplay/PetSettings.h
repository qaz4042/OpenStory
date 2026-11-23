#pragma once
#include <algorithm>
#include <cstdint>

namespace ms
{
    // Shared by pet settings UI and automatic recovery. Zero disables recovery.
    inline int pet_potion_threshold(int percentage)
    {
        return std::clamp(percentage, 0, 95);
    }

    inline bool pet_needs_potion(int32_t current, int32_t maximum, int percentage)
    {
        const int threshold = pet_potion_threshold(percentage);
        return threshold > 0 && maximum > 0 && current >= 0 && current < maximum
            && int64_t(current) * 100 <= int64_t(maximum) * threshold;
    }
}
