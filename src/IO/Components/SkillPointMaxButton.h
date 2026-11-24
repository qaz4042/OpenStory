#pragma once

#include "Button.h"
#include "../../Graphics/Texture.h"
#include "../../Graphics/Text.h"
#include "../../Configuration.h"
#include <nlnx/node.hpp>
#include <algorithm>

namespace ms {
// Match BtSpUp's pixel border and four state palettes, with room for one label.
class SkillPointMaxButton : public Button {
public:
    explicit SkillPointMaxButton(nl::node source) {
        active = true;
        pressed = false;
        state = NORMAL;
        const char* names[] = {"normal", "disabled", "mouseOver", "pressed"};
        for (int i = 0; i < 4; ++i) {
            auto bitmap = source[names[i]]["0"].get_bitmap();
            const int sw = bitmap.width(), sh = bitmap.height();
            if (sw < 6 || sh < 6) continue;
            const auto* data = static_cast<const uint8_t*>(bitmap.data());
            std::vector<uint8_t> pixels(BUTTON_WIDTH * sh * 4);
            for (int y = 0; y < sh; ++y) {
                for (int x = 0; x < BUTTON_WIDTH; ++x) {
                    // Preserve the two-pixel border; extend the clean left
                    // interior column instead of stretching the original '+'.
                    int sx = x < 2 ? x : x >= BUTTON_WIDTH - 2 ? sw - (BUTTON_WIDTH - x) : 2;
                    std::copy_n(data + (y * sw + sx) * 4, 4,
                                pixels.data() + (y * BUTTON_WIDTH + x) * 4);
                }
            }
            skins[i] = Texture::from_pixels(std::string("skill-point-max-native-v1/") + names[i],
                                            BUTTON_WIDTH, sh, std::move(pixels), {});
            button_height = sh;
        }
    }
    void draw(Point<int16_t> parent) const override {
        if (!active) return;
        const auto at = parent + position;
        skins[state <= PRESSED ? state : NORMAL].draw(at);
        const bool disabled = state == DISABLED;
        const bool down = state == PRESSED;
        Text(Text::A11M, Text::CENTER, Color::Name::WHITE,
             Setting<UILanguage>::get().load() == "zh-CN" ? "满" : "M")
            .draw(DrawArgument(at + Point<int16_t>(BUTTON_WIDTH / 2, down ? -2 : -3),
                               0.82f, 0.82f, disabled ? 0.7f : 1.0f));
    }
    void update() override {}
    Rectangle<int16_t> bounds(Point<int16_t> parent) const override {
        return {parent + position, parent + position + Point<int16_t>(BUTTON_WIDTH, button_height)};
    }
    int16_t width() const override { return BUTTON_WIDTH; }
    Point<int16_t> origin() const override { return {}; }
    Cursor::State send_cursor(bool, Point<int16_t>) override { return Cursor::IDLE; }
private:
    static constexpr int16_t BUTTON_WIDTH = 16;
    int16_t button_height = 12;
    Texture skins[4];
};
}
