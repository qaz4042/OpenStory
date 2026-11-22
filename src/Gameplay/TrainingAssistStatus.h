#pragma once
#include "TrainingAutoAttack.h"
#include <chrono>
#include <sstream>
#include <string>

namespace ms
{
    struct TrainingAssistStatus
    {
        bool known = false, allowed = false, invincible = false, silence = false, vacuum = false;
        int auto_skill = -1, radius = 800, limit = 20;
        std::string stop_reason;
        unsigned revision = 0;
        std::chrono::steady_clock::time_point received{};
        bool accept(const std::string& message, int current_map)
        {
            const std::string prefix = "[TrainingAssist:state:v1] ";
            if (message.compare(0, prefix.size(), prefix) != 0) return false;
            int map, a, i, s, v, skill, range, count;
            std::string extra;
            std::istringstream input(message.substr(prefix.size()));
            if (!(input >> map >> a >> i >> s >> v >> skill >> range >> count) || (input >> extra)) return true;
            if (map != current_map || a < 0 || a > 1 || i < 0 || i > 1 || s < 0 || s > 1
                || v < 0 || v > 1 || (skill != -1 && !TrainingAutoAttack::supported(skill))
                || range < 200 || range > 800 || count < 1 || count > 20) return true;
            allowed = a; invincible = i; silence = s; vacuum = v;
            auto_skill = skill; radius = range; limit = count; known = true;
            received = std::chrono::steady_clock::now(); ++revision;
            return true;
        }
        bool fresh() const
        {
            return known && std::chrono::steady_clock::now() - received < std::chrono::seconds(6);
        }
        void clear() { known = false; stop_reason.clear(); ++revision; }
    };
}
