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
    UIEquipInventory equipment(Stage::get().get_player().get_inventory());
    for (const char* lang : {"en", "zh-CN"}) {
        Setting<UILanguage>::get().save(lang);
        GraphicsGL::get().clearscene();
        Window::get().begin(); equipment.draw(1.0f);
        GraphicsGL::get().flush(1.0f); auto reference = capture();
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
    std::cout << "PASS: EN/CN equipment is pixel-identical at six frame interpolation values\n";
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
