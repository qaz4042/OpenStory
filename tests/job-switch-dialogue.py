#!/usr/bin/env python3
"""Render Chinese job-switch menu and confirmation through production NPC packet/UI handling."""
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
#include "src/IO/KeyConfig.h"
#include "src/IO/UITypes/UINpcTalk.h"
#include "src/Net/Handlers/NpcInteractionHandlers.h"
#include "src/Net/NpcResponseTracker.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
namespace ms { extern std::vector<std::vector<int8_t>> captured; }
class Talk : public UINpcTalk {
public: using UINpcTalk::button_pressed;
 bool active_button(int id) { return buttons.at(id)->is_active(); }
};
std::vector<int8_t> wire;
void put(uint64_t n,int bytes) { for(int i=0;i<bytes;++i)wire.push_back((n>>(i*8))&255); }
void receive(int type,int prev=0,int next=0) {
 wire.clear();put(4,1);put(12101,4);put(type,1);put(0,1);
 std::string text="Amherst dialogue";put(text.size(),2);for(char c:text)put(c,1);
 if(type==0) {put(prev,1);put(next,1);}
 InPacket packet(wire.data(),wire.size());NpcDialogueHandler handler;
 static_cast<const PacketHandler&>(handler).handle(packet);assert(packet.length()==0);
 assert(UI::get().get_element<UINpcTalk>()->is_active());
}
void reply(int type,int action) {
 assert(captured.size()==1);assert(uint8_t(captured[0][0])==60);
 assert(captured[0][2]==type && captured[0][3]==action);captured.clear();
}
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false);assert(!Window::get().init());UI::get().change_state(UI::State::GAME);
 // All four Cosmic text variants retain msgtype 0 in the reply.
 for(int style=0;style<4;++style) {
  Talk talk;talk.change_text(12101,0,style,0,"Text");
  assert(talk.active_button(3)==(style==0)); // OK
  assert(talk.active_button(1)==(style==2 || style==3)); // Next
  assert(talk.active_button(4)==(style==1 || style==3)); // Previous
  talk.button_pressed(style==0 ? 3 : style==1 ? 4 : 1);reply(0,style==1 ? 0 : 1);
 }
 // Use the physical Y path through UI, independent of the gameplay keymap.
 UI::get().get_keyboard().assign(KeyConfig::Y,KeyType::SKILL,2001002);
 for(int round=0;round<5;++round) {
  NpcResponseTracker::get().mark_pending(12101);receive(0,0,1);
  UI::get().send_key(GLFW_KEY_Y,true);reply(0,1);
  // A new page arrives before key-up: key repeat must not skip it.
  receive(0,1,1);UI::get().send_key(GLFW_KEY_Y,true);assert(captured.empty());
  UI::get().send_key(GLFW_KEY_Y,false);UI::get().send_key(GLFW_KEY_Y,true);reply(0,1);
  UI::get().send_key(GLFW_KEY_Y,false);
  receive(0);UI::get().send_key(GLFW_KEY_Y,true);reply(0,1);UI::get().send_key(GLFW_KEY_Y,false);
  assert(!UI::get().get_element<UINpcTalk>()->is_active());
 }
 receive(0,1,1);UI::get().send_key(GLFW_KEY_ESCAPE,true);reply(0,-1);UI::get().send_key(GLFW_KEY_ESCAPE,false);
 receive(1);UI::get().send_key(GLFW_KEY_Y,true);reply(1,1);UI::get().send_key(GLFW_KEY_Y,false);
 receive(12);UI::get().send_key(GLFW_KEY_Y,true);reply(12,1);UI::get().send_key(GLFW_KEY_Y,false);
 // Y in a text field remains printable instead of submitting the dialog.
 receive(2);UI::get().send_key(GLFW_KEY_Y,true);UI::get().send_char('y');UI::get().send_key(GLFW_KEY_Y,false);
 assert(captured.empty() && UI::get().get_element<UINpcTalk>()->is_active());
 // Render the shortened Chinese dialogue using the production parser and UI.
 Setting<UILanguage>::get().save("zh-CN");
 for (int page=0;page<2;++page) {
  std::string chinese = page==0
   ? "#e职业切换#n\r\n30–69级同系二转，脱战30秒后可用。\r\n确认时自动卸下全部装备，请留足装备栏空位。\r\n#b请选择目标职业：#k\r\n#L0#剑客#l\r\n#L2#枪战士#l\r\n"
   : "确认切换为 #b枪战士#k？\r\n保留当前全部属性加点。\r\n#r全部装备（含现金装备）将自动移入装备栏。#k\r\n二转技能重置并退还SP，一转和基础血蓝保留。\r\n不发转职奖励；请重新加点并检查快捷键。\r\n是否继续？";
  wire.clear();put(4,1);put(1022000,4);put(page==0 ? 4 : 1,1);put(0,1);
  put(chinese.size(),2);for(char c:chinese)put(c,1);
  InPacket packet(wire.data(),wire.size());NpcDialogueHandler handler;
  static_cast<const PacketHandler&>(handler).handle(packet);assert(packet.length()==0);
  for(int tick=0;tick<4000;++tick) UI::get().get_element<UINpcTalk>()->update();
  Window::get().begin();UI::get().draw(1);Window::get().end();
  Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+std::to_string(page));
  Window::get().take_screenshot();
 }
 std::cout<<"PASS: text button flags/wire replies, five reopen cycles, Y yes/next/OK, repeat suppression, Escape cancel and text input\n";
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
    link = [arg for arg in link if not arg.endswith(('/src/MapleStory.cpp.o', '/src/Net/OutPacket.cpp.o'))]
    # Only replace the transport boundary; retain production packet serialization.
    transport = (root / 'src/Net/OutPacket.cpp').read_text()
    start = transport.index('\tvoid OutPacket::dispatch()')
    end = transport.index('\n\tvoid OutPacket::skip', start)
    transport = transport[:start] + 'std::vector<std::vector<int8_t>> captured;\nvoid OutPacket::dispatch() { captured.push_back(bytes); }\n' + transport[end:]
    transport = transport.replace('#include "', '#include "' + str(root / 'src/Net') + '/')
    transport_file = work / 'Transport.cpp'
    transport_file.write_text(transport)
    link += [str(transport_file)]
    link[link.index('-o') + 1] = str(executable)
    link += ['-std=c++17', '-UNDEBUG', '-I' + str(root), *shlex.split(includes),
             str(cpp), '-Wl,-rpath,' + str(root / 'vendor/bass24/macos')]
    subprocess.run(link, cwd=build, check=True)
    output = build / 'job-switch-dialogue-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
