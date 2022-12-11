#include "msdf_loader.hpp"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <fstream>
#include <stdexcept>
#include <filesystem>

msdf_font load_msdf_font(std::string const & path)
{
    rapidjson::Document document;

    {
        std::ifstream input(path, std::ios::binary);
        rapidjson::IStreamWrapper stream(input);
        document.ParseStream(stream);
    }

    msdf_font result;

    {
        auto pages = document["pages"].GetArray();
        assert(pages.Size() == 1);
        result.texture_path = (std::filesystem::path(path).parent_path() / pages[0].GetString()).string();
    }

    {
        auto const & sdf = document["distanceField"];
        result.sdf_scale = sdf["distanceRange"].GetFloat();
    }

    auto chars = document["chars"].GetArray();

    for (auto const & charInfo : chars)
    {
        char32_t id = charInfo["id"].GetUint();

        auto & data = result.glyphs[id];
        data.x = charInfo["x"].GetInt();
        data.y = charInfo["y"].GetInt();
        data.width = charInfo["width"].GetInt();
        data.height = charInfo["height"].GetInt();
        data.xoffset = charInfo["xoffset"].GetInt();
        data.yoffset = charInfo["yoffset"].GetInt();
        data.advance = charInfo["xadvance"].GetInt();
    }

    return result;
}
