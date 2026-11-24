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

#include "src/Gameplay/MapleMap/MapObject.h"
#include "src/Character/Look/CharLook.h"
#include "src/Graphics/Animation.h"
#include "src/Graphics/Text.h"
#include "src/Util/Randomizer.h"
#include "src/IO/Components/ChatBalloon.h"
#include "src/Template/Singleton.h"
#include <chrono>
#include <map>
#include <unordered_set>
#define private public
#include "src/Gameplay/MapleMap/Npc.h"
#include "src/Net/NpcResponseTracker.h"
#undef private
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Gameplay/Stage.h"
#include "src/Character/QuestAvailability.h"
#include "src/Util/NxFiles.h"
#include <nlnx/nx.hpp>
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false);assert(!Window::get().init());
 auto& player=Stage::get().get_player();auto& inv=player.get_inventory();auto& quests=player.get_quests();
 player.get_stats().set_stat(MapleStat::LEVEL,11);player.get_stats().set_stat(MapleStat::JOB,0);
 Npc mai(12100,1,false,0,false,{}),yoona(20100,2,false,0,false,{});
 mai.update_quest_mark();yoona.update_quest_mark();
 assert(mai.quest_mark_type==QuestMarkType::NONE && yoona.quest_mark_type==QuestMarkType::NONE);
 auto check=nl::nx::quest["Check.img"];
 assert(!quest_start_requirements_met(check["8000"]["0"],0,quests,inv));
 inv.add_item(InventoryType::EQUIP,1,1042003,false,-1,1,"",0);
 assert(quest_start_requirements_met(check["8000"]["0"],0,quests,inv));
 assert(!quest_start_requirements_met(check["8000"]["0"],100,quests,inv));
 assert(!quest_start_requirements_met(check["8021"]["0"],0,quests,inv));
 quests.add_completed(8020,0);assert(quest_start_requirements_met(check["8021"]["0"],0,quests,inv));
 mai.update_quest_mark();assert(mai.quest_mark_type==QuestMarkType::AVAILABLE);
 // Expired response request must never hide this legitimately available quest.
 auto& tracker=NpcResponseTracker::get();tracker.mark_pending(12100);
 tracker.pending_[12100]=std::chrono::steady_clock::now()-std::chrono::seconds(4);
 auto rev=tracker.revision();assert(!tracker.tick());assert(tracker.pending_.empty());
 assert(!tracker.is_unavailable(12100) && tracker.revision()==rev);
 mai.update_quest_mark();assert(mai.quest_mark_type==QuestMarkType::AVAILABLE);
 // Required equipment must be in the bag, not merely worn.
 inv.modify(InventoryType::EQUIP,1,2,-5,Inventory::Movement::MOVE_EQUIP);
 assert(!quest_start_requirements_met(check["8000"]["0"],0,quests,inv));
 std::cout<<"PASS: real Mai/Yoona level11 false bulbs, item/job/prerequisite gates, worn equipment exclusion and timeout preserving valid bulb\n";
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
