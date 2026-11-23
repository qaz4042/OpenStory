#pragma once
#include <string_view>

namespace ms
{
    // v83 names occupy 13 bytes including the terminating NUL.
    // Accept ASCII letters/digits and basic CJK, with at least 3 characters.
    inline bool valid_character_name_format(std::string_view name)
    {
        if (name.size() > 12) return false;
        unsigned count = 0;
        for (size_t i = 0; i < name.size(); ++count)
        {
            const auto c = static_cast<unsigned char>(name[i]);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9')) { ++i; continue; }
            if (i + 2 >= name.size() || c < 0xE4 || c > 0xE9) return false;
            const auto b = static_cast<unsigned char>(name[i + 1]);
            const auto d = static_cast<unsigned char>(name[i + 2]);
            if ((b & 0xC0) != 0x80 || (d & 0xC0) != 0x80) return false;
            const unsigned cp = ((c & 0x0F) << 12) | ((b & 0x3F) << 6) | (d & 0x3F);
            if (cp < 0x4E00 || cp > 0x9FFF) return false;
            i += 3;
        }
        return count >= 3;
    }
}
