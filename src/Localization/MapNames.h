#pragma once
#include "Locale.h"
#include <string_view>

namespace ms::localization
{
    // Display-only fallback vocabulary for the current English String.nx.
    // IDs and original resources are preserved. These are project translations.
    inline std::string map_name(std::string_view english, std::string_view language)
    {
        if (language != "zh-CN") return std::string(english);
        static constexpr Message names[] = {
            {"Victoria Road", "维多利亚路"},
            {"Victoria Island", "金银岛"},
            {"Maple Road", "彩虹岛路"},
            {"Maple Island", "彩虹岛"},
            {"Dungeon", "地下迷宫"},
            {"Sleepywood", "林中之城"},
            {"SleepyWood", "林中之城"},
            {"Sleepy Dungeon", "沉睡的迷宫"},
            {"On the Way to the Harbor", "通往港口的路"},
            {"Lith Harbor", "明珠港"},
            {"Henesys", "射手村"},
            {"Henesys Market", "射手村市场"},
            {"Henesys Park", "射手村公园"},
            {"Henesys Hunting Ground", "射手训练场"},
            {"Hunting Ground in the Middle of the Forest", "森林中央的狩猎场"},
            {"Mushroom Garden", "蘑菇花园"},
            {"An Empty House", "空屋"},
            {"Pig Beach", "猪的海岸"},
            {"Ellinia", "魔法密林"},
            {"Perion", "勇士部落"},
            {"Kerning City", "废弃都市"},
            {"Kerning City Subway", "废弃都市地铁"},
            {"Nautilus Harbor", "诺特勒斯码头"},
            {"The Nautilus", "诺特勒斯号"},
            {"Nautilus", "诺特勒斯号"},
            {"Orbis", "天空之城"},
            {"Orbis Park", "天空之城公园"},
            {"Orbis Tower", "通天塔"},
            {"El Nath", "冰峰雪域"},
            {"El Nath Market", "冰峰雪域市场"},
            {"Ludibrium", "玩具城"},
            {"Ludibrium Village", "玩具城村庄"},
            {"Eos Tower", "奥斯塔"},
            {"Helios Tower", "赫丽奥斯塔"},
            {"Omega Sector", "地球防御本部"},
            {"Korean Folk Town", "童话村"},
            {"Aquarium", "水下世界"},
            {"Aqua Road", "水下世界路"},
            {"Leafre", "神木村"},
            {"Leafre Forest", "神木村森林"},
            {"Mu Lung", "武陵"},
            {"Herb Town", "百草堂"},
            {"Ariant", "阿里安特"},
            {"Magatia", "玛加提亚"},
            {"Nihal Desert", "尼哈沙漠"},
            {"Ereve", "圣地"},
            {"Rien", "里恩"},
            {"Rien Village", "里恩村"},
            {"Florina Beach", "黄金海岸"},
            {"Amherst", "南港"},
            {"Southperry", "彩虹岛码头"},
            {"Amoria", "婚礼村"},
            {"New Leaf City", "新叶城"},
            {"Singapore", "新加坡"},
            {"Boat Quay Town", "驳船码头"},
            {"CBD", "中央商务区"},
            {"Zipangu", "日本"},
            {"Mushroom Shrine", "蘑菇神社"},
            {"Showa Town", "昭和村"},
            {"Showa Street Market", "昭和市场"},
            {"Temple of Time", "时间神殿"},
            {"Time Lane", "时间之路"},
            {"Deep Sleepywood", "林中之城深处"},
            {"Ant Tunnel", "蚂蚁洞"},
            {"The Cave of Evil Eye", "邪恶之眼洞穴"},
            {"The Forest of Golem", "石巨人森林"},
            {"Golem's Temple", "石巨人寺院"},
            {"Dangerous Steam", "危险的蒸汽"},
            {"The Swamp of Despair", "绝望的沼泽"},
            {"Swamp", "沼泽"},
            {"The Tree That Grew", "大木林"},
            {"Southern Forest", "南部森林"},
            {"Northern Forest", "北部森林"},
            {"East Rocky Mountain", "东部岩山"},
            {"West Rocky Mountain", "西部岩山"},
            {"Burnt Land", "火焰之地"},
            {"Sunset Sky", "日落天空"},
            {"Hidden Street", "隐藏地图"},
            {"Hidden Map", "隐藏地图"},
            {"Free Market", "自由市场"},
            {"Cash Shop", "商城"},
        };
        for (const auto& name : names)
            if (english == name.en) return name.zh;
        // Repeated training areas have a shared name plus a numbered suffix.
        // Translate only a known whole base and a verified numeric/Roman suffix.
        const auto space = english.rfind(' ');
        if (space != std::string_view::npos)
        {
            const auto suffix = english.substr(space + 1);
            static constexpr std::string_view numbers[] = {
                "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"
            };
            for (auto number : numbers)
                if (suffix == number)
                    for (const auto& name : names)
                        if (english.substr(0, space) == name.en)
                            return std::string(name.zh) + " " + std::string(suffix);
        }
        return std::string(english);
    }
    inline std::string map_name(std::string_view english)
    {
        return map_name(english, Setting<UILanguage>::get().load());
    }
}
