#!/usr/bin/env python3
"""macOS regression: single-point clicks and acknowledged fill-to-cap.

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
#include <chrono>
#define private public
#include "src/IO/UITypes/UISkillBook.h"
#undef private
#include "src/IO/Window.h"
#include "src/IO/UI.h"
#include "src/Configuration.h"
#include "src/Util/NxFiles.h"
#include "src/Graphics/GraphicsGL.h"
#include "src/Data/SkillData.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class Book : public UISkillBook {
public:
 using UISkillBook::UISkillBook;
 using UISkillBook::button_pressed;
 Button& button(int id) { return *buttons.at(id); }
 Point<int16_t> at(int id) { return button(id).bounds(get_draw_position()).get_left_top()+Point<int16_t>(4,5); }
};
int main(int argc,char** argv) {
 std::filesystem::current_path(argv[1]);assert(!NxFiles::init());std::filesystem::current_path(argv[2]);
 Setting<Width>::get().save(1024);Setting<Height>::get().save(768);Setting<Fullscreen>::get().save(false);
 assert(!Window::get().init());UI::get().init();
 CharStats stats;stats.set_stat(MapleStat::LEVEL,40);stats.change_job(510);stats.set_stat(MapleStat::SP,8);
 SkillBook skills;Book book(stats,skills);if(!book.is_active())book.toggle_active();book.change_tab(1);
 const int sid=5001001;
 auto row=[&] { for(size_t i=0;i<book.skills.size();++i) if(book.skills[i].get_id()==sid) return int(i); assert(false);return 0; };
 auto show=[&] { book.change_offset(row()); };
 auto ack=[&] {int level=skills.get_level(sid);skills.set_skill(sid,level+1,20,-1);book.update_skills(sid);};
 show();auto plus=book.at(Book::BT_SPUP0);auto max=book.at(Book::BT_SPMAX0);
 assert(book.button(Book::BT_SPMAX0).is_active()); // Level zero is shown.
 // Mouse movement while held, including after a server acknowledgement, must not re-spend.
 book.send_cursor(false,plus);book.send_cursor(true,plus);
 assert(book.pending_sp_skill==sid && book.pending_sp_waiting && book.pending_sp_left==0);
 for(int i=0;i<8;++i)book.send_cursor(true,plus+Point<int16_t>(i%2,0));
 ack();assert(book.pending_sp_skill==0);
 for(int i=0;i<8;++i)book.send_cursor(true,plus);
 assert(book.pending_sp_skill==0 && skills.get_level(sid)==1);
 book.send_cursor(false,plus);book.send_cursor(true,plus);assert(book.pending_sp_skill==sid);ack();
 book.send_cursor(false,plus);
 // Eight available points: one outstanding request at a time, no reply -> no progress.
 skills.set_skill(sid,0,20,-1);book.update_skills(sid);show();
 book.send_cursor(true,max);assert(book.pending_sp_left==7 && book.pending_sp_waiting);
 for(int i=0;i<8;++i)book.advance_spup();assert(book.pending_sp_left==7);
 for(int i=0;i<8;++i){assert(book.pending_sp_waiting);ack();book.advance_spup();}
 assert(skills.get_level(sid)==8 && book.pending_sp_skill==0);
 book.send_cursor(false,max);
 // Current cap limits fill even with excess SP.
 stats.set_stat(MapleStat::SP,100);skills.set_skill(sid,18,20,-1);book.update_skills(sid);show();
 book.send_cursor(true,max);assert(book.pending_sp_left==1);ack();book.advance_spup();ack();book.advance_spup();
 assert(skills.get_level(sid)==20 && !book.button(Book::BT_SPMAX0).is_active());
 // Zero SP and already maxed skills cannot submit.
 book.send_cursor(false,max);stats.set_stat(MapleStat::SP,0);skills.set_skill(sid,0,20,-1);book.update_skills(sid);show();
 book.send_spup(row(),true);assert(!book.pending_sp_skill); // Disabled buttons are visible but not active.
 assert(book.button(Book::BT_SPMAX0).get_state()==Button::DISABLED);
 stats.set_stat(MapleStat::SP,8);book.change_sp();book.send_spup(row(),true);
 book.pending_sp_sent-=std::chrono::seconds(4);book.advance_spup();assert(!book.pending_sp_skill);
 // Closing or changing tabs cancels remaining requests.
 book.send_spup(row(),true);book.close();book.advance_spup();assert(!book.pending_sp_skill);if(!book.is_active())book.toggle_active();
 book.change_sp();book.send_spup(row(),true);book.change_tab(2);assert(!book.pending_sp_skill);
 book.change_tab(1);show();
 // An unmet prerequisite blocks both single-point and max allocation.
 CharStats warrior;warrior.set_stat(MapleStat::LEVEL,20);warrior.change_job(100);warrior.set_stat(MapleStat::SP,8);
 SkillBook warrior_skills;Book locked(warrior,warrior_skills);locked.change_tab(1);
 bool checked=false;
 for(size_t i=0;i<locked.skills.size();++i) {
  int id=locked.skills[i].get_id();
  if(!locked.check_required(id)) {
   locked.send_spup(i);assert(!locked.pending_sp_skill);
   locked.send_spup(i,true);assert(!locked.pending_sp_skill);checked=true;break;
  }
 }
 assert(checked);
 for(auto lang:{"zh-CN","en"}) {
  Setting<UILanguage>::get().save(lang);
  GraphicsGL::get().clearscene();Window::get().begin();book.draw(1.0f);GraphicsGL::get().flush(1.0f);
  assert(glGetError()==GL_NO_ERROR);
  Setting<ScreenshotFolder>::get().save(argv[3]);Window::get().end();
  Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+lang);Window::get().take_screenshot();
 }
 std::cout<<"PASS: one click/hold produces one point; ACK-gated fill respects SP/cap; zero-SP, timeout, close/tab cancellation; EN/CN rendering\n";
}

'''

with tempfile.TemporaryDirectory(prefix='openstory-skill-points-') as directory:
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
    output = build / 'skill-point-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
