#pragma once
#include <algorithm>
#include <string_view>

namespace ms
{
    // Cosmic Character.canCreateChar: [a-zA-Z0-9]{3,12}.
    inline bool valid_character_name_format(std::string_view name)
    {
        return name.size() >= 3 && name.size() <= 12
            && std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                    || (c >= '0' && c <= '9');
            });
    }
}
