#pragma once

#include "Messages.h"
#include "../Configuration.h"
#include <string>

namespace ms::localization
{
    inline const char* get(Key key)
    {
        return text(key, Setting<UILanguage>::get().load());
    }

    // Named substitution keeps sentence order in the translation catalog.
    inline std::string format(Key key, const std::string& value)
    {
        std::string result = get(key);
        const auto at = result.find("{value}");
        if (at != std::string::npos) result.replace(at, 7, value);
        return result;
    }
}
