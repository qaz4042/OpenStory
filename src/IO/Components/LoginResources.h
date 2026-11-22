#pragma once
#include <nlnx/file.hpp>
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>
#include <fstream>
#include <memory>
#include <iostream>
namespace ms
{
    inline nl::node login_title_resources()
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
        if (archive && archive->root()["Login.img"]["Title"]["BtLogin"]["normal"]["0"])
            return archive->root()["Login.img"]["Title"];
        return nl::nx::ui["Login.img"]["Title"];
    }
}
