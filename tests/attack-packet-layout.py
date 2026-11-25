#!/usr/bin/env python3
"""macOS regression: v83 attack packet layout against Cosmic parseDamage.

Serializes production AttackPacket objects and independently reads their fields
in the order used by Cosmic. No window or server connection.
"""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
build = root / 'build-macos'
subprocess.run(['cmake', '--build', str(build), '-j', '8'], cwd=root, check=True,
               stdout=subprocess.DEVNULL)
source = r'''

#include <vector>
#include <string>
#include "src/Template/Point.h"
#define private public
#include "src/Net/OutPacket.h"
#undef private
#include "src/Gameplay/Combat/Attack.h"
#include "src/Net/Packets/AttackAndSkillPackets.h"
#include <cassert>
#include <iostream>
using namespace ms;
int main(int,char**) {
 int cases=0;
 for(auto type:{Attack::CLOSE,Attack::RANGED,Attack::MAGIC})
 for(int id:{0,5001001,5001003,5201004,22121000,22151001,2121001,2221001,
             2321001,5201002,5101004,15101003,14111006,3121004,3221001,5221004,13111002})
 for(int charge:{0,700}) for(int targets:{0,1,3}) {
  Attack a; a.type=type; a.skill=id; a.hitcount=2; a.speed=6; a.stance=9;
  a.origin={123,-456}; AttackResult result(a); result.mobcount=targets; result.charge=charge;
  for(int i=0;i<targets;++i) result.damagelines[12345+i]={{321,false},{654,false}};
  AttackPacket packet(result); const auto& bytes=packet.bytes; size_t p=0;
  auto skip=[&](size_t n){assert(p+n<=bytes.size()); p+=n;};
  auto read=[&](int n){uint32_t v=0;for(int i=0;i<n;++i){assert(p<bytes.size());v|=uint32_t(uint8_t(bytes[p++]))<<(i*8);}return v;};
  assert(read(2)==(type==Attack::CLOSE?44:type==Attack::RANGED?45:46));
  skip(1);assert(read(1)==uint32_t((targets<<4)|2)); assert(read(4)==uint32_t(id));
  // This reader follows Cosmic parseDamage's field order independently.
  bool charged=id==22121000||id==22151001||id==2121001||id==2221001||id==2321001
      ||id==5201002||id==5101004||id==15101003||id==14111006;
  if(charged) assert(read(4)==uint32_t(charge));
  skip(8);skip(1);skip(1);assert(read(1)==9);skip(1);assert(read(1)==6);
  if(type==Attack::RANGED){skip(1);skip(1);skip(7);
    if(id==3121004||id==3221001||id==5221004||id==13111002) skip(4);
  } else skip(4);
  for(int i=0;i<targets;++i){auto oid=read(4);assert(oid>=12345 && oid<12345+targets);
    skip(14);assert(read(4)==321);assert(read(4)==654);skip(4);
  }
  if(id==14111006){skip(4);assert(read(2)==123);assert(read(2)==uint16_t(-456));}
  assert(p==bytes.size());++cases;
 }
 std::cout<<"PASS: "<<cases<<" production packets decoded in Cosmic field order, all bytes consumed\n";
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-world-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'WorldCheck.app'
    (app / 'Contents/MacOS').mkdir(parents=True)
    (app / 'Contents/Resources').mkdir()
    executable = app / 'Contents/MacOS/check'
    flags = (build / 'CMakeFiles/OpenStory.dir/flags.make').read_text()
    includes = next(line.split(' = ', 1)[1] for line in flags.splitlines()
                    if line.startswith('CXX_INCLUDES = '))
    link = shlex.split((build / 'CMakeFiles/OpenStory.dir/link.txt').read_text())
    link = [arg for arg in link if not arg.endswith('/src/MapleStory.cpp.o')]
    link[link.index('-o') + 1] = str(executable)
    link += ['-std=c++17', '-UNDEBUG', '-I' + str(root), *shlex.split(includes),
             str(cpp), '-Wl,-rpath,' + str(root / 'vendor/bass24/macos')]
    subprocess.run(link, cwd=build, check=True)
    output = build / 'world-map-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
