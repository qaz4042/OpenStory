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
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class DragTarget : public UIElement {
public:
 static constexpr Type TYPE=Type::ITEMINVENTORY;
 static constexpr bool TOGGLED=false, FOCUSED=false;
 int pickups=0, drops=0;
 Icon icon;
 DragTarget() { position={100,100}; dimension={100,100}; active=true; }
 Type get_type() const override { return TYPE; }
 Cursor::State send_cursor(bool down,Point<int16_t>) override {
  if(down) { ++pickups; icon.start_drag({0,0}); UI::get().drag_icon(&icon); return Cursor::GRABBING; }
  return Cursor::IDLE;
 }
 bool send_icon(const Icon&,Point<int16_t>) override { ++drops; return true; }
};
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 assert(!Window::get().init());
 UI::get().init(); UI::get().change_state(UI::State::GAME);
 auto target=UI::get().emplace<DragTarget>(); assert(target);
 for(auto dest : {Point<int16_t>(160,120),Point<int16_t>(120,120)}) {
  const int before=target->pickups;
  UI::get().send_cursor(Point<int16_t>(120,120));
  UI::get().send_cursor(true); assert(target->pickups==before+1);
  UI::get().send_cursor(false);
  UI::get().send_cursor(dest);
  UI::get().send_cursor(true);
  // Real cursor callbacks arrive before mouse-up as IDLE while held.
  UI::get().send_cursor(dest+Point<int16_t>(1,1));
  assert(target->pickups==before+1);
  UI::get().send_cursor(false);
 }
 assert(target->drops==2);
 std::cout<<"PASS: swap and same-slot drop do not repick on held cursor movement\n";
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
    output = build / 'item-drag-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
