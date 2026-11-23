#!/usr/bin/env python3
"""macOS integration check: cash shop catalog, locker packets and TOS wheel routing.

Uses production CMake objects and NX assets, opens a temporary render window,
and keeps all Settings writes in a temporary directory. No server connection.
Screenshots are saved under build-macos/cashshop-scroll-check/.
"""
from pathlib import Path
import shlex
import subprocess
import tempfile
import sys

root = Path(__file__).resolve().parents[1]
build = root / 'build-macos'
subprocess.run(['cmake', '--build', str(build), '-j', '8'], cwd=root, check=True,
               stdout=subprocess.DEVNULL)
source = r'''
#include "src/IO/UITypes/UICashShop.h"
#include "src/IO/UITypes/UITermsOfService.h"
#include "src/IO/UI.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include "src/Net/Handlers/CashShopHandlers.h"
#include "src/Gameplay/Stage.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
std::vector<int8_t> bytes;
void number(uint64_t n,int count) { for(int i=0;i<count;++i) bytes.push_back((n>>(8*i))&255); }
void record(int64_t id,int item,int count=1) {
 number(id,8); number(12,4); number(0,4); number(item,4); number(10000001,4); number(count,2);
 for(int i=0;i<29;++i) bytes.push_back(0);
}
void packet() { InPacket p(bytes.data(),bytes.size()); CashShopOperationHandler().handle(p); assert(p.length()==0); }
void render(UIElement& page,const std::string& folder) {
 Window::get().begin();page.draw(1);Window::get().end();
 Setting<ScreenshotFolder>::get().save(folder); Window::get().take_screenshot();
}
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<Fullscreen>::get().save(false); Setting<UILanguage>::get().save("zh-CN");
 assert(!Window::get().init()); assert(!Sound::init()); assert(!Music::init());
 // Real Cosmic cash locker records: snapshot, ordinary purchase and package.
 bytes={0x4B}; number(1,2);record(101,1002000);number(4,4);packet();
 assert(get_cash_inventory().size()==1 && get_cash_inventory()[0].item_id==1002000);
 bytes={0x57};record(102,2000000,20);packet(); assert(get_cash_inventory().size()==2);
 bytes={0x57};record(102,2000000,20);packet(); assert(get_cash_inventory().size()==2);
 bytes={int8_t(0x89)};number(2,1);record(103,5000000);record(104,1002001);number(0,2);packet();
 assert(get_cash_inventory().size()==4);
 // Transfer success carries a destination slot and full item record.
 bytes={0x68};number(9,2);number(2,1);number(2000000,4);number(1,1);number(102,8);
 number(0,8);number(20,2);number(0,2);number(0,2);packet();
 assert(Stage::get().get_player().get_inventory().get_item_id(InventoryType::USE,9)==2000000);
 assert(get_cash_inventory().size()==3);
 bytes={0x57};record(102,2000000,20);packet();
 bytes={0x4B};number(1,2);record(999,1002000); // missing trailing slot counts
 try {packet();assert(false);} catch(const PacketError&) {}
 assert(get_cash_inventory().size()==4);
 UI::get().change_state(UI::State::CASHSHOP);
 auto shop=UI::get().get_element<UICashShop>();assert(shop);
 CharLook::init();shop->preview_look.set_body(0);shop->preview_look.set_hair(30000);shop->preview_look.set_face(20000);
 assert(shop->items.size()>15);
 UI::get().send_cursor(Point<int16_t>(400,320));
 UI::get().send_scroll(-1);assert(shop->list_offset==5);
 UI::get().send_scroll(1);assert(shop->list_offset==0);
 UI::get().send_scroll(1);assert(shop->list_offset==0);
 for(int i=0;i<500;++i) UI::get().send_scroll(-1);
 assert(shop->list_offset==std::max<int>(0, ((int(shop->items.size())+4)/5-3)*5));
 for(int i=0;i<500;++i) UI::get().send_scroll(1);
 assert(shop->list_offset==0);
 render(*shop,std::string(argv[3])+"/catalog");
 // Search through actual OS character routing, supports UTF-8 names and IDs.
 const int id=shop->items.front().get_itemid();
 shop->send_cursor(true,Point<int16_t>(500,12));
 assert(shop->search.get_state()==Textfield::State::FOCUSED);
 for(char c:std::to_string(id)) UI::get().send_char(c);
 shop->update(); assert(shop->search.get_text()==std::to_string(id));
 assert(!shop->items.empty());
 for(const auto& item:shop->items) assert(std::to_string(item.get_itemid()).find(std::to_string(id))!=std::string::npos);
 shop->search.change_text("__no_such_cash_item__");shop->filter_catalog();assert(shop->items.empty());
 assert(shop->list_offset==0 && !shop->buttons.at(UICashShop::Buttons::BtBuy)->is_active());
 shop->search.change_text("");shop->filter_catalog();
 shop->send_cursor(true,Point<int16_t>(40,58+33+10));assert(shop->category==1 && !shop->items.empty());
 for(const auto& item:shop->items) assert(item.get_itemid()/10000==100);
 auto title=shop->items.front().get_name();
 title.erase(std::remove(title.begin(),title.end(),'\0'),title.end());
 shop->search.change_text("");shop->search.set_state(Textfield::State::FOCUSED);
 for(size_t i=0;i<title.size();) {
  size_t used=0;auto cp=Text::utf8_decode(title.data(),title.size(),i,used);assert(used>0);
  UI::get().send_char(cp);i+=used;
 }
 shop->update();assert(!shop->items.empty() && shop->search.get_text()==title);
 render(*shop,std::string(argv[3])+"/search");
 for(int i=0;i<20;++i) {bytes={0x57};record(200+i,1002000+i);packet();}
 shop->send_cursor(false,Point<int16_t>(820,540));UI::get().send_scroll(-1);assert(shop->cash_offset==7);
 render(*shop,std::string(argv[3])+"/purchased");
 bytes={0x6C};number(200,8);packet();assert(get_cash_inventory().size()==23);
 bytes={0x4B};number(0,2);number(4,4);packet();assert(get_cash_inventory().empty());
 UI::get().remove_textfield();
 UI::get().change_state(UI::State::LOGIN);
 auto terms=UI::get().emplace<UITermsOfService>([](){});assert(terms);
 assert(terms->buttons.at(0)->get_state()==Button::State::DISABLED);
 UI::get().send_scroll(-1);assert(terms->offset==1);
 UI::get().send_scroll(1);assert(terms->offset==0);
 render(*terms,std::string(argv[3])+"/terms-top");
 for(int i=0;i<terms->max_rows+3;++i) UI::get().send_scroll(-1);
 assert(terms->offset==terms->max_rows-1);
 assert(terms->buttons.at(0)->get_state()==Button::State::NORMAL);
 render(*terms,std::string(argv[3])+"/terms-bottom");
 UI::get().send_scroll(1);assert(terms->buttons.at(0)->get_state()==Button::State::DISABLED);
 Sound::close(); std::cout<<"PASS: cash snapshots/purchase/package, search, categories, catalog and locker wheel, login TOS routing and acceptance\n";
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-cash-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'CashCheck.app'
    (app / 'Contents/MacOS').mkdir(parents=True)
    (app / 'Contents/Resources').mkdir()
    executable = app / 'Contents/MacOS/check'
    flags = (build / 'CMakeFiles/OpenStory.dir/flags.make').read_text()
    includes = next(line.split(' = ', 1)[1] for line in flags.splitlines()
                    if line.startswith('CXX_INCLUDES = '))
    link = shlex.split((build / 'CMakeFiles/OpenStory.dir/link.txt').read_text())
    link = [arg for arg in link if not arg.endswith('/src/MapleStory.cpp.o')]
    link[link.index('-o') + 1] = str(executable)
    link += ['-std=c++17', '-UNDEBUG', '-fno-access-control', '-I' + str(root), *shlex.split(includes),
             str(cpp), '-Wl,-rpath,' + str(root / 'vendor/bass24/macos')]
    subprocess.run(link, cwd=build, check=True)
    output = build / 'cashshop-scroll-check'
    output.mkdir(exist_ok=True)
    if "--build-only" not in sys.argv:
        subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                       cwd=work, check=True)
