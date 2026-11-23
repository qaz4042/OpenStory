#pragma once

#include "Button.h"
#include "../../Graphics/Texture.h"
#include "../../Graphics/Text.h"
#include "../../Configuration.h"
#include <nlnx/nx.hpp>

namespace ms {
// Reuse the game's native button skin with a localized text label.
class LabeledButton : public Button {
public:
    LabeledButton(const char* zh, const char* en, int16_t width) : zh(zh), en(en), button_width(width) {
        active = true; pressed = false; state = NORMAL;
        const char* names[] = {"normal", "disabled", "mouseOver", "pressed"};
        for (int i = 0; i < 4; ++i) skins[i] = Texture(nl::nx::ui["Basic.img"]["BtOK"][names[i]]["0"]);
    }
    void draw(Point<int16_t> parent) const override {
        if (!active) return;
        const auto& skin = skins[state <= PRESSED ? state : NORMAL];
        skin.draw(DrawArgument(parent + position, float(button_width) / skin.width(), 18.0f / skin.height()));
        Text(Text::A11M, Text::CENTER, Color::Name::WHITE,
             Setting<UILanguage>::get().load() == "zh-CN" ? zh : en)
            .draw(parent + position + Point<int16_t>(button_width / 2, -4));
    }
    void update() override {}
    Rectangle<int16_t> bounds(Point<int16_t> parent) const override {
        return {parent + position, parent + position + Point<int16_t>(button_width, 18)};
    }
    int16_t width() const override { return button_width; }
    Point<int16_t> origin() const override { return {}; }
    Cursor::State send_cursor(bool, Point<int16_t>) override { return Cursor::IDLE; }
private:
    Texture skins[4];
    const char* zh;
    const char* en;
    int16_t button_width;
};
}
