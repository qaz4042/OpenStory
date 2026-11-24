#include "../src/Gameplay/TrainingAutoAttack.h"
#include "../src/Gameplay/TrainingAssistStatus.h"
#include <cassert>
#include <chrono>
#include <string>
using namespace std::chrono;
int main() {
    const auto start=ms::TrainingAutoAttack::Clock::now();
    for (int rate : {10,20,40}) {
        ms::TrainingAutoAttack state;
        ms::TrainingAssistStatus status;
        auto ack="[TrainingAssist:rapid:v1] 100 5001003 "+std::to_string(rate)+" 0 0 0 0 1";
        assert(state.accept(ack,100) && status.accept_rapid(ack,100));
        assert(state.active() && state.rapid_rate()==rate && status.rapid_known);
        int count=0;
        // Real 8ms game ticks must preserve the requested average, not round
        // every interval up to the next frame (20Hz would otherwise become 17.9).
        for (int ms=0;ms<10000;ms+=8) {
            if (ms%2000==0) state.accept(ack,100); // polling cannot reset phase
            if (state.ready(start+milliseconds(ms))) ++count;
        }
        assert(count==rate*10);
        assert(state.ready(start+seconds(60)));
        assert(!state.ready(start+seconds(60))); // no catch-up burst
        state.accept("[TrainingAssist:rapid:v1] 101 -1 0 0 0 0 0 0",100);
        state.accept("[TrainingAssist:rapid:v1] 100 0 40 0 0 0 0 1",100);
        state.accept("[TrainingAssist:rapid:v1] 100 5001003 60 0 0 0 0 1",100);
        state.accept(ack+" trailing",100);
        assert(state.active() && state.rapid_rate()==rate);
        state.accept("[TrainingAssist:auto:v1] 100 -1",100);
        assert(!state.active() && !state.rapid_rate());
        state.accept(ack,100);
        state.stop(); assert(!state.active() && !state.rapid_rate());
        status.clear(); assert(!status.rapid_known && !status.rapid_rate);
    }
    ms::TrainingAutoAttack normal;
    normal.accept("[TrainingAssist:auto:v1] 100 5001002",100);
    normal.accept("[TrainingAssist:rapid:v1] 100 -1 0 0 0 0 0 0",100);
    assert(normal.active() && !normal.rapid_rate());
}
