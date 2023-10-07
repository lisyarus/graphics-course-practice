#pragma once

#include "GL/glew.h"

#include "program.h"
#include "camera.h"

#include <vector>

class Grid {
public:
    Grid();

    void Render(float time, Camera cam);

    void DrawIsolines(bool draw);
    void SetAngle(float angle);

    void IncreaseQuality();
    void DecreaseQuality();

private:
    void GenerateGeometry();

    GLuint vao, vbo, ebo;

    struct vertex {
        float x, y;
    };

    std::vector<vertex> vtxs;
    std::vector<uint32_t> indexes;

    size_t quality = 4;
    static constexpr float w = 512;
    static constexpr float h = 512;
    
    Program program;

    std::vector<float> model = {
		1.0, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	};
};