#pragma once
#include "TrainingAutoAttack.h"
#include <chrono>
#include <sstream>
#include <string>

namespace ms
{
    struct TrainingAssistStatus
    {
        bool rapid_known = false;
        int rapid_rate = 0, rapid_skill = -1;
        unsigned long long rapid_accepted=0, rapid_rejected=0, rapid_lines=0, rapid_damage=0, rapid_elapsed=0;
        bool accept_rapid(const std::string& message, int current_map) {
            const std::string prefix="[TrainingAssist:rapid:v1] ";
            if (message.compare(0,prefix.size(),prefix)!=0) return false;
            int map,id,rate; long long accepted,rejected,lines,damage,elapsed;
            std::string extra; std::istringstream input(message.substr(prefix.size()));
            if ((input>>map>>id>>rate>>accepted>>rejected>>lines>>damage>>elapsed) && !(input>>extra)
                && accepted>=0 && rejected>=0 && lines>=0 && damage>=0 && elapsed>=0
                && map==current_map && (rate==0 || rate==10 || rate==20 || rate==40)
                && (rate==0 ? id==-1 : TrainingAutoAttack::rapid_supported(id))) {
                rapid_known=true; rapid_rate=rate; rapid_skill=id; rapid_accepted=accepted;
                rapid_rejected=rejected; rapid_lines=lines; rapid_damage=damage; rapid_elapsed=elapsed; ++revision;
            }
            return true;
        }
        bool pet_vacuum = false, pet_vacuum_known = false;
        bool pet_vacuum_mesos = false, pet_vacuum_items = false, pet_vacuum_split_known = false;
        bool known = false, allowed = false, invincible = false, silence = false, vacuum = false;
        int auto_skill = -1, radius = 0, limit = 0;
        int vacuum_pinned = -1, vacuum_eligible = -1;
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
                || !((range == 0 && count == 0) || (range >= 200 && range <= 800 && count >= 1 && count <= 20))) return true;
            allowed = a; invincible = i; silence = s; vacuum = v;
            auto_skill = skill; radius = range; limit = count; known = true;
            received = std::chrono::steady_clock::now(); ++revision;
            return true;
        }
        bool accept_pet_vacuum(const std::string& message, int current_map)
        {
            const std::string split = "[TrainingAssist:petvac:v2] ";
            if (message.compare(0,split.size(),split)==0) {
                int map,mask; std::string extra;
                std::istringstream input(message.substr(split.size()));
                if ((input>>map>>mask) && !(input>>extra) && map==current_map && mask>=0 && mask<=3) {
                    pet_vacuum_mesos=mask&1; pet_vacuum_items=mask&2;
                    pet_vacuum=mask!=0; pet_vacuum_known=pet_vacuum_split_known=true; ++revision;
                }
                return true;
            }
            const std::string prefix = "[TrainingAssist:petvac:v1] ";
            if (message.compare(0, prefix.size(), prefix) != 0) return false;
            int map, on; std::string extra;
            std::istringstream input(message.substr(prefix.size()));
            if ((input >> map >> on) && !(input >> extra) && map == current_map && (on == 0 || on == 1)) {
                pet_vacuum = on; pet_vacuum_known = true; ++revision;
            }
            return true;
        }
        bool accept_vacuum_count(const std::string& message, int current_map)
        {
            const std::string prefix = "[TrainingAssist:vac-count:v1] ";
            if (message.compare(0, prefix.size(), prefix) != 0) return false;
            int map, pinned, eligible;
            std::string extra;
            std::istringstream input(message.substr(prefix.size()));
            if ((input >> map >> pinned >> eligible) && !(input >> extra)
                && map == current_map && pinned >= 0 && eligible >= pinned) {
                vacuum_pinned = pinned; vacuum_eligible = eligible;
            }
            return true;
        }
        bool fresh() const
        {
            return known && std::chrono::steady_clock::now() - received < std::chrono::seconds(6);
        }
        void clear() { rapid_known=false; rapid_rate=0; rapid_skill=-1; pet_vacuum_mesos = pet_vacuum_items = pet_vacuum_split_known = false; pet_vacuum = pet_vacuum_known = false; known = false; vacuum_pinned = vacuum_eligible = -1; stop_reason.clear(); ++revision; }
    };
}
