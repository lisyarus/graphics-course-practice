#include "program.h"

#include "shaders.h"

#include <string>
#include <stdexcept>

void Program::CreateShaders()
{
    auto creation = [](const char* source, GLenum type) {
        GLuint result = glCreateShader(type);

        glShaderSource(result, 1, &source, nullptr);
        glCompileShader(result);
        GLint status;
        glGetShaderiv(result, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE)
        {
            GLint info_log_length;
            glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
            std::string info_log(info_log_length, '\0');
            glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
            throw std::runtime_error("Shader compilation failed: " + info_log);
        }
        return result;
    };

    vertex_shader = creation(vertex_shader_source, GL_VERTEX_SHADER);
    fragment_shader = creation(fragment_shader_source, GL_FRAGMENT_SHADER);
}

void Program::CreateProgram() {
    CreateShaders();

	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		GLint info_log_length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetProgramInfoLog(program, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Program linkage failed: " + info_log);
	}
}

Program::Program() {
    CreateProgram();
    
    view_location = glGetUniformLocation(program, "view");
	model_location = glGetUniformLocation(program, "model");
	projection_location = glGetUniformLocation(program, "projection");
	draw_location = glGetUniformLocation(program, "draw_isoline");
	time_location = glGetUniformLocation(program, "time");
}

void Program::SetView(const float *view) {
    glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
}

void Program::SetModel(const float *model) {
    glUniformMatrix4fv(model_location, 1, GL_TRUE, model);
}

void Program::SetProjection(const float *proj) {
    glUniformMatrix4fv(projection_location, 1, GL_TRUE, proj);
}

void Program::SetDrawIsolines(bool isolines) {
    glUniform1i(draw_location, isolines);
}

void Program::SetTime(float time) {
    glUniform1f(time_location, time);
}

void Program::Use() {
    glUseProgram(program);    
}
