#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <iostream>
#include <stdexcept>
#include <string_view>

std::string to_string(std::string_view str) {
	return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
	throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
	throw std::runtime_error(to_string(message) +
							 reinterpret_cast<const char *>(glewGetErrorString(error)));
}

GLuint create_shader(GLenum shader_type, const char *shader_source) {
	GLuint shader = glCreateShader(shader_type);

	glShaderSource(shader, 1, &shader_source, NULL);
	glCompileShader(shader);

	GLint compileStatus;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE) {
		GLsizei log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
		std::string info_log(log_length, '\0');
		glGetShaderInfoLog(shader, log_length, &log_length, info_log.data());
		throw std::runtime_error("Shader compilation error: " + info_log);
	}

	return shader;
}

GLuint create_program(GLuint vertexShader, GLuint fragmentShader) {
	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glLinkProgram(program);

	GLint linkStatus;
	glGetShaderiv(program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE) {
		GLsizei log_length = 0;
		glGetShaderiv(program, GL_INFO_LOG_LENGTH, &log_length);
		std::string info_log(log_length, '\0');
		glGetProgramInfoLog(program, log_length, &log_length, info_log.data());
		throw std::runtime_error("Program link error: " + info_log);
	}

	return program;
}

const char fragmentSource[] =
	R"(#version 330 core
in vec3 color;
in vec2 pos;
// flat in vec3 color;

layout (location = 0) out vec4 out_color;

void main() {
	int n = 10;
	float c = mod(floor(n * pos.x) + floor(n * pos.y), 2.0);

	// out_color = vec4(color, 1.0);
	out_color = vec4(vec3(c, c, c), 1.0);
}
)";

const char vertexSource[] =
	R"(#version 330 core
const vec2 VERTICES[3] = vec2[3](
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0)
);

const vec3 COLORS[3] = vec3[3](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

out vec3 color;
out vec2 pos;
// flat out vec3 color;

void main() {
	gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);

	// color = COLORS[gl_VertexID];
	// or
	// color = vec3(gl_Position.xy, 0.0);

	pos = gl_Position.xy;
}
)";

int main() try {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		sdl2_fail("SDL_Init: ");
	}

	SDL_Window *window = SDL_CreateWindow(
		"Graphics course practice 1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800,
		600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

	if (!window) {
		sdl2_fail("SDL_CreateWindow: ");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);

	if (!gl_context) {
		sdl2_fail("SDL_GL_CreateContext: ");
	}

	if (auto result = glewInit(); result != GLEW_NO_ERROR) {
		glew_fail("glewInit: ", result);
	}

	if (!GLEW_VERSION_3_3) {
		throw std::runtime_error("OpenGL 3.3 is not supported");
	}

	// create_shader(GL_FRAGMENT_SHADER, "somebody once told me");

	GLuint fragmentShader = create_shader(GL_FRAGMENT_SHADER, fragmentSource);
	GLuint vertexShader	  = create_shader(GL_VERTEX_SHADER, vertexSource);
	GLuint program		  = create_program(vertexShader, fragmentShader);

	GLuint vao;
	glGenVertexArrays(1, &vao);

	glClearColor(0.8f, 0.8f, 1.f, 0.f);

	glUseProgram(program);

	bool running = true;
	while (running) {
		for (SDL_Event event; SDL_PollEvent(&event);) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		if (!running) {
			break;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
} catch (std::exception const &e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
