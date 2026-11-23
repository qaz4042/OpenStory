#pragma once
#include "TrainingSkillSupport.h"
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <set>
namespace ms {
class TrainingAutoAttack {
public:
    using Clock = std::chrono::steady_clock;
    struct Entry { int skill, interval; Clock::time_point next{}; };
    static bool supported(int32_t id) { return training_skill(id); }
    bool accept(const std::string& notice, int32_t map) {
        const std::string rotation="[TrainingAssist:rotation:v1] ";
        if (notice.compare(0,rotation.size(),rotation)==0) {
            std::istringstream input(notice.substr(rotation.size()));
            int source; if (!(input>>source) || source!=map) return true;
            std::vector<Entry> parsed; std::set<int> seen; std::string token;
            while (input>>token) {
                auto colon=token.find(':'); if (colon==std::string::npos) return true;
                int id,ms; char extra; std::istringstream a(token.substr(0,colon)), b(token.substr(colon+1));
                if (!(a>>id) || (a>>extra) || !(b>>ms) || (b>>extra) || !supported(id)
                    || ms<200 || ms>600000 || !seen.insert(id).second || parsed.size()>=4) return true;
                parsed.push_back({id,ms,{}});
            }
            if (parsed.empty()) return true;
            bool same=parsed.size()==entries_.size();
            if (same) for (size_t i=0;i<parsed.size();++i)
                if (parsed[i].skill!=entries_[i].skill || parsed[i].interval!=entries_[i].interval) same=false;
            if (!same) { entries_=parsed; cursor_=0; next_={}; }
            skill_=entries_.front().skill;
            return true;
        }
        const std::string prefix="[TrainingAssist:auto:v1] ";
        if (notice.compare(0,prefix.size(),prefix)!=0) return false;
        int source,id; std::string extra; std::istringstream input(notice.substr(prefix.size()));
        if (!(input>>source>>id) || (input>>extra) || source!=map) return true;
        if (id==-1) stop();
        else if (supported(id)) { skill_=id; entries_={{id,200,{}}}; cursor_=0; next_={}; }
        return true;
    }
    bool active() const { return skill_>=0; }
    int32_t skill() const { return skill_; }
    void stop() { skill_=-1; entries_.clear(); cursor_=0; }
    bool ready(Clock::time_point now) {
        if (!active() || now<next_ || entries_.empty()) return false;
        for (size_t n=0;n<entries_.size();++n) {
            size_t i=(cursor_+n)%entries_.size();
            if (now<entries_[i].next) continue;
            selected_=i; cursor_=(i+1)%entries_.size(); skill_=entries_[i].skill;
            entries_[i].next=now+std::chrono::milliseconds(entries_[i].interval);
            next_=now+std::chrono::milliseconds(200); return true;
        }
        return false;
    }
    void retry(Clock::time_point now) {
        if (selected_<entries_.size()) entries_[selected_].next=now+std::chrono::milliseconds(200);
    }
private:
    int skill_=-1;
    size_t cursor_=0,selected_=0;
    std::vector<Entry> entries_;
    Clock::time_point next_{};
};
}
