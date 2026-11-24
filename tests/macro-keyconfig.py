#!/usr/bin/env python3
"""macOS regression: visible macro bindings after drag/drop, move and save.

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
#include "src/IO/Components/Charset.h"
#include "src/IO/Components/Slider.h"
#include "src/IO/Components/StatefulIcon.h"
#include "src/IO/Components/Textfield.h"
#include "src/Character/CharStats.h"
#include "src/Character/SkillBook.h"
#include "src/IO/Keyboard.h"
#define private public
#include "src/IO/UITypes/UIKeyConfig.h"
#include "src/IO/UITypes/UISkillBook.h"
#undef private
#include "src/IO/Window.h"
#include "src/IO/UI.h"
#include "src/Configuration.h"
#include "src/Util/NxFiles.h"
#include "src/Graphics/GraphicsGL.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024);Setting<Height>::get().save(768);Setting<Fullscreen>::get().save(false);
 assert(!Window::get().init());UI::get().change_state(UI::State::GAME);
 Inventory inventory;SkillBook skills;
 UI::get().emplace<UIKeyConfig>(inventory,skills);
 auto config=UI::get().get_element<UIKeyConfig>();assert(config);
 auto at=[&](KeyConfig::Key key) {
  // Keyboard settings are centered in the 1024x768 viewport.
  return Point<int16_t>((1024-622)/2,(768-374)/2)+config->keys_pos[key]+Point<int16_t>(16,16);
 };
 const KeyConfig::Key keys[]={KeyConfig::A,KeyConfig::S,KeyConfig::D};
 for(int i=0;i<3;++i) {
  Icon icon(std::make_unique<UISkillBook::MacroIcon>(i),Texture(),-1);
  assert(config->send_icon(icon,at(keys[i])));
  assert(config->get_staged_mapping(keys[i])==Keyboard::Mapping(KeyType::MACRO,i));
  assert(config->macro_icons.count(i) && config->macro_icons.at(i)->get_texture().is_valid());
 }
 // Move an already bound macro; no invisible binding or duplicate key remains.
 auto icon=config->macro_icons.at(0).get();assert(config->send_icon(*icon,at(KeyConfig::F1)));
 assert(config->macro_icons.at(0).get()==icon);
 assert(config->get_staged_mapping(KeyConfig::F1)==Keyboard::Mapping(KeyType::MACRO,0));
 assert(config->get_staged_mapping(KeyConfig::A).type==KeyType::NONE);
 config->save_staged_mappings();
 assert(UI::get().get_keyboard().get_maple_mapping(KeyConfig::F1)==Keyboard::Mapping(KeyType::MACRO,0));
 for(const char* lang:{"en","zh-CN"}) {
  Setting<UILanguage>::get().save(lang);
  UIKeyConfig reopened(inventory,skills);
  assert(reopened.get_staged_mapping(KeyConfig::F1)==Keyboard::Mapping(KeyType::MACRO,0));
  for(int i=0;i<3;++i) assert(reopened.macro_icons.at(i)->get_texture().is_valid());
  GraphicsGL::get().clearscene();Window::get().begin();reopened.draw(1.0f);GraphicsGL::get().flush(1.0f);
  assert(glGetError()==GL_NO_ERROR);
 }
 std::cout<<"PASS: three macro drops create visible key icons; moving and saving/reopening preserve bindings in EN/CN\n";
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
