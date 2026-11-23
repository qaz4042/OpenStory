#pragma once
#include "MapleButton.h"
#include "LocalizedArt.h"
namespace ms
{
    class LocalizedMapleButton : public MapleButton
    {
    public:
        LocalizedMapleButton(nl::node source, localization::Key key, Point<int16_t> at = {})
            : MapleButton(source, at), language_switch(key == localization::Key::LanguageToggle)
        {
            const char* names[] = {"normal", "disabled", "mouseOver", "pressed", "keyFocused"};
            const State states[] = {NORMAL, DISABLED, MOUSEOVER, PRESSED, KEYFOCUSED};
            for (int i = 0; i < 5; ++i)
            {
                auto frame = source[names[i]]["0"];
                nl::bitmap bitmap = frame;
                if (!bitmap || bitmap.width() < 8 || bitmap.height() < 8) frame = source["normal"]["0"];
                bitmap = frame;
                if (!bitmap) continue;
                // The blank left padding contains the actual gradient. Sampling
                // across the caption also picks up the baked letters and their
                // dark outline, producing horizontal stripes on short buttons.
                art[states[i]] = LocalizedArt(frame, {{3, 3, static_cast<int16_t>(bitmap.width()-6), static_cast<int16_t>(bitmap.height()-6), key, Color::Name::WHITE, 4}}, true);
            }
        }
        LocalizedMapleButton(nl::node source, localization::Key key, int16_t x, int16_t y)
            : LocalizedMapleButton(source, key, Point<int16_t>(x, y)) {}
        void draw(Point<int16_t> parent) const override
        {
            if (active) draw(parent, 1.0f);
        }
        void draw(Point<int16_t> parent, float alpha) const
        {
            if (!language_switch && Setting<UILanguage>::get().load() != "zh-CN") {
                MapleButton::draw(parent, alpha); return;
            }
            // Keep the original state artwork fixed; only the caption sinks
            // when pressed, matching the approved button preview.
            art[state].draw(parent + position, language_switch, alpha,
                Point<int16_t>(0, state == PRESSED ? 1 : 0));
        }

    private:
        bool language_switch;
        LocalizedArt art[NUM_STATES];
    };
}
