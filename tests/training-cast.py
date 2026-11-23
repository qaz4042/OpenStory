#!/usr/bin/env python3
"""macOS production-object regression for normal and skill auto casting.

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
#include "src/Gameplay/Combat/Combat.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Util/NxFiles.h"
#include "src/Audio/Audio.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<Fullscreen>::get().save(false); assert(!Window::get().init()); assert(!Sound::init());
 CharLook::init();
 CharEntry entry{}; entry.look.faceid=20000; entry.look.hairid=30000;
 entry.look.equips[-11]=1482000;
 entry.stats.stats[MapleStat::Id::JOB]=500;
 entry.stats.stats[MapleStat::Id::HP]=1000;
 entry.stats.stats[MapleStat::Id::MP]=1000;
 entry.stats.stats[MapleStat::Id::LEVEL]=30;
 Player player(entry); player.respawn({0,0},false);
 player.change_skill(5001002,20,20,-1);
 Physics physics; MapChars chars; MapMobs mobs; MapReactors reactors;
 Combat combat(player,chars,mobs,reactors,physics);
 std::string reason;
 assert(player.can_attack());
 assert(combat.auto_use_move(0,reason));
 assert(player.is_attacking());
 // Busy actions report a deferred cast so rotation retries without consuming a buff interval.
 assert(!combat.auto_use_move(0,reason));
 assert(reason=="等待动作结束");
 player.respawn({0,0},false);
 assert(combat.auto_use_move(5001002,reason));
 assert(player.is_attacking());
 player.respawn({0,0},false);
 player.get_stats().set_stat(MapleStat::Id::MP,0);
 assert(!combat.auto_use_move(5001002,reason));
 assert(reason=="MP不足");
 entry.look.equips[-11]=1492000;
 Player gun(entry); gun.respawn({0,0},false);
 Combat gun_combat(gun,chars,mobs,reactors,physics);
 reason.clear();
 assert(!gun_combat.auto_use_move(0,reason)); // gun attacks still require ammunition
 assert(reason=="弹药不足");
 gun.change_skill(5001005,10,10,-1);
 gun.respawn({0,0},false);
 assert(gun_combat.auto_use_move(5001005,reason)); // Dash uses MP, never ammunition
 std::cout<<"PASS: production normal attack and learned Somersault Kick cast without targets; action lock and MP checks retained\n";
 Sound::close();
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-auto-cast-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'TrainingCastCheck.app'
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
    output = build / 'training-cast-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
