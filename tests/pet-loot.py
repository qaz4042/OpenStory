#!/usr/bin/env python3
"""Exercise real pet pickup decisions and accessory double-click packets offline."""
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
build = root / 'build-macos'
subprocess.run(['cmake', '--build', str(build), '-j', '8'], cwd=root, check=True, stdout=subprocess.DEVNULL)
source = r'''

#include "src/Configuration.h"
#include "src/IO/Window.h"
#include "src/IO/UITypes/UIItemInventory.h"
#include "src/Gameplay/Stage.h"
#include "src/Gameplay/MapleMap/ItemDrop.h"
#include "src/Gameplay/MapleMap/MesoDrop.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
namespace ms { extern std::vector<std::vector<int8_t>> captured; }
class Items : public UIItemInventory {
public: using UIItemInventory::UIItemInventory;
 void click(int slot) { doubleclick(position+Point<int16_t>(20+(slot-1)*36,60)); }
};
int value(const std::vector<int8_t>& p,int at,int n) {
 unsigned v=0;for(int i=0;i<n;++i)v|=unsigned(uint8_t(p[at+i]))<<(8*i);return v;
}
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false);assert(!Window::get().init());
 auto& player=Stage::get().get_player();player.set_position(0,0);
 auto& inventory=player.get_inventory();inventory.set_slotmax(InventoryType::EQUIP,24);
 for(int i=0;i<2;++i) inventory.add_item(InventoryType::EQUIP,1+i,1812000+i,true,-1,1,"",0);
 Items ui(inventory);ui.change_tab(InventoryType::EQUIP);
 for(int i=0;i<2;++i) {
  captured.clear();ui.click(i+1);assert(captured.size()==1);
  const auto& p=captured[0];assert(value(p,0,2)==71 && p[6]==1);
  assert(value(p,7,2)==i+1);assert(int16_t(value(p,9,2))==-181-i);
  assert(!inventory.find_item(InventoryType::EQUIPPED,1812000+i));
  inventory.modify(InventoryType::EQUIP,1+i,2,-181-i,Inventory::Movement::MOVE_EQUIP);
  captured.clear();ui.click(i+1);assert(captured.empty());
 }
 player.add_pet(0,5000066,"pet",123,{0,0},2,0);
 auto& pet=player.get_pet(0);
 MapDrops drops;PetAI ai;const Texture texture;const Animation animation;
 auto item=[&](int id,int x,int y=0,bool discard=false,int mode=2) {
  drops.get_drops()->add(std::make_unique<ItemDrop>(id,0,Point<int16_t>(x,y),Point<int16_t>(x,y),0,mode,2000000,discard,texture));
 };
 auto reset=[&](){ drops.clear();ai.reset();captured.clear();pet.set_position(0,0);pet.clear_loot_target(); };
 Setting<PetLootMesos>::get().save(true);Setting<PetLootItems>::get().save(true);
 item(1,85);ai.update(player,drops);assert(captured.size()==1 && value(captured[0],19,4)==1);
 item(2,40);for(int n=0;n<10;++n)ai.update(player,drops);
 assert(captured.size()==2 && value(captured[1],19,4)==2); // pending #1 cannot block #2
 for(int n=0;n<60;++n)ai.update(player,drops);assert(captured.size()>=3); // rejection retries
 reset();item(3,130);ai.update(player,drops);assert(captured.empty() && pet.has_loot_target);
 pet.set_position(50,0);for(int n=0;n<10;++n)ai.update(player,drops);assert(captured.size()==1);
 reset();item(4,151);item(5,20,70);item(6,10,0,true);item(7,10,0,false,0);
 ai.update(player,drops);assert(captured.empty() && !pet.has_loot_target);
 reset();Setting<PetLootItems>::get().save(false);item(8,10);
 drops.get_drops()->add(std::make_unique<MesoDrop>(9,0,Point<int16_t>(60,0),Point<int16_t>(60,0),0,2,false,animation));
 ai.update(player,drops);assert(captured.size()==1 && value(captured[0],19,4)==9);
 reset();Setting<PetLootMesos>::get().save(false);Setting<PetLootItems>::get().save(true);item(10,30);
 drops.get_drops()->add(std::make_unique<MesoDrop>(11,0,Point<int16_t>(10,0),Point<int16_t>(10,0),0,2,false,animation));
 ai.update(player,drops);assert(captured.size()==1 && value(captured[0],19,4)==10);
 reset();inventory.modify(InventoryType::EQUIPPED,-182,2,2,Inventory::Movement::MOVE_UNEQUIP);
 item(12,20);ai.update(player,drops);assert(captured.empty() && !pet.has_loot_target);
 Setting<PetLootItems>::get().save(false);pet.set_loot_target({100,0});captured.clear();ai.update(player,drops);
 assert(captured.empty() && !pet.has_loot_target);
 std::cout<<"PASS: real accessory double-click packets; stationary nearby pickup, chase, retries, filters, range and landing exclusions\n";
}
'''
with tempfile.TemporaryDirectory(prefix='openstory-item-drag-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'ItemDragCheck.app'
    (app / 'Contents/MacOS').mkdir(parents=True)
    (app / 'Contents/Resources').mkdir()
    executable = app / 'Contents/MacOS/check'
    flags = (build / 'CMakeFiles/OpenStory.dir/flags.make').read_text()
    includes = next(line.split(' = ', 1)[1] for line in flags.splitlines()
                    if line.startswith('CXX_INCLUDES = '))
    link = shlex.split((build / 'CMakeFiles/OpenStory.dir/link.txt').read_text())
    link = [arg for arg in link if not arg.endswith(('/src/MapleStory.cpp.o', '/src/Net/OutPacket.cpp.o'))]
    # Only replace the transport boundary; retain production packet serialization.
    transport = (root / 'src/Net/OutPacket.cpp').read_text()
    start = transport.index('\tvoid OutPacket::dispatch()')
    end = transport.index('\n\tvoid OutPacket::skip', start)
    transport = transport[:start] + 'std::vector<std::vector<int8_t>> captured;\nvoid OutPacket::dispatch() { captured.push_back(bytes); }\n' + transport[end:]
    transport = transport.replace('#include "', '#include "' + str(root / 'src/Net') + '/')
    transport_file = work / 'Transport.cpp'
    transport_file.write_text(transport)
    link += [str(transport_file)]
    link[link.index('-o') + 1] = str(executable)
    link += ['-std=c++17', '-UNDEBUG', '-I' + str(root), *shlex.split(includes),
             str(cpp), '-Wl,-rpath,' + str(root / 'vendor/bass24/macos')]
    subprocess.run(link, cwd=build, check=True)
    output = build / 'inventory-large-update-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
