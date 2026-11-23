#!/usr/bin/env python3
"""Render and interact with the production training panel without a server.
Uses temporary Settings; screenshots stay in build-training/panel-check.
"""
from pathlib import Path
import shlex, subprocess, tempfile, argparse
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build-training')
parser.add_argument('--compile-only', action='store_true', help='Compile production checks without starting a window')
args = parser.parse_args()
build = root / args.build_dir
subprocess.run(['cmake', '--build', str(build), '-j', '6'], check=True, stdout=subprocess.DEVNULL)
source = r'''
#include "src/IO/UITypes/UITrainingPanel.h"
#include "src/IO/Window.h"
#include "src/Character/Player.h"
#include "src/Configuration.h"
#include "src/Gameplay/MapleMap/MapMobs.h"
#include "src/Gameplay/MapleMap/Drop.h"
#include "src/Gameplay/Combat/Skill.h"
#include "src/Gameplay/Physics/Physics.h"
#include "src/Gameplay/FlightMotion.h"
#include "src/Gameplay/MapleMap/MapPortals.h"
#include <nlnx/nx.hpp>
#include "src/Gameplay/Stage.h"
#include "src/Gameplay/Spawn.h"
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
class PickupDrop : public Drop {
public:
    PickupDrop() : Drop(123, 1, {0,0}, {0,0}, 0, 2, false) {}
    void draw(double, double, float) const override {}
};
int main(int argc,char**argv) {
    std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
    std::filesystem::current_path(argv[2]);
    Setting<Fullscreen>::get().save(false); Setting<Width>::get().save(800); Setting<Height>::get().save(600);
    Setting<FontPathCJK>::get().save("/System/Library/Fonts/STHeiti Light.ttc");
    assert(!Window::get().init()); assert(!Sound::init());
    CharLook::init();
    Skill dash(5001005);
    assert(dash.can_use(10,Weapon::GUN,Job(500),100,100,0)==SpecialMove::FBR_NONE);

    CharEntry entry{}; entry.look.faceid=20000; entry.look.hairid=30000;
    Player player(entry);
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

    // Only authoritative per-monster messages pin a mob. Stale movement,
    // control assignment, impulses, and repeated confirmations cannot move it.
    Physics physics;
    for (int mode : {2,5}) {
        PickupDrop drop;
        assert(drop.can_pet_loot(true,true));
        drop.expire(mode,nullptr);
        assert(!drop.can_pet_loot(true,true));
        for (int tick=0; tick<60; ++tick) drop.update(physics);
        assert(!drop.is_active()); // pet pickups must disappear just like player pickups
    }

    // Virtual flight must not move the body; real flight bypasses gravity/walls.
    player.respawn({100,100},false);
    player.set_assist_flight(true,false,{30,-30});
    player.update(physics);
    assert(player.get_position()==Point<int16_t>(100,100));
    player.set_assist_flight(true,true,{30,-30});
    player.update(physics);
    assert(player.get_position()==Point<int16_t>(130,70));
    player.set_assist_flight(true,true,{});
    player.update(physics);
    assert(player.get_position()==Point<int16_t>(130,70));
    player.set_assist_flight(false,false);
    Range<int16_t> bounds(-200,200);
    auto straight=flight_next({0,0},100,0,bounds,bounds);
    auto diagonal=flight_next({0,0},100,-100,bounds,bounds);
    assert(std::abs(std::hypot(diagonal.x(),diagonal.y())-straight.x())<1e-8);
    assert(flight_next({0,0},0,0,bounds,bounds)==Point<double>(0,0));
    assert(flight_next({200,200},100,100,bounds,bounds)==Point<double>(200,200));
    assert(flight_speed(2,{400,300},800,600)==960);
    assert(flight_speed(1,{400,300},800,600)==480);
    assert(flight_speed(1,{799,300},800,600)==2400);
    assert(flight_speed(1,{400,599},800,600)==2400);
    assert(flight_speed(1,{600,300},800,600)>480);
    assert(flight_speed(1,{600,300},800,600)<2400);
    auto fast=flight_next({0,0},100,0,bounds,bounds,2400);
    assert(std::abs(fast.x()/straight.x()-5)<1e-8);
    auto real=flight_next({0,0},100,0,bounds,bounds,960);
    assert(std::abs(real.x()/straight.x()-2)<1e-8);
    // A missed UP press must not consume portal cooldown.
    auto portal_src=nl::nx::map["Map"]["Map1"]["100000000.img"]["portal"];
    MapPortals portal_map(portal_src,100000000);
    for (int i=0;i<1000;++i) portal_map.update({-30000,-30000});
    assert(portal_map.find_warp_at({-30000,-30000},true).name.empty());
    bool found_door=false;
    for (auto door : portal_src) if (int(door["pt"])==2) {
        auto warp=portal_map.find_warp_at({door["x"],door["y"]},true);
        assert(!warp.name.empty()); found_door=true; break;
    }
    assert(found_door);
    // Panel mode switches are local, independent of training server status.
    auto& stage=Stage::get();
    stage.get_player().get_stats().set_stat(MapleStat::Id::HP,50);
    stage.load(100000000,0);

    MapMobs vacuum_mobs;
    vacuum_mobs.spawn({1001, 100100, 0, 4, 0, false, 0, {200, 0}});
    vacuum_mobs.spawn({1002, 100100, 0, 4, 0, false, 0, {300, 0}});
    vacuum_mobs.set_training_pin(1001, true, {0, 0});
    vacuum_mobs.update(physics);
    auto* first = static_cast<Mob*>(vacuum_mobs.get_mobs()->get(1001).get());
    auto* second = static_cast<Mob*>(vacuum_mobs.get_mobs()->get(1002).get());
    assert(first->is_training_pinned() && first->get_position()==Point<int16_t>(0,0));
    assert(!second->is_training_pinned());
    vacuum_mobs.send_movement(1001, {999,999}, {Movement(999,999,0,0,4,0)});
    vacuum_mobs.update(physics);
    assert(first->get_position()==Point<int16_t>(0,0));
    first->set_control(2);
    first->apply_damage(10,true);
    for (int tick=0;tick<120;++tick) vacuum_mobs.update(physics);
    assert(first->get_position()==Point<int16_t>(0,0));
    assert(!first->is_controlled());
    vacuum_mobs.set_training_pin(1001, true, {0, 0});
    vacuum_mobs.set_training_pin(1002, true, {0, 0});
    vacuum_mobs.update(physics);
    assert(first->get_position()==Point<int16_t>(0,0));
    assert(second->is_training_pinned() && second->get_position()==Point<int16_t>(0,0));
    vacuum_mobs.set_training_pin(1001, false, {});
    vacuum_mobs.set_training_pin(1002, false, {});
    assert(!first->is_training_pinned() && !second->is_training_pinned());
    // The network pin can precede the queued spawn or concern a different viewer.
    vacuum_mobs.set_training_pin(1003,true,{45,20});
    vacuum_mobs.spawn({1003,100100,0,4,0,false,0,{300,0}});
    vacuum_mobs.update(physics);
    auto* delayed=static_cast<Mob*>(vacuum_mobs.get_mobs()->get(1003).get());
    assert(delayed->is_training_pinned() && delayed->get_position()==Point<int16_t>(45,20));
    vacuum_mobs.clear();
    assert(vacuum_mobs.get_mobs()->size()==0);


    TrainingAssistStatus status;
    std::vector<std::string> commands;
    // Real Stage input dispatch keeps the acknowledged automation session alive.
    assert(stage.training_auto_notice("[TrainingAssist:auto:v1] 100000000 0"));
    stage.send_key(KeyType::Id::FACE, 0, true);
    stage.send_key(KeyType::Id::FACE, 0, false);
    stage.send_cursor(true, {-100,-100});
    stage.send_cursor(false, {-100,-100});
    assert(stage.get_training_status().stop_reason.empty());
    stage.stop_training_auto("Server stopped auto attack", false);
    assert(stage.get_training_status().stop_reason == "服务端已停止挂机");
    stage.get_training_status().stop_reason.clear();
    stage.get_player().get_skills().set_skill(5001001,1,20,0);
    stage.get_player().get_skills().set_skill(5001002,1,20,0);
    stage.get_player().get_skills().set_skill(5001003,1,20,0);
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
    panel.click(6); assert(stage.get_flight_mode()==1); assert(!panel.expanded()); panel.click(0);
    panel.click(7); assert(stage.get_flight_mode()==2); assert(!panel.expanded()); panel.click(0);
    panel.click(7); assert(stage.get_flight_mode()==0); assert(panel.expanded());
    panel.click(6); stage.clear(); assert(stage.get_flight_mode()==0);
    stage.load(100000000,0);
    stage.get_player().get_skills().set_skill(5001001,1,20,0);
    stage.get_player().get_skills().set_skill(5001002,1,20,0);
    stage.get_player().get_skills().set_skill(5001003,1,20,0);
    panel.click(0);
    // Ordinary map transfers keep flight, full clears/login do not.
    stage.set_flight_mode(2); stage.clear(true); stage.clear(true);
    assert(stage.get_flight_mode()==0);
    stage.load(100000000,0); assert(stage.get_flight_mode()==2);
    stage.clear(); stage.load(100000000,0); assert(stage.get_flight_mode()==0);
    stage.set_flight_mode(1); stage.clear(true);
    stage.load(100000000,0); assert(stage.get_flight_mode()==1);
    stage.set_flight_mode(2);
    stage.loadplayer(entry); // a new character/login must not inherit flight
    assert(stage.get_flight_mode()==0);
    stage.get_player().get_stats().set_stat(MapleStat::Id::HP,50);

    assert(commands.back()=="@assist sync");
    ack("0 0 0 0 -1 800 20"); render();
    auto before=commands.size(); panel.click(1); assert(commands.size()==before);
    ack("1 0 0 0 -1 800 20"); render();
    auto pet_before=commands.size(); panel.click(12); assert(commands.size()==pet_before);
    status.accept_pet_vacuum("[TrainingAssist:petvac:v1] 100000000 0",100000000); panel.update();
    panel.click(12); assert(commands.back()=="@assist petvac on");
    status.accept_pet_vacuum("[TrainingAssist:petvac:v1] 100000000 1",100000000); panel.update();
    panel.click(12); assert(commands.back()=="@assist petvac off");
    status.accept_pet_vacuum("[TrainingAssist:petvac:v1] 100000000 0",100000000); panel.update();
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
    stage.get_player().get_skills().set_skill(5001001,1,20,0);
    panel.click(13); panel.verify_bounds(); render();
    panel.click(30); panel.click(21); // enable slot two: normal attack duplicates slot one
    auto rotation_before=commands.size(); panel.click(41); assert(commands.size()==rotation_before);
    panel.click(21); // first learned skill
    panel.click(41); assert(commands.back()=="@assist rotation 0:2000 5001001:1000");
    ack("1 1 1 1 0 0 0"); render(); panel.click(40); render();
    status.accept_vacuum_count("[TrainingAssist:vac-count:v1] 100 42 42",100);
    ack("1 1 1 1 -1 0 0"); render();
    panel.click(7); assert(stage.get_flight_mode()==2); assert(!panel.expanded()); panel.click(0);
    panel.click(9); assert(stage.get_flight_mode()==0); assert(commands.back()=="@assist off");
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
    if args.compile_only:
        print('PASS: production UI/mob regression harness compiled; window was not started')
    else:
        subprocess.run([str(executable),str(root/'wz'),str(work),str(output)],cwd=work,check=True)
