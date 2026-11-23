#!/usr/bin/env python3
"""Render real EN/CN equipment UI under forced atlas eviction.

Uses production objects and NX assets, compares framebuffer pixels and checks
GL errors without connecting to a server or modifying the user's Settings.
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
#include "src/Configuration.h"
#include "src/IO/Window.h"
#include "src/IO/UITypes/UIEquipInventory.h"
#include "src/Gameplay/Stage.h"
#include "src/Util/NxFiles.h"
#include "src/Graphics/GraphicsGL.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "src/Gameplay/PetSettings.h"
#include "src/Gameplay/MapleMap/ItemDrop.h"
#include "src/Gameplay/MapleMap/MesoDrop.h"
using namespace ms;
class Equipment : public UIEquipInventory {
public:
    using UIEquipInventory::UIEquipInventory;
    Point<int16_t> at(int x,int y) { return position+Point<int16_t>(x,y); }
    void pet() { send_cursor(true,at(dimension.x()-35,dimension.y()-16)); }
    void equip() { send_cursor(true,at(25,dimension.y()-16)); }
};
class Potion : public Icon::NullType {
public:
    int id; InventoryType::Id source;
    Potion(int id, InventoryType::Id source) : id(id), source(source) {}
    int32_t get_action_id() const override { return id; }
    InventoryType::Id get_source_tab() const override { return source; }
};
std::vector<unsigned char> capture() {
    auto* w = glfwGetCurrentContext(); int fw, fh;
    glfwGetFramebufferSize(w, &fw, &fh);
    std::vector<unsigned char> result(size_t(fw)*fh*4);
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
    // Pressure draws only this corner pixel, outside the equipment window.
    std::fill(result.begin(), result.begin()+fw*8*4, 0);
    std::fill(result.end()-fw*8*4, result.end(), 0);
    return result;
}
int main(int argc, char** argv) {
    std::filesystem::current_path(argv[1]); assert(!NxFiles::init());
    std::filesystem::current_path(argv[2]);
    Setting<Fullscreen>::get().save(false);
    Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
    assert(!Window::get().init());
    Inventory inventory;
    inventory.add_item(InventoryType::Id::USE,1,2000000,false,0,10,"",0);
    inventory.add_item(InventoryType::Id::USE,2,2000003,false,0,10,"",0);
    Equipment equipment(inventory);
    equipment.pet();
    Icon hp(std::make_unique<Potion>(2000000,InventoryType::Id::USE),Texture(),1);
    Icon mp(std::make_unique<Potion>(2000003,InventoryType::Id::USE),Texture(),1);
    equipment.send_icon(hp,equipment.at(25,100));
    equipment.send_icon(mp,equipment.at(25,172));
    assert(Configuration::get().get_auto_hp_pot()==2000000);
    assert(Configuration::get().get_auto_mp_pot()==2000003);
    equipment.send_icon(mp,equipment.at(25,100)); // wrong recovery type
    assert(Configuration::get().get_auto_hp_pot()==2000000);
    Icon absent(std::make_unique<Potion>(2000001,InventoryType::Id::USE),Texture(),1);
    equipment.send_icon(absent,equipment.at(25,100));
    assert(Configuration::get().get_auto_hp_pot()==2000000);
    equipment.doubleclick(equipment.at(25,100));
    equipment.doubleclick(equipment.at(25,172));
    assert(Configuration::get().get_auto_hp_pot()==0);
    assert(Configuration::get().get_auto_mp_pot()==0);
    equipment.equip();
    equipment.send_icon(hp,equipment.at(25,100));
    assert(Configuration::get().get_auto_hp_pot()==0);
    equipment.pet();
    equipment.send_icon(hp,equipment.at(25,100));
    equipment.send_icon(mp,equipment.at(25,172));
    assert(Setting<PetLootMesos>::get().load() && Setting<PetLootItems>::get().load());
    equipment.send_cursor(true,equipment.at(100,55)); // only mesos
    assert(Setting<PetLootMesos>::get().load() && !Setting<PetLootItems>::get().load());
    equipment.send_cursor(true,equipment.at(20,55)); // all off
    assert(!Setting<PetLootMesos>::get().load() && !Setting<PetLootItems>::get().load());
    Configuration::get().load();
    assert(!Setting<PetLootMesos>::get().load() && !Setting<PetLootItems>::get().load());
    equipment.send_cursor(true,equipment.at(20,55)); // restore only mesos
    equipment.send_cursor(true,equipment.at(115,134));
    assert(Setting<HPWarning>::get().load()==45);
    equipment.send_cursor(true,equipment.at(148,206));
    assert(Setting<MPWarning>::get().load()==55);
    for (int n=0;n<30;++n) equipment.send_cursor(true,equipment.at(148,134));
    assert(Setting<HPWarning>::get().load()==95);
    for (int n=0;n<30;++n) equipment.send_cursor(true,equipment.at(115,206));
    assert(Setting<MPWarning>::get().load()==0);
    Configuration::get().load();
    assert(Setting<HPWarning>::get().load()==95 && Setting<MPWarning>::get().load()==0);
    assert(pet_needs_potion(50,100,50));
    assert(!pet_needs_potion(51,100,50));
    assert(!pet_needs_potion(0,100,0));
    assert(pet_needs_potion(0,100,5));
    assert(!pet_needs_potion(0,0,50));
    assert(!pet_needs_potion(100,100,100));
    assert(!pet_needs_potion(96,100,100));
    assert(pet_needs_potion(95,100,100));
    assert(pet_needs_potion(1000000000,2000000000,50));
    const Texture texture;
    const Animation animation;
    ItemDrop monster(1,0,{},{},0,2,2000000,false,texture);
    ItemDrop discarded(2,0,{},{},0,2,2000000,true,texture);
    ItemDrop falling(3,0,{},{},0,0,2000000,false,texture);
    MesoDrop coins(4,0,{},{},0,2,false,animation);
    MesoDrop thrown(5,0,{},{},0,2,true,animation);
    assert(monster.can_pet_loot(true,true));
    assert(!monster.can_pet_loot(true,false));
    assert(monster.can_pet_loot(false,true));
    assert(coins.can_pet_loot(true,false));
    assert(!coins.can_pet_loot(false,true));
    assert(!discarded.can_pet_loot(true,true));
    assert(!thrown.can_pet_loot(true,true));
    assert(!falling.can_pet_loot(true,true));
    assert(!monster.can_pet_loot(false,false));
    monster.expire(0,nullptr);
    assert(!monster.can_pet_loot(true,true));
    Setting<HPWarning>::get().save(100);
    Configuration::get().save();
    Configuration::get().load();
    assert(Setting<HPWarning>::get().load()==95);
    Setting<HPWarning>::get().save(50);
    Setting<MPWarning>::get().save(30);
    // Pet accessories follow normal EQUIP -> EQUIPPED inventory acknowledgements.
    inventory.set_slotmax(InventoryType::Id::EQUIP,24);
    EnumMap<EquipStat::Id,uint16_t> equipStats;
    for (int i=0;i<2;++i) {
        inventory.add_equip(InventoryType::Id::EQUIP,10+i,1812000+i,true,0,0,0,equipStats,"",0,0,0,0);
        equipment.send_cursor(true,equipment.at(20+i*80,240)); // equip request, offline session
        assert(inventory.find_item(InventoryType::Id::EQUIPPED,1812000+i)==0);
        inventory.modify(InventoryType::Id::EQUIP,10+i,2,-181-i,Inventory::Movement::MOVE_EQUIP);
        assert(inventory.find_item(InventoryType::Id::EQUIPPED,1812000+i)==181+i);
        equipment.modify(181+i,0,0);
        equipment.send_cursor(true,equipment.at(20+i*80,240)); // unequip request
        assert(inventory.find_item(InventoryType::Id::EQUIPPED,1812000+i)==181+i);
        inventory.modify(InventoryType::Id::EQUIPPED,-181-i,2,10+i,Inventory::Movement::MOVE_UNEQUIP);
        assert(inventory.find_item(InventoryType::Id::EQUIPPED,1812000+i)==0);
    }
    for (const char* lang : {"en", "zh-CN"}) {
        Setting<UILanguage>::get().save(lang);
        GraphicsGL::get().clearscene();
        Window::get().begin(); equipment.draw(1.0f);
        GraphicsGL::get().flush(1.0f); auto reference = capture();
        int w,h; glfwGetFramebufferSize(glfwGetCurrentContext(),&w,&h);
        std::vector<unsigned char> pixels(size_t(w)*h*4);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        std::ofstream img(std::string(argv[3])+"/pet-"+lang+".ppm",std::ios::binary);
        img << "P6\n" << w << " " << h << "\n255\n";
        for (int y=h-1;y>=0;--y) for(int x=0;x<w;++x) img.write(reinterpret_cast<char*>(pixels.data()+(y*w+x)*4),3);
        for (float alpha : {0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.99f}) {
            GraphicsGL::get().clearscene();
            Window::get().begin(); equipment.draw(alpha);
            GraphicsGL::get().flush(1.0f);
            assert(glGetError() == GL_NO_ERROR);
            if (capture() != reference) {
                std::cerr << "FAIL: equipment changes with frame interpolation (" << lang << ", " << alpha << ")\n";
                return 1;
            }
        }
    }
    std::cout << "PASS: potion assignment, pickup filters, discard/landing states, percentage boundaries, persistence and EN/CN rendering\n";
}
'''
with tempfile.TemporaryDirectory(prefix='openstory-locale-') as directory:
    work = Path(directory)
    cpp = work / 'check.cpp'
    cpp.write_text(source)
    app = work / 'LocaleCheck.app'
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
    output = build / 'equipment-flicker-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
