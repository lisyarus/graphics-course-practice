#pragma once

#include <string>
#include <unordered_map>

struct msdf_font
{
    std::string texture_path;

    struct glyph
    {
        int x, y;
        int width, height;
        int xoffset, yoffset;
        int advance;
    };

    std::unordered_map<char32_t, glyph> glyphs;
    float sdf_scale;
};

msdf_font load_msdf_font(std::string const & path);
