#!/usr/bin/env python3
"""Render the production skill-window entry and verify server status gating."""
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
build = root / 'build-macos'
subprocess.run(['cmake', '--build', str(build), '-j', '8'], cwd=root, check=True, stdout=subprocess.DEVNULL)
source = r'''
#include "src/IO/UITypes/UISkillBook.h"
#include "src/IO/Window.h"
#include "src/IO/UI.h"
#include "src/Character/CharStats.h"
#include "src/Character/SkillBook.h"
#include "src/Configuration.h"
#include "src/Gameplay/Stage.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class Book : public UISkillBook {
public:
 using UISkillBook::UISkillBook;
 void verify_layout() const {
   // BT_JOB_SWITCH is the final button; locate it by its 52px width.
   const Button* entry=nullptr;
   for(const auto& item:buttons) if(item.second->width()==52) entry=item.second.get();
   assert(entry);
   auto b=entry->bounds({});
   std::cout<<"Switch button bounds "<<b.left()<<","<<b.top()<<","<<b.right()<<","<<b.bottom()<<" window "<<dimension.x()<<","<<dimension.y()<<"\n";
   assert(b.left()>=0 && b.top()>=275 && b.right()<=dimension.x() && b.bottom()<=dimension.y());
   for(const auto& item:buttons) if(item.second.get()!=entry && item.second->is_active()) {
     auto other=item.second->bounds({});
     assert(b.right()<=other.left() || b.left()>=other.right() || b.bottom()<=other.top() || b.top()>=other.bottom());
   }
 }
 bool disabled() const {
   for(const auto& item:buttons) if(item.second->width()==52) return item.second->get_state()==Button::DISABLED;
   assert(false); return true;
 }
 void click_entry() {
   for(const auto& item:buttons) if(item.second->width()==52) {
     auto at=item.second->bounds(get_draw_position()).get_left_top()+Point<int16_t>(20,8);
     send_cursor(false,at); send_cursor(true,at); send_cursor(false,at); return;
   }
 }
 void hover() {
   for(const auto& item:buttons) if(item.second->width()==52) {
     auto b=item.second->bounds(get_draw_position());
     send_cursor(false,b.get_left_top()+Point<int16_t>(20,8)); return;
   }
 }
};
int main(int argc,char**argv) {
 std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
 std::filesystem::current_path(argv[2]);
 Setting<Fullscreen>::get().save(false); Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
 Setting<UILanguage>::get().save("zh-CN");
 assert(!Window::get().init()); UI::get().init();
 CharStats stats; stats.set_stat(MapleStat::Id::LEVEL,40); stats.change_job(220);
 SkillBook skills; Book book(stats,skills); book.verify_layout();
 auto ack=[&](int code,int seconds){book.accept_job_switch_status("[JobSwitch:state:v1] "+std::to_string(Stage::get().get_mapid())+" 220 "+std::to_string(code)+" "+std::to_string(seconds));book.update();};
 assert(book.disabled()); ack(1,18); assert(book.disabled());
 ack(0,0); assert(!book.disabled());
 book.click_entry();book.update();assert(book.disabled());
 book.click_entry();book.update();assert(book.disabled());
 ack(0,0);assert(!book.disabled());
 book.accept_job_switch_status("[JobSwitch:state:v1] 999999 220 0 0");book.update();assert(!book.disabled());
 ack(4,0); assert(book.disabled()); ack(3,0); assert(book.disabled());
 book.accept_job_switch_status("[JobSwitch:state:v1] garbage");book.update();assert(book.disabled());
 ack(0,0); stats.change_job(211);book.update();assert(book.disabled());
 stats.change_job(220); ack(1,18);book.hover();
 Window::get().begin();book.draw(1);Window::get().end();
 Setting<ScreenshotFolder>::get().save(argv[3]);Window::get().take_screenshot();
 std::cout<<"PASS: footer bounds/no overlaps, status, unsupported job and stale-map filtering\n";
}
'''
with tempfile.TemporaryDirectory(prefix='openstory-job-switch-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'JobSwitchCheck.app'
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
    output = build / 'job-switch-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
