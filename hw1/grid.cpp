#include "grid.h"

#include <cstdint>
#include <iostream>
#include <cmath>

Grid::Grid() {
    GenerateGeometry();

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vtxs.size() * sizeof(vertex), vtxs.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexes.size() * sizeof(int), indexes.data(), GL_STATIC_DRAW); 

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) (0));

    program.Use();
    program.SetModel(model.data());
    program.SetDrawIsolines(false);
}

void Grid::Render(float time, Camera cam) {
    program.Use();
    program.SetTime(time);
    program.SetView(cam.GetView().data());
    program.SetProjection(cam.GetProjection().data());
    program.SetModel(model.data());
    
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, nullptr);
}

void Grid::DrawIsolines(bool draw) {
    program.SetDrawIsolines(draw);    
}

void Grid::SetAngle(float angle) {
    model = {
		std::cos(angle), 0.f, std::sin(angle), 0.f,
		0.f, 1.f, 0.f, 0.f,
		-std::sin(angle), 0.f, std::cos(angle), 0.f,
		0.f, 0.f, 0.f, 1.f
	};
}

void Grid::IncreaseQuality() {
    quality *= 2;
    GenerateGeometry();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexes.size() * sizeof(int), indexes.data(), GL_STATIC_DRAW); 
	glBufferData(GL_ARRAY_BUFFER, vtxs.size() * sizeof(vertex), vtxs.data(), GL_STATIC_DRAW);
}

void Grid::DecreaseQuality() {
    if (quality == 1) return;
    quality /= 2;
    GenerateGeometry();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexes.size() * sizeof(int), indexes.data(), GL_STATIC_DRAW); 
	glBufferData(GL_ARRAY_BUFFER, vtxs.size() * sizeof(vertex), vtxs.data(), GL_STATIC_DRAW);
}

void Grid::GenerateGeometry() {
	vtxs.clear();
	indexes.clear();
	
    for (int x = 0; x <= quality; x++) {
		for (int z = 0; z <= quality; z++) {
            float tx = (float) x / quality;
            float ty = (float) z / quality;
            float xc = -(float)w/2 * (tx) + (float)w/2* (1 - tx);
            float yc = -(float)h/2 * (ty) + (float)h/2* (1 - ty);

			vertex v = {static_cast<float>(xc) / 20, static_cast<float>(yc) / 20};
			vtxs.push_back(v);
		}
	}

    for (uint32_t j = 0; j < quality; j++) {
		for (uint32_t i = 0; i < quality; i++) {
			indexes.push_back(i + j * (quality + 1));
			indexes.push_back(i + j * (quality + 1) + 1);
			indexes.push_back(i + j * (quality + 1) + quality + 2);

			indexes.push_back(i + j * (quality + 1) + quality + 2);
			indexes.push_back(i + j * (quality + 1) + quality + 1);
			indexes.push_back(i + j * (quality + 1));
		}
	}
}
