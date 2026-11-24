#!/usr/bin/env python3
"""macOS regression: world-map navigation, frame stability and travel debounce.

Uses production CMake objects and NX assets, opens a temporary render window,
and keeps all Settings writes in a temporary directory. No server connection.
Exercises production UI objects without connecting to a server.
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
#include "src/IO/UIDragElement.h"
#include "src/IO/Components/LocalizedArt.h"
#include "src/IO/Components/AreaButton.h"
#include "src/IO/Components/Textfield.h"
#include <chrono>
#define private public
#include "src/IO/UITypes/UIWorldMap.h"
#undef private
#include "src/IO/Window.h"
#include "src/IO/UI.h"
#include "src/IO/UITypes/UINotice.h"
#include "src/Configuration.h"
#include "src/Util/NxFiles.h"
#include "src/Audio/Audio.h"
#include <nlnx/nx.hpp>
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class TestWorld : public UIWorldMap {
public:
 void place() { position={80,80}; }
 void click_link(int index) {
  auto& button=buttons.at(6+index); assert(button->is_active());
  auto b=button->bounds(position); auto p=b.get_left_top()+Point<int16_t>(b.width()/2,b.height()/2);
  send_cursor(false,p); send_cursor(true,p);
  const auto entered_parent = parent_map;
  assert(region_click_consumed);
  // UIStateGame routes cursor moves during a held press as clicked=true.
  // Put the cursor on a child link to prove it cannot drill down again.
  Point<int16_t> child_point = p;
  for (const auto& link : link_maps) {
   if (link.second == "WorldMap") continue;
   auto child=buttons.at(link.first)->bounds(position);
   child_point=child.get_left_top()+Point<int16_t>(child.width()/2,child.height()/2);
   break;
  }
  for(int move=0;move<5;++move) send_cursor(true,child_point);
  doubleclick(child_point); // GLFW can dispatch this before the release.
  assert(parent_map==entered_parent && region_click_consumed);
  assert(pending_confirmation==-1 && pending_destinations.empty());
  send_cursor(false,child_point);
  assert(!region_click_consumed && parent_map==entered_parent);
  // Releasing outside the window clears the latch via remove_cursor too.
  region_click_consumed=true; remove_cursor(); assert(!region_click_consumed);
 }
};
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<Fullscreen>::get().save(false); assert(!Window::get().init()); assert(!Sound::init());
 UI::get().change_state(UI::State::LOGIN);
 for (const char* lang : {"en","zh-CN"}) {
  Setting<UILanguage>::get().save(lang);
  TestWorld world; world.update(); world.place();
  // Map 0 -> WorldMap00 -> WorldMap; first Esc closes search.
  world.send_key(0,true,true); world.update_world("WorldMap");
  for(int repeat=0;repeat<30;++repeat) {
   assert(world.parent_map.empty());
   int index = -1;
   for (const auto& link : world.link_maps) {
    if (link.second == "WorldMap010") index = link.first - 6;
   }
   assert(index >= 0);
   world.click_link(index); // overview Victoria link; callback replaces the clicked button
   assert(!world.parent_map.empty());
   world.update();
   world.send_key(0,true,true);
  }
  std::vector<uint8_t> previous;
  for (int i=0;i<4;++i) {
   if(i==2) world.set_search(true);
   for(int warm=0;warm<3;++warm) { Window::get().begin(); world.draw((i%2) ? .95f : .05f); Window::get().end(); }
   Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+lang+"-"+std::to_string(i));
   Window::get().take_screenshot();
   std::vector<uint8_t> pixels(1024*768*4);
   glReadBuffer(GL_FRONT); glReadPixels(0,0,1024,768,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
   if(i%2) assert(pixels == previous); // interpolation must not alter static UI opacity
   previous=std::move(pixels);
  }
  world.update_world("WorldMap010");
  bool tested=false;
  for (const auto& spot : world.map_spots) {
   if(spot.second.map_ids.size()!=1 || spot.second.map_ids.front()==world.mapid) continue;
   auto at=spot.first+Point<int16_t>(80,80)+world.base_position-10+Point<int16_t>(1,1);
   world.doubleclick(at);
   auto next=world.next_travel;
   assert(next==std::chrono::steady_clock::time_point{});
   assert(world.pending_confirmation==spot.second.map_ids.front());
   world.update();
   auto notice=UI::get().get_element<UIYesNo>(); assert(notice && notice->is_active());
   notice->send_key(0,true,true); // cancel does not send or start cooldown
   assert(next==world.next_travel);
   world.doubleclick(at); world.update();
   notice=UI::get().get_element<UIYesNo>(); assert(notice && notice->is_active());
   notice->send_key(KeyAction::Id::RETURN,true,false);
   next=world.next_travel; assert(next>std::chrono::steady_clock::now());
   world.doubleclick(at); assert(world.pending_confirmation==-1);
   assert(next==world.next_travel);
   tested=true; break;
  }
  assert(tested);
 }
 std::cout<<"PASS: held region clicks consume repeats and double-clicks until release; repeated overview -> Victoria -> overview, both languages\n";
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
