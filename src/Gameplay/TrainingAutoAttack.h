#pragma once

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>

namespace ms
{
    // No persisted setting: a private server acknowledgement starts each session.
    class TrainingAutoAttack
    {
    public:
        using Clock = std::chrono::steady_clock;
        static bool supported(int32_t skill)
        {
            return skill == 0 || skill == 5001001 || skill == 5001002 || skill == 5001003;
        }
        bool accept(const std::string& notice, int32_t map)
        {
            const std::string prefix = "[TrainingAssist:auto:v1] ";
            if (notice.compare(0, prefix.size(), prefix) != 0) return false;
            std::istringstream input(notice.substr(prefix.size()));
            int32_t ack_map, ack_skill;
            std::string extra;
            if (!(input >> ack_map >> ack_skill) || (input >> extra)) return true;
            if (ack_map != map) return true;
            if (ack_skill == -1) stop();
            else if (supported(ack_skill)) { skill_ = ack_skill; next_ = Clock::time_point{}; }
            return true;
        }
        bool active() const { return skill_ >= 0; }
        int32_t skill() const { return skill_; }
        void stop() { skill_ = -1; }
        bool ready(Clock::time_point now)
        {
            if (!active() || now < next_) return false;
            next_ = now + std::chrono::milliseconds(200);
            return true;
        }
    private:
        int32_t skill_ = -1;
        Clock::time_point next_{};
    };
}
