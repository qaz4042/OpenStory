#!/usr/bin/env python3
"""macOS integration check: character name format errors and retry.

Uses production CMake objects and NX assets, opens a temporary render window,
and keeps all Settings writes in a temporary directory. No server connection.
Screenshots are saved under build-macos/character-naming-check/.
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
#include "src/IO/UITypes/UIExplorerCreation.h"
#include "src/IO/UITypes/UINotice.h"
#include "src/IO/UI.h"
#include "src/IO/Window.h"
#include "src/Configuration.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class Creation : public UIExplorerCreation {
public:
 void reject(const char* name) {
  namechar.change_text(name); namechar.set_state(Textfield::State::FOCUSED);
  naming_ok_pressed();
  assert(namechar.get_state()==Textfield::State::DISABLED);
 }
 void restored() { assert(namechar.get_state()==Textfield::State::FOCUSED); }
};
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<Fullscreen>::get().save(false);
 assert(!Window::get().init()); assert(!Sound::init()); assert(!Music::init());
 UI::get().change_state(UI::State::LOGIN);
 Creation page;
 for(const char* language : {"en", "zh-CN"}) {
  Setting<UILanguage>::get().save(language);
  for(const char* name : {"卡卡寂312", "ab", "abc def", "abc!"}) {
   page.reject(name);
   auto notice=UI::get().get_element<UIOk>(); assert(notice);
   Window::get().begin(); page.draw(1.0f); notice->draw(1.0f); Window::get().end();
   Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+language);
   Window::get().take_screenshot();
   notice->send_key(KeyAction::Id::RETURN,true,false);
   page.restored(); UI::get().remove(UIElement::Type::NOTICE);
  }
 }
 Sound::close(); std::cout<<"PASS: invalid names, bilingual notice, retry after dismissal\n";
}
'''

with tempfile.TemporaryDirectory(prefix='openstory-name-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'NameCheck.app'
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
    output = build / 'character-naming-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
