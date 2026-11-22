#!/usr/bin/env python3
"""Render and interact with the production training panel without a server.
Uses temporary Settings; screenshots stay in build-training/panel-check.
"""
from pathlib import Path
import shlex, subprocess, tempfile, argparse
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build-training')
args = parser.parse_args()
build = root / args.build_dir
subprocess.run(['cmake', '--build', str(build), '-j', '6'], check=True, stdout=subprocess.DEVNULL)
source = r'''
#include "src/IO/UITypes/UITrainingPanel.h"
#include "src/IO/Window.h"
#include "src/Character/Player.h"
#include "src/Configuration.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class Panel : public UITrainingPanel {
public:
    using UITrainingPanel::UITrainingPanel;
    void click(int id) {
        auto b = buttons.at(id)->bounds(get_draw_position());
        auto at = b.get_left_top()+Point<int16_t>(b.width()/2,b.height()/2);
        send_cursor(false,at); send_cursor(true,at); send_cursor(false,at);
        update();
    }
    void verify_bounds() {
        for (auto& item : buttons) {
            auto b=item.second->bounds({});
            assert(b.left()>=0 && b.top()>=0 && b.right()<=dimension.x() && b.bottom()<=dimension.y());
        }
    }
};
int main(int argc,char**argv) {
    std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
    std::filesystem::current_path(argv[2]);
    Setting<Fullscreen>::get().save(false); Setting<Width>::get().save(800); Setting<Height>::get().save(600);
    Setting<FontPathCJK>::get().save("/System/Library/Fonts/STHeiti Light.ttc");
    assert(!Window::get().init()); assert(!Sound::init());
    Player player;
    player.set_training_invulnerable(true);
    assert(player.is_invincible());
    const auto hspeed=player.get_phobj().hspeed;
    const auto vforce=player.get_phobj().vforce;
    MobAttack hit{};
    const auto result=player.damage(hit);
    assert(result.damage==0);
    assert(player.get_phobj().hspeed==hspeed && player.get_phobj().vforce==vforce);
    player.set_training_invulnerable(false);
    assert(!player.is_training_invulnerable());
    TrainingAssistStatus status;
    std::vector<std::string> commands;
    Panel panel(status,[&](const std::string&cmd){commands.push_back(cmd);});
    auto ack=[&](const std::string&values) {
        status.accept("[TrainingAssist:state:v1] 100 "+values,100); panel.update();
    };
    int frame=0;
    auto render=[&]() {
        panel.verify_bounds(); Window::get().begin();panel.draw(1);Window::get().end();
        Setting<ScreenshotFolder>::get().save(std::string(argv[3])+"/"+std::to_string(++frame));
        Window::get().take_screenshot();
    };
    render(); panel.click(0); assert(panel.expanded()); render();
    assert(commands.back()=="@assist sync");
    ack("0 0 0 0 -1 800 20"); render();
    auto before=commands.size(); panel.click(1); assert(commands.size()==before);
    ack("1 0 0 0 -1 800 20"); render();
    panel.click(1); assert(commands.back()=="@assist invincible on");
    before=commands.size(); panel.click(1); assert(commands.size()==before); // pending: no repeat
    ack("1 1 0 0 -1 800 20");
    panel.click(2); assert(commands.back()=="@assist silence on");
    ack("1 1 1 0 -1 800 20");
    panel.click(3); assert(commands.back()=="@assist vac on");
    ack("1 1 1 1 -1 800 20");
    panel.click(5); panel.click(4); assert(commands.back()=="@assist auto on 5001001");
    ack("1 1 1 1 5001001 800 20"); render();
    panel.click(4); assert(commands.back()=="@assist auto off");
    ack("1 1 1 1 -1 800 20");
    panel.click(6); assert(commands.back()=="@assist vac settings 200 20");
    ack("1 1 1 1 -1 200 20");
    panel.click(7); assert(commands.back()=="@assist vac settings 200 5");
    ack("1 1 1 1 -1 200 5"); render();
    panel.click(9); assert(commands.back()=="@assist off");
    ack("1 0 0 0 -1 200 5");
    panel.click(10); assert(!panel.expanded()); render();
    Sound::close();
    std::cout<<"PASS: production panel rendered, button bounds, disabled/pending states and command callbacks\n";
}
'''
with tempfile.TemporaryDirectory(prefix='maple-panel-') as directory:
    work=Path(directory); cpp=work/'check.cpp'; cpp.write_text(source)
    executable=work/'check'
    flags=(build/'CMakeFiles/OpenStory.dir/flags.make').read_text()
    includes=next(x.split(' = ',1)[1] for x in flags.splitlines() if x.startswith('CXX_INCLUDES = '))
    link=shlex.split((build/'CMakeFiles/OpenStory.dir/link.txt').read_text())
    link=[x for x in link if not x.endswith('/src/MapleStory.cpp.o')]
    link[link.index('-o')+1]=str(executable)
    link+=['-std=c++17','-UNDEBUG','-I'+str(root),*shlex.split(includes),str(cpp),'-Wl,-rpath,'+str(root/'vendor/bass24/macos')]
    subprocess.run(link,cwd=build,check=True)
    output=build/'panel-check';output.mkdir(exist_ok=True)
    subprocess.run([str(executable),str(root/'wz'),str(work),str(output)],cwd=work,check=True)
