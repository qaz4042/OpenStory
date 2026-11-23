#pragma once
#include <nlnx/file.hpp>
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>
#include <fstream>
#include <memory>
#include <iostream>
namespace ms
{
    inline nl::node retained_login_resources()
    {
        // Optional, separately retained v83 UI archive. Keep it alive for all
        // Texture/Animation nodes; never replace the compatible in-game UI.nx.
        static std::unique_ptr<nl::file> archive = []() {
            if (!std::ifstream("UI.Login.nx").good()) return std::unique_ptr<nl::file>();
            try { return std::make_unique<nl::file>("UI.Login.nx"); }
            catch (const std::exception& error) {
                std::cerr << "[Login UI] " << error.what() << '\n';
                return std::unique_ptr<nl::file>();
            }
        }();
        return archive ? archive->root()["Login.img"] : nl::node();
    }
    inline nl::node login_title_resources()
    {
        auto title = retained_login_resources()["Title"];
        if (title["BtLogin"]["normal"]["0"]) return title;
        return nl::nx::ui["Login.img"]["Title"];
    }

    inline nl::node login_race_resources()
    {
        auto race = retained_login_resources()["RaceSelect"];
        if (race["normal"]["BtNormal"]["normal"]["0"]
            && race["knight"]["BtKnight"]["normal"]["0"]
            && race["aran"]["BtAran"]["normal"]["0"]
            && race["BtSelect"]["normal"]["0"])
            return race;
        return nl::nx::ui["Login.img"]["RaceSelect"];
    }
}
