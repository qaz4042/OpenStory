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
#include "src/IO/UIDragElement.h"
#include "src/IO/Components/LocalizedArt.h"
#include "src/IO/Components/Slider.h"
#include "src/Character/Look/CharLook.h"
#include "src/IO/Window.h"
#include "src/IO/UI.h"
#include "src/Graphics/GraphicsGL.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#define private public
#include "src/IO/UITypes/UIItemInventory.h"
#include "src/IO/UITypes/UIShop.h"
#include "src/IO/UITypes/UINotice.h"
#include "src/Net/Packets/NpcInteractionPackets.h"
#undef private
using namespace ms;
class Bag : public UIItemInventory {
public:
 using UIItemInventory::UIItemInventory;
 using UIItemInventory::button_pressed;
 Point<int16_t> at(int16_t slot) { return position+get_slotpos(slot)+Point<int16_t>(8,8); }
 void place() { position={30,40}; }
 void assert_lock_inside() {
  auto r=buttons[BT_SELL_LOCK]->bounds({0,0});
  assert(r.left()>=0 && r.top()>=20 && r.right()<=dimension.x() && r.bottom()<=dimension.y());
 }
};
class Shop : public UIShop {
public:
 using UIShop::UIShop;
 using UIShop::button_pressed;
 void place() { position={230,20}; }
 void print_buttons() {
  for(auto& b:buttons) if(b.second->is_active()) {
   auto r=b.second->bounds({0,0});
   std::cout<<"button "<<b.first<<" "<<r.left()<<","<<r.top()<<" "<<r.right()<<","<<r.bottom()<<"\n";
  }
 }
};
void render(UIElement& page,const std::string& path) {
 GraphicsGL::get().clearscene(); Window::get().begin(); page.draw(1); GraphicsGL::get().flush(1);
 int w,h;glfwGetFramebufferSize(glfwGetCurrentContext(),&w,&h);
 std::vector<unsigned char> pixels(size_t(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
 std::ofstream out(path,std::ios::binary); out<<"P6\n"<<w<<" "<<h<<"\n255\n";
 for(int y=h-1;y>=0;--y)for(int x=0;x<w;++x)out.write(reinterpret_cast<char*>(pixels.data()+(y*w+x)*4),3);
 assert(glGetError()==GL_NO_ERROR);
}
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false);Setting<Width>::get().save(1024);Setting<Height>::get().save(768);
 assert(!Window::get().init());UI::get().init();UI::get().change_state(UI::State::GAME);
 std::cerr<<"initialized UI\n";
 Inventory inv; inv.set_slotmax(InventoryType::EQUIP,96);inv.set_slotmax(InventoryType::USE,96);inv.set_slotmax(InventoryType::ETC,96);inv.set_slotmax(InventoryType::CASH,96);
 inv.add_item(InventoryType::USE,1,2000000,false,-1,10,"",0x400);
 inv.add_item(InventoryType::USE,2,2000001,false,-1,20,"",0);
 inv.add_item(InventoryType::CASH,1,5450000,true,-1,1,"",0);
 EnumMap<EquipStat::Id,uint16_t> stats;
 inv.add_equip(InventoryType::EQUIP,1,1002008,false,-1,7,0,stats,"",0x400,0,0,0);
 assert(inv.is_sell_locked(InventoryType::EQUIP,1));assert(inv.is_sell_locked(InventoryType::USE,1));assert(!inv.is_sell_locked(InventoryType::USE,2));
 inv.modify(InventoryType::USE,1,Inventory::SWAP,3,Inventory::MOVE_INTERNAL);
 assert(!inv.is_sell_locked(InventoryType::USE,1));assert(inv.is_sell_locked(InventoryType::USE,3));
 // Apply the same remove/add flag update used by the server, without optimistic changes.
 inv.modify(InventoryType::USE,3,Inventory::REMOVE,0,Inventory::MOVE_NONE);
 inv.add_item(InventoryType::USE,3,2000000,false,-1,10,"",0);
 assert(!inv.is_sell_locked(InventoryType::USE,3));
 inv.modify(InventoryType::USE,3,Inventory::REMOVE,0,Inventory::MOVE_NONE);
 inv.add_item(InventoryType::USE,3,2000000,false,-1,10,"",0x400);
 std::cerr<<"inventory checks passed\n";
 Bag bag(inv);bag.place();bag.change_tab(InventoryType::USE);
 bag.button_pressed(Bag::BT_SELL_LOCK);assert(bag.sell_lock_mode);
 bag.send_cursor(true,bag.at(3));assert(bag.lock_press_handled);assert(inv.is_sell_locked(InventoryType::USE,3));
 bag.send_cursor(true,bag.at(3));bag.send_cursor(false,bag.at(3));assert(!bag.lock_press_handled);
 std::cerr<<"bag checks passed\n";
 CharLook::init();CharLook look;look.set_body(0);look.set_hair(30000);look.set_face(20000);Shop shop(look,inv);shop.reset(9090000);shop.place();shop.add_item(2000000,50,0,0,100);shop.finalize_buy_tabs();shop.changeselltab(InventoryType::USE);
 std::cerr<<"shop prepared\n";shop.print_buttons();
 for(const char* lang:{"zh-CN","en"}) {
  Setting<UILanguage>::get().save(lang);
  std::cerr<<"render bag "<<lang<<"\n";bag.set_full(false);bag.assert_lock_inside();render(bag,std::string(argv[3])+"/bag-"+lang+".ppm");
  bag.set_full(true);bag.assert_lock_inside();render(bag,std::string(argv[3])+"/bag-full-"+lang+".ppm");
  std::cerr<<"render shop\n";render(shop,std::string(argv[3])+"/shop-"+lang+".ppm");
  std::cerr<<"open modal\n";shop.button_pressed(Shop::SELL_ALL);auto modal=UI::get().get_element<UIYesNo>();assert(modal);
  render(*modal,std::string(argv[3])+"/confirm-"+lang+".ppm");
  modal->yesnohandler(false);UI::get().remove(UIElement::Type::NOTICE);
 }
 ShopSellAllPacket bulk(9090000,{{2,2,2000001,20}});assert(bulk.bytes.size()==18);assert(bulk.bytes[2]==4);
 SetSellLockPacket lock(2,3,2000000,true);assert(lock.bytes.size()==11);assert(lock.bytes[2]==5);
 std::cout<<"PASS: server flag updates, equip/stack locks, swap retention, lock click mode, bulk confirmation and EN/CN rendering\n";
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
    output = build / 'shop-bulk-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
