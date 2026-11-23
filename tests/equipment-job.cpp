// Run from the repository root:
// c++ -std=c++17 tests/equipment-job.cpp src/Character/Job.cpp -o /tmp/openstory-equipment-job-test
// /tmp/openstory-equipment-job-test
#include "../src/Character/Job.h"
#include <cassert>

int main()
{
    // Screenshot regression: a first-job pirate can wear reqJob=16 knuckles.
    assert(ms::Job(500).can_equip(16));
    for (int job : {500, 510, 511, 512, 520, 521, 522, 1500, 1512})
    {
        assert(ms::Job(job).can_equip(16));
        assert(!ms::Job(job).can_equip(1));
        assert(!ms::Job(job).can_equip(-1));
    }
    for (int job : {0, 1000, 2000})
    {
        assert(ms::Job(job).can_equip(0));
        assert(ms::Job(job).can_equip(-1));
        assert(!ms::Job(job).can_equip(16));
    }
    for (int job : {100, 112, 1100, 1112, 2100, 2112})
        assert(ms::Job(job).can_equip(1));
    for (int family = 1; family <= 5; ++family)
    {
        for (int job : {family * 100, 1000 + family * 100})
        {
            assert(ms::Job(job).can_equip(0));
            for (int mask = 1; mask < 32; ++mask)
                assert(ms::Job(job).can_equip(mask) == ((mask & (1 << (family - 1))) != 0));
        }
    }
    for (int job : {900, 910})
        assert(ms::Job(job).can_equip(16));
    // Skill ancestry still uses job IDs, independently of equipment masks.
    assert(ms::Job(500).can_use(5001001));
    assert(!ms::Job(500).can_use(5101001));
}
