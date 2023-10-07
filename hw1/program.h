#pragma once

#include <GL/glew.h>

class Program {
public:
    Program();

    void SetView(const float *view);
    void SetModel(const float *model);
    void SetProjection(const float *proj);

    void SetDrawIsolines(bool isolines);
    void SetTime(float time);

    void Use();

private:
    void CreateShaders();
    void CreateProgram();

    GLuint vertex_shader, fragment_shader, program;

    GLuint view_location;
	GLuint model_location;
	GLuint projection_location;
	GLuint draw_location;
	GLuint time_location;
};