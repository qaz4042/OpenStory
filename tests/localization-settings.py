#!/usr/bin/env python3
"""macOS integration check: real settings UI, fonts, preview/cancel/save/reload.

Uses production CMake objects and NX assets, opens a temporary render window,
and keeps all Settings writes in a temporary directory. No server connection.
Screenshots are saved under build-macos/localization-check/.
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
#include "src/Localization/Locale.h"
#include "src/IO/UITypes/UINotice.h"
#include "src/IO/UITypes/UIJoypad.h"
#include "src/IO/UITypes/UIStatsInfo.h"
#include "src/IO/UITypes/UIItemInventory.h"
#include "src/IO/UITypes/UIQuestHelper.h"
#include "src/IO/UITypes/UILogin.h"
#include "src/IO/Components/LoginResources.h"
#include "src/IO/UITypes/UIStatusBar.h"
#include "src/IO/UITypes/UISkillBook.h"
#include "src/Gameplay/Stage.h"
#include "src/IO/UITypes/UIKeyConfig.h"
#include "src/IO/UITypes/UILoginNotice.h"
#include "src/IO/UITypes/UIPartySettings.h"
#include "src/IO/UITypes/UIPartySearchStart.h"
#include "src/Localization/Actions.h"
#include "src/IO/KeyAction.h"
#include "src/IO/UITypes/UIOptionMenu.h"
#include "src/IO/Window.h"
#include "src/Audio/Audio.h"
#include "src/Util/NxFiles.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace ms;
class TestMenu : public UIOptionMenu {
public:
    void click_control(unsigned id) {
        auto bounds = buttons.at(id)->bounds(get_draw_position());
        auto at = bounds.get_left_top() + Point<int16_t>(bounds.width()/2, bounds.height()/2);
        send_cursor(false, at); send_cursor(true, at); send_cursor(false, at);
    }
};
void render(UIElement& menu) {
    Window::get().begin(); menu.draw(1.0f); Window::get().end();
    static int frame = 0;
    static const auto folder = Setting<ScreenshotFolder>::get().load();
    Setting<ScreenshotFolder>::get().save(folder + "/" + std::to_string(++frame));
    Window::get().take_screenshot();
}
template<class T> class TestNotice : public T {
public:
    using T::T;
    void press(unsigned id) { this->button_pressed(id); }
    void place(bool check_buttons = false) {
        this->position = Point<int16_t>((1024 - this->dimension.x()) / 2, (768 - this->dimension.y()) / 2);
        if (!check_buttons) return;
        for (const auto& button : this->buttons) {
            const auto bounds = button.second->bounds(Point<int16_t>());
            assert(bounds.left() >= 0 && bounds.top() >= 24);
            assert(bounds.right() <= this->dimension.x());
            assert(bounds.bottom() <= this->dimension.y());
        }
    }
    void click_button(unsigned id) {
        auto bounds = this->buttons.at(id)->bounds(this->get_draw_position());
        auto at = bounds.get_left_top() + Point<int16_t>(bounds.width()/2, bounds.height()/2);
        this->send_cursor(false, at); // hover
        this->send_cursor(true, at);
        this->send_cursor(false, at);
    }
};
int main(int argc, char** argv) {
    if (argc == 2) { assert(Setting<UILanguage>::get().load() == "zh-CN"); return 0; }
    // Load game files, then isolate every config write from the user's data.
    std::filesystem::current_path(argv[1]);
    assert(!NxFiles::init());
    std::filesystem::current_path(argv[2]);
    Setting<UILanguage>::get().save("en");
    Setting<Fullscreen>::get().save(false);
    Setting<Width>::get().save(1024); Setting<Height>::get().save(768);
    Setting<ScreenshotFolder>::get().save(argv[3]);
    assert(!Window::get().init());
    assert(std::filesystem::equivalent(std::filesystem::current_path(), std::filesystem::path(argv[2])));
    assert(!Sound::init()); assert(!Music::init());
    // Verify every Chinese catalog codepoint exists in the selected system face.
    FT_Library library; FT_Face face;
    assert(!FT_Init_FreeType(&library));
    assert(!FT_New_Face(library, "/System/Library/Fonts/STHeiti Light.ttc", 0, &face));
    for (unsigned i = 0; i < static_cast<unsigned>(localization::Key::Count); ++i) {
        auto key = static_cast<localization::Key>(i);
        std::string chinese = localization::text(key, "zh-CN");
        assert(!chinese.empty());
        assert(std::string(localization::text(key, "unknown")) == localization::text(key, "en"));
        for (size_t j = 0; j < chinese.size();) {
            size_t consumed = 0;
            auto codepoint = Text::utf8_decode(chinese.data(), chinese.size(), j, consumed);
            assert(consumed > 0); assert(FT_Get_Char_Index(face, codepoint) != 0);
            j += consumed;
        }
    }
    FT_Done_Face(face); FT_Done_FreeType(library);
    TestMenu menu;
    render(menu); // English baseline
    menu.click_control(3); // Chinese preview
    assert(Setting<UILanguage>::get().load() == "zh-CN");
    render(menu);
    // Selection now commits immediately, including Escape/Cancel paths.
    menu.send_key(0, true, true);
    assert(Setting<UILanguage>::get().load() == "zh-CN");
    Setting<UILanguage>::get().save("en"); Configuration::get().load();
    assert(Setting<UILanguage>::get().load() == "zh-CN");
    menu.makeactive(); render(menu);
    menu.click_control(0);
    TestMenu reopened; render(reopened);
    reopened.click_control(3); reopened.click_control(1); // English + Cancel
    assert(Setting<UILanguage>::get().load() == "en");
    reopened.makeactive(); reopened.click_control(3); reopened.send_key(0, true, true);
    assert(Setting<UILanguage>::get().load() == "zh-CN");
    Setting<UILanguage>::get().save("en"); Configuration::get().load();
    assert(Setting<UILanguage>::get().load() == "zh-CN");
    reopened.makeactive(); render(reopened);
    // Exercise production modal buttons and callbacks in both languages.
    for (const auto* language : {"en", "zh-CN"}) {
        Setting<UILanguage>::get().save(language);
        int answer = -1, calls = 0;
        auto decide = [&](bool yes) { answer = yes; ++calls; };
        TestNotice<UIYesNo> quit(localization::get(localization::Key::QuitPrompt), decide);
        quit.place(); render(quit);
        quit.click_button(1); assert(answer == 0 && calls == 1 && !quit.is_active());
        quit.makeactive(); quit.send_key(KeyAction::Id::RETURN, true, false);
        assert(answer == 1 && calls == 2 && !quit.is_active());
        quit.makeactive(); quit.send_key(0, true, true);
        assert(answer == 0 && calls == 3 && !quit.is_active());
        TestNotice<UIYesNo> drop(localization::get(localization::Key::DropUntradable), decide);
        drop.place(); render(drop); drop.click_button(0);
        assert(answer == 1 && calls == 4);
        TestNotice<UIOk> info(localization::get(localization::Key::CannotDrop), decide);
        info.place(); render(info); info.click_button(0);
        assert(answer == 1 && calls == 5);
        int quantity = 0;
        TestNotice<UIEnterNumber> number(localization::get(localization::Key::DropQuantity),
            [&](int n) { quantity = n; }, 10, 3);
        number.place(); render(number); number.click_button(0);
        assert(quantity == 3 && !number.is_active());
        TestNotice<UIEnterText> entry(localization::get(localization::Key::Settings),
            [](const std::string&) {});
        entry.place(); render(entry); entry.send_key(0, true, true);
        assert(!entry.is_active());
        for (auto key : {localization::Key::NumbersOnly, localization::Key::NumberMinimum,
                         localization::Key::NumberMaximum, localization::Key::ForfeitQuest}) {
            TestNotice<UIOk> validation(localization::format(key, "10"), decide);
            validation.place(); render(validation);
        }
        for (auto key : {localization::Key::OK, localization::Key::Cancel}) {
            Text label(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE,
                       localization::get(key));
            assert(label.width() <= 38); // text fits the new common button
        }
    }
    Inventory inventory;
    SkillBook skills;
    for (const auto* language : {"en", "zh-CN"}) {
        Setting<UILanguage>::get().save(language);
        TestNotice<UISkillBook> macros(Stage::get().get_player().get_stats(), skills);
        macros.press(4); // BT_MACRO: exercise the actual settings entry
        macros.place(false); render(macros);
        TestNotice<UIStatusBar> status(Stage::get().get_player().get_stats());
        status.press(5); // BT_OPTIONS opens the system menu without a server action
        for (int i = 0; i < 30; ++i) status.update();
        render(status); status.send_key(0, true, true);
        TestNotice<UIKeyConfig> keyboard(inventory, skills);
        keyboard.place(); render(keyboard);
        keyboard.send_key(0, true, true); assert(!keyboard.is_active());
        TestNotice<UIJoypad> gamepad;
        gamepad.place(); render(gamepad);
        gamepad.send_key(0, true, true); assert(!gamepad.is_active());
        TestNotice<UIPartySettings> party(false);
        party.place(); render(party); party.click_button(1); assert(!party.is_active());
        TestNotice<UIPartySearchStart> recruit;
        recruit.place(); render(recruit); recruit.send_key(0, true, true); assert(!recruit.is_active());
        TestNotice<UIKeySelect> select([](bool) {}, false);
        select.place(); render(select); select.click_button(0); assert(!select.is_active());
        int confirmed = 0;
        TestNotice<UIKeyConfirm> confirm(true, [&]() { ++confirmed; }, false);
        confirm.place(); render(confirm); confirm.send_key(KeyAction::RETURN, true, false);
        assert(confirmed == 1 && !confirm.is_active());
        assert(std::string(localization::action_name(KeyAction::ATTACK)) ==
            localization::text(localization::Key::ActionATTACK, language));
    }
    // Warm fonts in the middle of sprite use, then render the original
    // settings artwork again: catches glyph/sprite atlas overlap visually.
    Setting<UILanguage>::get().save("zh-CN");
    TestMenu final_menu; render(final_menu);
    Setting<UILanguage>::get().save("en"); render(final_menu);
    for (const auto* language : {"en", "zh-CN"}) {
        Setting<UILanguage>::get().save(language);
        TestNotice<UIStatsInfo> stats(Stage::get().get_player().get_stats());
        stats.place(); render(stats);
        // Detail button opens the second stat pane without a network action.
        stats.press(10); render(stats);
        TestNotice<UIItemInventory> items(inventory); items.place(); render(items);
        TestNotice<UIQuestHelper> helper(Stage::get().get_player().get_quests()); helper.place(); render(helper);
    }
    std::filesystem::current_path(argv[1]);
    assert(Texture(login_title_resources()["BtNew"]["normal"]["0"]).width() == 92);
    UILogin login;
    for (int i = 0; i < 120; ++i) login.update();
    render(login);
    std::filesystem::current_path(argv[2]);
    Setting<UILanguage>::get().save("zh-CN"); Configuration::get().save();
    Sound::close();
    std::cout << "PASS: catalog fallback, Chinese glyphs, immediate language save, Escape/Cancel, Settings reload and bilingual notice callbacks\n";
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
    output = build / 'localization-check'
    output.mkdir(exist_ok=True)
    subprocess.run([str(executable), str(root / 'wz'), str(work), str(output)],
                   cwd=work, check=True)
    subprocess.run([str(executable), "--check-saved-language"], cwd=work, check=True)
