#include "../src/Gameplay/TrainingAutoAttack.h"
#include "../src/Gameplay/TrainingAssistStatus.h"
#include <cassert>
#include <chrono>
int main() {
    ms::TrainingAutoAttack state;
    const auto now = ms::TrainingAutoAttack::Clock::now();
    assert(!state.active() && !state.ready(now));
    assert(!state.accept("normal chat", 100));
    assert(state.accept("[TrainingAssist:auto:v1] 101 0", 100));
    assert(!state.active());
    for (const char* bad : {"", "100", "100 42", "100 -2", "100 0 trailing", "x y", "100 999999999999999"}) {
        assert(state.accept(std::string("[TrainingAssist:auto:v1] ") + bad, 100));
        assert(!state.active());
    }
    assert(state.accept("[TrainingAssist:auto:v1] 100 5001001", 100));
    assert(state.active() && state.skill() == 5001001);
    assert(state.ready(now));
    assert(!state.ready(now));
    assert(!state.ready(now + std::chrono::milliseconds(199)));
    assert(state.ready(now + std::chrono::milliseconds(200)));
    // Resuming after a long pause never bursts catch-up attacks.
    assert(state.ready(now + std::chrono::seconds(60)));
    assert(!state.ready(now + std::chrono::seconds(60)));
    assert(state.accept("[TrainingAssist:auto:v1] 101 -1", 100));
    assert(state.active());
    state.stop(); // same operation used by manual input, death, map clear and disconnect
    assert(!state.active());
    assert(!state.ready(now + std::chrono::seconds(61)));
    state.accept("[TrainingAssist:auto:v1] 100 0", 100);
    assert(state.active() && state.skill() == 0);
    state.accept("[TrainingAssist:auto:v1] 100 -1", 100);
    assert(!state.active());
    ms::TrainingAutoAttack rotation;
    assert(rotation.accept("[TrainingAssist:rotation:v1] 100 5001002:1000 5101006:60000",100));
    assert(rotation.ready(now) && rotation.skill()==5001002);
    assert(!rotation.ready(now));
    assert(rotation.ready(now+std::chrono::milliseconds(200)) && rotation.skill()==5101006);
    // Polling the same plan preserves timers, especially long buff intervals.
    rotation.accept("[TrainingAssist:rotation:v1] 100 5001002:1000 5101006:60000",100);
    assert(!rotation.ready(now+std::chrono::milliseconds(400)));
    assert(rotation.ready(now+std::chrono::milliseconds(1000)) && rotation.skill()==5001002);
    rotation.retry(now+std::chrono::milliseconds(1000));
    assert(rotation.ready(now+std::chrono::milliseconds(1200)) && rotation.skill()==5001002);
    rotation.accept("[TrainingAssist:rotation:v1] 100 5001002:1",100);
    assert(rotation.active()); // malformed plan cannot replace existing settings
    rotation.stop(); assert(!rotation.ready(now+std::chrono::seconds(600)));
    ms::TrainingAssistStatus status;
    assert(!status.fresh());
    assert(!status.accept("unrelated",100));
    assert(status.accept("[TrainingAssist:state:v1] 101 1 0 1 0 -1 800 20",100));
    assert(!status.known);
    assert(status.accept("[TrainingAssist:state:v1] 100 1 0 1 0 -1 9999 20",100));
    assert(!status.known);
    assert(status.accept("[TrainingAssist:state:v1] 100 1 0 1 0 -1 800 20",100));
    assert(status.fresh() && status.allowed && status.silence && !status.invincible);
    status.received -= std::chrono::seconds(7);
    assert(!status.fresh());
    const auto revision = status.revision;
    status.accept("[TrainingAssist:state:v1] 100 1 0 1 0 -1 800 20 trailing",100);
    assert(status.revision==revision);
    status.clear(); assert(!status.known && !status.fresh());

    status.accept("[TrainingAssist:state:v1] 100 1 0 0 1 -1 0 0",100);
    assert(status.known && status.radius==0 && status.limit==0);
    status.accept_vacuum_count("[TrainingAssist:vac-count:v1] 100 125 130",100);
    assert(status.vacuum_pinned==125 && status.vacuum_eligible==130);
    status.accept_vacuum_count("[TrainingAssist:vac-count:v1] 101 1 2",100);
    status.accept_vacuum_count("[TrainingAssist:vac-count:v1] 100 3 2",100);
    assert(status.vacuum_pinned==125);
    status.clear(); assert(status.vacuum_pinned==-1);

}
