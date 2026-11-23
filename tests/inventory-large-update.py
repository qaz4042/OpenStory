#!/usr/bin/env python3
"""Exercise production cursor routing for click-to-pick / click-to-drop."""
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
#include "src/IO/UI.h"
#include "src/Gameplay/Stage.h"
#include "src/Net/Handlers/InventoryHandlers.h"
#include "src/Net/Packets/NpcInteractionPackets.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
std::vector<int8_t> bytes;
void put(uint64_t value,int width) { for(int i=0;i<width;++i) bytes.push_back((value>>(8*i))&255); }
void apply() {
 InPacket packet(bytes.data(),bytes.size());ModifyInventoryHandler handler;
 static_cast<const PacketHandler&>(handler).handle(packet);
 assert(packet.length()==0);
}
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false);Setting<Width>::get().save(1024);Setting<Height>::get().save(768);
 assert(!Window::get().init());UI::get().init();UI::get().change_state(UI::State::GAME);
 auto& inv=Stage::get().get_player().get_inventory();inv.set_slotmax(InventoryType::USE,96);
 for(int slot=1;slot<=96;++slot) inv.add_item(InventoryType::USE,slot,2000000,false,-1,100,"",0);
 // Full 96-slot sort: 96 removals followed by 96 additions, exactly as Cosmic emits.
 bytes.clear();put(1,1);put(192,1);
 for(int slot=1;slot<=96;++slot) { put(3,1);put(2,1);put(slot,2); }
 for(int slot=1;slot<=96;++slot) {
  put(0,1);put(2,1);put(slot,2);put(2,1);put(2000002+(slot%2),4);
  put(0,1);put(0,8);put(slot,2);put(0,2);put(slot==96 ? 0x400 : 0,2);
 }
 put(0xff,1);apply();
 std::vector<ShopSale> snapshot;
 for(int slot=1;slot<=96;++slot) {
  assert(inv.get_item_id(InventoryType::USE,slot)==2000002+(slot%2));
  assert(inv.get_item_count(InventoryType::USE,slot)==slot);
  assert(inv.is_sell_locked(InventoryType::USE,slot)==(slot==96));
  if(!inv.is_sell_locked(InventoryType::USE,slot)) snapshot.push_back({2,int16_t(slot),inv.get_item_id(InventoryType::USE,slot),inv.get_item_count(InventoryType::USE,slot)});
 }
 assert(snapshot.size()==95);ShopSellAllPacket sale(9090000,snapshot);
 // Wire-count boundaries, including the maximum unsigned byte value.
 for(int count:{127,128,192,255}) {
  bytes.clear();put(1,1);put(count,1);
  for(int i=1;i<=count;++i) { put(1,1);put(2,1);put(1,2);put(i,2); }
  put(0xff,1);apply();assert(inv.get_item_count(InventoryType::USE,1)==count);
 }
 std::cout<<"PASS: full 96-slot sort restores IDs/counts/locks and sale snapshot; 127/128/192/255 inventory updates consumed\n";
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
    link = [arg for arg in link if not arg.endswith('/src/MapleStory.cpp.o')]
    link[link.index('-o') + 1] = str(executable)
    link += ['-std=c++17', '-UNDEBUG', '-I' + str(root), *shlex.split(includes),
             str(cpp), '-Wl,-rpath,' + str(root / 'vendor/bass24/macos')]
    subprocess.run(link, cwd=build, check=True)
    output = build / 'inventory-large-update-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
