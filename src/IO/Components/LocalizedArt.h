#pragma once
#include "../../Graphics/Texture.h"
#include "../../Graphics/Text.h"
#include "../../Localization/Locale.h"
#include <nlnx/node.hpp>
#include <algorithm>
#include <vector>

namespace ms
{
    // Keep the original frame, gradients and coordinates. Only clear explicitly
    // audited text rectangles; English draws the original bitmap unchanged.
    class LocalizedArt
    {
    public:
        struct Caption { int16_t x, y, width, height; localization::Key key; Color::Name color; int16_t sample_x = -1; };
        LocalizedArt() = default;
        LocalizedArt(nl::node source, std::vector<Caption> captions, bool embossed = false)
            : original(source), captions(std::move(captions))
        {
            nl::bitmap bitmap = source;
            if (!bitmap) return;
            const auto* data = static_cast<const uint8_t*>(bitmap.data());
            std::vector<uint8_t> pixels(data, data + bitmap.width() * bitmap.height() * 4);
            for (const auto& caption : this->captions)
            {
                // Reconstruct each row from its background tone. A quantile
                // rejects the bright baked glyphs while preserving the source's
                // vertical gradient; dark-on-light labels use the upper quantile.
                for (int y = caption.y; y < caption.y + caption.height && y < bitmap.height(); ++y)
                {
                    std::vector<const uint8_t*> samples;
                    for (int x = caption.x; x < caption.x + caption.width && x < bitmap.width(); ++x)
                        samples.push_back(data + (y * bitmap.width() + x) * 4);
                    std::sort(samples.begin(), samples.end(), [](const auto* a, const auto* b) {
                        return int(a[0]) + a[1] + a[2] < int(b[0]) + b[1] + b[2];
                    });
                    if (samples.empty()) continue;
                    const auto* background = caption.sample_x >= 0
                        ? data + (y * bitmap.width() + caption.sample_x) * 4
                        : samples[(samples.size() - 1) * (caption.color == Color::Name::DARKGREY ? 3 : 1) / 4];
                    for (int x = caption.x; x < caption.x + caption.width && x < bitmap.width(); ++x)
                        for (int c = 0; c < 4; ++c)
                            pixels[(y * bitmap.width() + x) * 4 + c] = background[c];
                }
                labels.emplace_back(Text::A11M, Text::CENTER, caption.color,
                    localization::text(caption.key, "zh-CN"), caption.width, false);
                if (embossed)
                    shadows.emplace_back(Text::A11M, Text::CENTER, Color::Name::DARKGREY,
                        localization::text(caption.key, "zh-CN"), caption.width, false);
            }
            std::string key = "localized-art/" + std::to_string(bitmap.id());
            for (const auto& c : this->captions) key += "/" + std::to_string(static_cast<unsigned>(c.key));
            translated = Texture::from_pixels(key, bitmap.width(), bitmap.height(), std::move(pixels), source["origin"]);
        }
        void draw(Point<int16_t> at, bool force_translated = false, float alpha = 1.0f,
            Point<int16_t> caption_offset = {}) const
        {
            if ((!force_translated && Setting<UILanguage>::get().load() != "zh-CN") || !translated.is_valid()) { original.draw(DrawArgument(at, alpha)); return; }
            translated.draw(DrawArgument(at, alpha));
            at -= original.get_origin();
            for (size_t i = 0; i < captions.size(); ++i)
            {
                const auto& c = captions[i];
                const auto label_at = at + caption_offset
                    + Point<int16_t>(c.x + c.width / 2, c.y + (c.height - 12) / 2 - 6);
                if (!shadows.empty())
                    shadows[i].draw(DrawArgument(label_at + Point<int16_t>(1, 1), alpha));
                labels[i].draw(DrawArgument(label_at, alpha));
            }
        }
    private:
        Texture original, translated;
        std::vector<Caption> captions;
        std::vector<Text> labels;
        std::vector<Text> shadows;
    };
}
