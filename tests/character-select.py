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
#include "src/IO/UITypes/UICharSelect.h"
#include "src/IO/Components/LoginResources.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
int main(int argc,char** argv){
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());
 assert(login_character_resources()==retained_login_resources()["CharSelect"]);
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024);Setting<Height>::get().save(768);Setting<Fullscreen>::get().save(false);Setting<DefaultCharacter>::get().save(0);Configuration::get().set_worldid(0);Configuration::get().set_channelid(0);
 assert(!Window::get().init());assert(!Sound::init());assert(!Music::init());
 CharEntry entry{};entry.stats.name="TestHero";entry.stats.stats[MapleStat::Id::LEVEL]=7;entry.stats.stats[MapleStat::Id::STR]=35;entry.stats.stats[MapleStat::Id::DEX]=10;entry.stats.stats[MapleStat::Id::INT]=4;entry.stats.stats[MapleStat::Id::LUK]=4;
 UICharSelect p({entry},1,3,0);for(int i=0;i<200;++i)p.update();
 Window::get().begin();p.draw(1.0f);Window::get().end();Setting<ScreenshotFolder>::get().save(argv[3]);Window::get().take_screenshot();
 Sound::close();std::cout<<"PASS: retained character resources and production rendering\n";
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
    output = build / 'character-select-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
