#!/usr/bin/env python3
"""macOS integration check: nearby NPC selection and shared cooldown.

Uses production CMake objects and NX assets, opens a temporary render window,
and keeps all Settings writes in a temporary directory. No server connection.
Checks production map objects without connecting to a server.
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
#include "src/Gameplay/MapleMap/MapNpcs.h"
#include "src/Gameplay/MapleMap/Npc.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Util/NxFiles.h"
#include "src/Net/NpcResponseTracker.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<Fullscreen>::get().save(false); assert(!Window::get().init());
 MapNpcs map;
 auto add = [&](int oid,int x,int y) {
  auto npc=std::make_unique<Npc>(2000,oid,false,0,false,Point<int16_t>(x,y));
  npc->makeactive(); map.get_npcs()->add(std::move(npc));
 };
 auto origin=Point<int16_t>(0,0);
 assert(map.find_talk_target(origin)==0);
 add(1,151,0); add(2,0,81);
 assert(map.find_talk_target(origin)==0);
 add(3,150,80); assert(map.find_talk_target(origin)==3);
 add(4,25,0); add(5,-25,0); assert(map.find_talk_target(origin)==4);
 map.remove(4); assert(map.find_talk_target(origin)==5);
 map.remove(5); assert(map.find_talk_target(origin)==3);
 add(6,0,0); assert(map.find_talk_target(origin)==6);
 map.clear(); assert(map.find_talk_target(origin)==0);
 auto& tracker=NpcResponseTracker::get();
 assert(tracker.can_click_now()); tracker.mark_clicked_now(); assert(!tracker.can_click_now());
 std::cout<<"PASS: nearest NPC, map range, inactive NPCs, stable ties, clear and cooldown\n";
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-npc-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'NpcCheck.app'
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
    output = build / 'npc-key-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
