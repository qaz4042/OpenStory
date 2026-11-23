#!/usr/bin/env python3
"""Offline world/channel layout and cursor regression using production objects."""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
build = root / 'build-macos'
subprocess.run(['cmake', '--build', str(build), '-j', '8'], cwd=root, check=True,
               stdout=subprocess.DEVNULL)
source = r'''
#include "src/IO/UITypes/UIWorldSelect.h"
#include "src/IO/Components/LoginResources.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class Page : public UIWorldSelect { public: void select(){button_pressed(0);} void check(int count){for(int i=0;i<count;++i){auto b=buttons.at(5+i)->bounds(get_draw_position());assert(b.width()>0);auto p=b.get_left_top()+Point<int16_t>(b.width()/2,b.height()/2);if(i){send_cursor(false,p);send_cursor(true,p);send_cursor(false,p);}assert(buttons.at(5+i)->get_state()==Button::State::PRESSED);}}};
int main(int argc,char** argv){
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());
 assert(login_world_resources()==retained_login_resources()["WorldSelect"]);
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024);Setting<Height>::get().save(768);Setting<Fullscreen>::get().save(false);
 assert(!Window::get().init());assert(!Sound::init());assert(!Music::init());
 for(int count:{1,20}){Page p;p.add_world(World{"Scania","",std::vector<int32_t>(count,10),static_cast<uint8_t>(count),0,0});p.draw_world();p.select();for(int i=0;i<200;++i)p.update();p.check(count);Window::get().begin();p.draw(1.0f);Window::get().end();Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+std::to_string(count));Window::get().take_screenshot();}
 Sound::close();std::cout<<"PASS: v83 world resources and one/twenty channel cursor selection\n";
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
    output = build / 'world-select-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
