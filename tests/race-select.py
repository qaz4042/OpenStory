#!/usr/bin/env python3
"""macOS integration check: v83 race selection with the mixed UI archives.

Uses production CMake objects and NX assets, opens a temporary render window,
and keeps all Settings writes in a temporary directory. No server connection.
Screenshots are saved under build-macos/race-select-check/.
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
#include "src/IO/UITypes/UIRaceSelect.h"
#include "src/IO/Components/LoginResources.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class Race : public UIRaceSelect {
public:
 void click(unsigned id) {
  auto b=buttons.at(id)->bounds(get_draw_position());
  assert(b.width()>0 && b.height()>0);
  auto at=b.get_left_top()+Point<int16_t>(b.width()/2,b.height()/2);
  send_cursor(false,at); send_cursor(true,at); send_cursor(false,at);
 }
};
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 auto race=login_race_resources();
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<Fullscreen>::get().save(false);
 Setting<ScreenshotFolder>::get().save(argv[3]);
 assert(!Window::get().init()); assert(!Sound::init()); assert(!Music::init());
 for(auto cls : {"normal","knight","aran"}) {
  assert(Texture(race[cls]["OnAnimation"]["0"]).is_valid());
  assert(Texture(race[cls]["text"]["0"]).is_valid());
 }
 assert(Texture(race["textGL"]["0"]).is_valid());
 Race page;
 for(unsigned i=0;i<3;++i) {
  page.click(2+i);
  Window::get().begin();page.draw(1.0f);Window::get().end();
  Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+std::to_string(i));
  Window::get().take_screenshot();
 }
 Sound::close();std::cout<<"PASS: three race panels, descriptions and cursor selection\n";
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-race-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'RaceCheck.app'
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
    output = build / 'race-select-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
