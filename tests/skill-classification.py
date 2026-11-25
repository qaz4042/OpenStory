#!/usr/bin/env python3
"""Audit all NX skill classifications and cross-class regressions (macOS).

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

def run(command, cwd=root):
    result = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE,
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
    int total=0, attacks=0, guarded=0;
    for(nl::node job:nl::nx::skill) for(nl::node skill:job["skill"]) {
        int id=std::stoi(skill.name()); const auto& d=SkillData::get(id); ++total;
        if(d.is_attack()) ++attacks;
        if(d.is_passive() || skill["skillType"].get_integer(-1)==2
           || skill["skillType"].get_integer(-1)==3 || skill["summon"] || id==12111005) {
            if(d.is_attack()) { std::cerr<<"Unexpected direct attack "<<id<<"\n"; return 1; }
            ++guarded;
        }
    }
    for(int id : {10001000,11001002,11001003,1111006,1111008,11101004,11111002,11111003,11111004,11111006,12001003,12101002,12101006,12111003,12111006,13001003,13101005,13111000,13111001,13111002,13111006,13111007,14001004,14101006,14111002,14111005,14111006,15001001,15001002,15101003,15101005,15111001,15111003,15111004,15111006,15111007,20001000,21111005,2121001,2201004,2201005,2211002,2211003,2211006,2221001,2221003,2221006,2221007,2301005,2311004,2321001,2321007,2321008,3101003,3111003,3111004,3111006,3121003,3121004,3201003,3201005,3211003,3211004,3211006,3221001,3221003,4101005,4111005,4121007,4121008,4201004,4201005,4211002,4211004,4221001,4221007,5111002,5111004,5111006,5121001,5121002,5121004,5121005,5121007,5201002,5201006,5211004,5211005,5211006,5221003,5221004,5221007,5221008,9001001}) {
        assert(SkillData::get(id).is_attack());
        assert(!SkillData::get(id).is_passive());
    }
    for(int id : {5001005,1101006,1111002,1211003,1211009,1311007,
                  21101003,11101002,13101002,5211001,5211002,5221006,
                  2111003,12111005,2301002,4211006})
        assert(!SkillData::get(id).is_attack());
    for(int id : {5001001,5001002,5101002,5101003,5101004,5111002,5121001})
        assert(SkillData::get(id).is_melee());
    for(int id : {5001003,5201001,5201004,5201006,5211004,5221007})
        assert(SkillData::get(id).is_attack() && !SkillData::get(id).is_melee());
    std::cout<<"PASS: "<<total<<" skills scanned, "<<attacks<<" attacks, "<<guarded
             <<" passive/buff/summon/field guards; 94 previously omitted attacks and melee/ranged controls\n";

}
'''

with tempfile.TemporaryDirectory(prefix='openstory-pirate-') as directory:
    work = Path(directory)
    source = work / 'check.cpp'
    source.write_text(CHECK)
    objects = build / 'CMakeFiles/OpenStory.dir'
    names = ['src/Data/SkillData', 'src/Character/Inventory/Weapon', 'src/Util/Misc', 'src/Configuration']
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
    print(run([str(executable), str(data)], cwd=work), end='')
