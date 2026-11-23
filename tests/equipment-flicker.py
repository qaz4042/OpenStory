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
using namespace ms;
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
    // A stretched one-pixel slice must not sample an adjacent blue sprite.
    Window::get().begin();
    const uint32_t red[8] = {0xffff0000,0xffff0000,0xffff0000,0xffff0000,
                            0xffff0000,0xffff0000,0xffff0000,0xffff0000};
    const uint32_t blue[8] = {0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
                             0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff};
    GraphicsGL::get().drawraw((size_t(1)<<53), 1, 8, red,
        Rectangle<int16_t>(50,350,50,58), {}, Color(Color::CWHITE), 0);
    GraphicsGL::get().drawraw((size_t(1)<<53)+1, 1, 8, blue,
        Rectangle<int16_t>(50,350,100,108), {}, Color(Color::CWHITE), 0);
    GraphicsGL::get().flush(1.0f);
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    for (int x : {50, 349}) {
        uint8_t pixel[4];
        glReadPixels(vp[0]+x*vp[2]/1024, vp[1]+(768-54)*vp[3]/768,
                     1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        assert(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0);
    }
    std::cout << "PASS: stretched UI slices do not bleed adjacent textures\n";
    UIEquipInventory equipment(Stage::get().get_player().get_inventory());
    // A solid opaque texture repeatedly fills the atlas without covering the
    // equipment window. Existing queued UI must survive eviction unchanged.
    for (const char* lang : {"en", "zh-CN"}) {
        Setting<UILanguage>::get().save(lang);
        Window::get().begin(); equipment.draw(1.0f);
        GraphicsGL::get().flush(1.0f); auto reference = capture();
        for (int frame = 0; frame < 8; ++frame) {
            Window::get().begin(); equipment.draw(1.0f);
            // Different IDs force actual allocation on every iteration.
            for (size_t n = 0; n < 5; ++n) {
                std::vector<uint8_t> pixels(size_t(2048)*2048*4, 255);
                GraphicsGL::get().drawraw((size_t(1)<<52) + frame*5+n,
                    2048, 2048, pixels.data(), Rectangle<int16_t>(0,1,0,1),
                    {}, Color(Color::CWHITE), 0.0f);
            }
            GraphicsGL::get().flush(1.0f);
            assert(glGetError() == GL_NO_ERROR);
            if (capture() != reference) {
                std::cerr << "FAIL: equipment pixels changed after atlas reuse (" << lang << ")\n";
                return 1;
            }
        }
    }
    std::cout << "PASS: EN/CN equipment remains pixel-identical across repeated atlas eviction\n";
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
