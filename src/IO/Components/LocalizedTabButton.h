#pragma once
#include "TwoSpriteButton.h"
#include "LocalizedArt.h"
namespace ms
{
    class LocalizedTabButton : public TwoSpriteButton
    {
    public:
        LocalizedTabButton(nl::node normal, nl::node selected, localization::Key key)
            : TwoSpriteButton(normal, selected)
        {
            auto make = [&](nl::node node) {
                nl::bitmap bitmap = node;
                return LocalizedArt(node, {{2, 3, static_cast<int16_t>(bitmap.width()-4),
                    static_cast<int16_t>(bitmap.height()-6), key, Color::Name::WHITE}});
            };
            art[0] = make(normal); art[1] = make(selected);
        }
        void draw(Point<int16_t> parent) const override
        {
            if (!active) return;
            if (Setting<UILanguage>::get().load() != "zh-CN") { TwoSpriteButton::draw(parent); return; }
            art[state == MOUSEOVER || state == PRESSED].draw(parent);
        }
    private:
        LocalizedArt art[2];
    };
}
