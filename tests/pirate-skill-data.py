#!/usr/bin/env python3
"""Check pirate skill metadata against actual NX assets (macOS CMake build).

Only Texture construction is stubbed; SkillData, ID formatting, weapon mapping,
and NX reads use the production objects. No game or server connection is needed.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=Path('build-macos'))
parser.add_argument('--data-dir', type=Path, default=Path('wz'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = (root / args.build_dir).resolve()
data = (root / args.data_dir).resolve()

def run(command):
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit(result.stdout)
    return result.stdout

# Refresh objects so the check always tests the current source.
run(['cmake', '--build', str(build), '-j', '8'])

CHECK = r'''

#include "src/Data/SkillData.h"
#include "src/Character/SkillId.h"
#include <nlnx/file.hpp>
#include <nlnx/nx.hpp>
#include <cassert>
#include <iostream>
namespace ms { Texture::Texture(nl::node) {} }
int main(int argc, char** argv) {
    nl::file skills(std::string(argv[1]) + "/Skill.nx");
    nl::file strings(std::string(argv[1]) + "/String.nx");
    nl::nx::skill = skills; nl::nx::string = strings;
    using namespace ms;
    for (int id : {SkillId::FLASH_FIST, SkillId::SOMERSAULT_KICK, SkillId::DOUBLE_SHOT}) {
        const auto& s = SkillData::get(id);
        assert(s.is_attack()); assert(!s.is_passive());
        assert(s.get_masterlevel() == 20);
        assert(s.get_stats(1).damage > 0);
    }
    const auto& fist = SkillData::get(SkillId::FLASH_FIST);
    const auto& kick = SkillData::get(SkillId::SOMERSAULT_KICK);
    const auto& shot = SkillData::get(SkillId::DOUBLE_SHOT);
    assert(fist.is_melee() && kick.is_melee() && !shot.is_melee());
    assert(fist.get_required_weapon() == Weapon::NONE);
    assert(kick.get_required_weapon() == Weapon::NONE);
    assert(shot.get_required_weapon() == Weapon::GUN);
    assert(fist.get_stats(1).mobcount == 1);
    assert(kick.get_stats(1).mobcount == 3 && kick.get_stats(20).mobcount == 6);
    assert(shot.get_stats(1).bulletcount == 2 && shot.get_stats(20).bulletcount == 2);
    assert(fist.get_stats(1).range.left() == -74);
    assert(kick.get_stats(1).range.left() == -81);
    assert(!SkillData::get(5001005).is_attack()); // Dash stays a buff
    assert(SkillData::get(5000000).is_passive()); // Bullet Time stays passive
    assert(SkillData::get(SkillId::POWER_STRIKE).is_attack());
    std::cout << "PASS: actual Skill.nx classification, weapon requirement, levels, hit counts, ranges and non-attack controls\n";
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-pirate-') as directory:
    work = Path(directory)
    source = work / 'check.cpp'
    source.write_text(CHECK)
    objects = build / 'CMakeFiles/OpenStory.dir'
    names = ['src/Data/SkillData', 'src/Character/Inventory/Weapon', 'src/Util/Misc']
    names += ['vendor/NoLifeNx/nlnx/' + name
              for name in ('node', 'file', 'bitmap', 'audio', 'nx')]
    brew = Path('/opt/homebrew') if Path('/opt/homebrew').exists() else Path('/usr/local')
    executable = work / 'check'
    run(['c++', '-std=c++17', '-Wno-deprecated-declarations',
         '-mmacosx-version-min=15.0', '-I' + str(root),
         '-I' + str(root / 'vendor/NoLifeNx'),
         '-I' + str(brew / 'opt/glew/include/GL'), str(source),
         *[str(objects / (name + '.cpp.o')) for name in names],
         '-L' + str(brew / 'lib'), '-llz4', '-Wl,-dead_strip', '-o', str(executable)])
    print(run([str(executable), str(data)]), end='')
