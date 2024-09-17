#include <cmath>
#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

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

const char vertex_shader_source[] =
	R"(#version 330 core

uniform mat4 view;

layout (location = 0) in vec2 in_position;
layout (location = 1) in float in_dist;
layout (location = 2) in vec4 in_color;

out vec4 color;
out float dist;

void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    color = in_color;
	dist = in_dist;
}
)";

const char fragment_shader_source[] =
	R"(#version 330 core

in vec4 color;
in float dist;

uniform float time;
uniform int dash;

layout (location = 0) out vec4 out_color;

void main()
{
	if (dash == 1 && mod(dist + time, 0.04) < 0.02)
		discard;
	else
		out_color = color;
}
)";

GLuint create_shader(GLenum type, const char *source) {
	GLuint result = glCreateShader(type);
	glShaderSource(result, 1, &source, nullptr);
	glCompileShader(result);
	GLint status;
	glGetShaderiv(result, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		GLint info_log_length;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Shader compilation failed: " + info_log);
	}
	return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
	GLuint result = glCreateProgram();
	glAttachShader(result, vertex_shader);
	glAttachShader(result, fragment_shader);
	glLinkProgram(result);

	GLint status;
	glGetProgramiv(result, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		GLint info_log_length;
		glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Program linkage failed: " + info_log);
	}

	return result;
}

struct vec2 {
	float x;
	float y;
};

struct vertex {
	vec2 position;
	float dist;
	std::uint8_t color[4];
};

vec2 bezier(std::vector<vertex> const &vertices, float t) {
	std::vector<vec2> points(vertices.size());

	for (std::size_t i = 0; i < vertices.size(); ++i) {
		points[i] = vertices[i].position;
	}

	// De Casteljau's algorithm
	for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
		for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
			points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
			points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
		}
	}
	return points[0];
}

std::vector<vertex> make_bezier(std::vector<vertex> const &vertices, int quality) {
	if (vertices.size() < 2) {
		return {};
	}
	int count = (vertices.size() - 1) * quality;
	std::vector<vertex> ans(count + 1);

	ans[0] = vertex{bezier(vertices, 0), 0, {255, 0, 0, 255}};
	for (int i = 1; i <= count; ++i) {
		vec2 cur = bezier(vertices, i * 1.f / count);
		ans[i]	 = vertex{cur,
						  ans[i - 1].dist + std::hypot(ans[i - 1].position.x - cur.x,
													   ans[i - 1].position.y - cur.y),
						  {255, 0, 0, 255}};
	}
	return std::move(ans);
}

int main() try {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		sdl2_fail("SDL_Init: ");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_Window *window = SDL_CreateWindow(
		"Graphics course practice 3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800,
		600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

	if (!window) {
		sdl2_fail("SDL_CreateWindow: ");
	}

	int width, height;
	SDL_GetWindowSize(window, &width, &height);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (!gl_context) {
		sdl2_fail("SDL_GL_CreateContext: ");
	}

	SDL_GL_SetSwapInterval(0);

	if (auto result = glewInit(); result != GLEW_NO_ERROR) {
		glew_fail("glewInit: ", result);
	}

	if (!GLEW_VERSION_3_3) {
		throw std::runtime_error("OpenGL 3.3 is not supported");
	}

	glClearColor(0.8f, 0.8f, 1.f, 0.f);

	auto vertex_shader	 = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
	auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
	auto program		 = create_program(vertex_shader, fragment_shader);

	GLuint view_location = glGetUniformLocation(program, "view");

	// std::array<vertex, 3> vertices = {
	//     vertex({1.0f, 0.0f}, {255, 0, 0, 255}),
	//     vertex({0.0f, 0.0f}, {0, 255, 0, 255}),
	//     vertex({0.0f, 1.0f}, {0, 0, 255, 255}),
	// };

	std::vector<vertex> vertices  = {};
	std::vector<vertex> bvertices = {};

	int quality = 4;

	GLuint vbo, vao, bvao, bvbo;

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// in_position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex),
						  (void *)offsetof(vertex, position));

	// in_dist
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vertex),
						  (void *)offsetof(vertex, dist));

	// in_color
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex),
						  (void *)offsetof(vertex, color));

	glGenBuffers(1, &bvbo);
	glBindBuffer(GL_ARRAY_BUFFER, bvbo);
	glGenVertexArrays(1, &bvao);
	glBindVertexArray(bvao);

	// in_position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex),
						  (void *)offsetof(vertex, position));

	// in_dist
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vertex),
						  (void *)offsetof(vertex, dist));

	// in_color
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex),
						  (void *)offsetof(vertex, color));

	glLineWidth(5.f);
	glPointSize(10);

	vec2 v_pos;
	bool new_vertices = false, change_quality = false;

	float scale;

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	GLuint timeUniform = glGetUniformLocation(program, "time");
	GLuint dashUniform = glGetUniformLocation(program, "dash");

	float time = 0.f;

	bool running = true;
	while (running) {
		for (SDL_Event event; SDL_PollEvent(&event);) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_WINDOWEVENT:
				switch (event.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					width  = event.window.data1;
					height = event.window.data2;
					glViewport(0, 0, width, height);
					scale = (float)width / height;
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT) {
					v_pos = {(float)2 * event.button.x / width - 1.0f,
							 -(float)2 * event.button.y / height + 1.0f};
					if (vertices.empty()) {
						vertices.push_back(vertex{v_pos, 0.f, {0, 0, 255, 255}});
					} else {
						vertices.push_back(
							vertex{v_pos,
								   std::hypot(vertices.back().position.x - v_pos.x,
											  vertices.back().position.y - v_pos.y),
								   {0, 0, 255, 255}});
					}
				} else if (event.button.button == SDL_BUTTON_RIGHT && !vertices.empty()) {
					vertices.pop_back();
				}
				bvertices	 = make_bezier(vertices, quality);
				new_vertices = true;
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_LEFT && quality > 1) {
					--quality;
				} else if (event.key.keysym.sym == SDLK_RIGHT) {
					++quality;
				}
				bvertices	   = make_bezier(vertices, quality);
				change_quality = true;
				break;
			}
		}

		if (!running) {
			break;
		}

		auto now = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration_cast<std::chrono::duration<float>>(
					   now - last_frame_start)
					   .count();
		last_frame_start = now;
		time += dt;

		glClear(GL_COLOR_BUFFER_BIT);

		float view[16] = {
			1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
		};

		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

		// glDrawArrays(GL_TRIANGLES, 0, vertices.size());

		if (new_vertices || change_quality) {
			glBindBuffer(GL_ARRAY_BUFFER, bvbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * bvertices.size(),
						 bvertices.data(), GL_DYNAMIC_DRAW);

			if (new_vertices) {
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) * vertices.size(),
							 vertices.data(), GL_DYNAMIC_DRAW);
			}
			change_quality = false;
			new_vertices   = false;
		}

		glBindVertexArray(bvao);
		glUniform1f(timeUniform, time / 10);
		glUniform1i(dashUniform, 1);
		glDrawArrays(GL_LINE_STRIP, 0, bvertices.size());

		glBindVertexArray(vao);
		glUniform1i(dashUniform, 0);
		glDrawArrays(GL_LINE_STRIP, 0, vertices.size());
		glDrawArrays(GL_POINTS, 0, vertices.size());

		SDL_GL_SwapWindow(window);
	}

	// Ну просто красивее так
	glDeleteVertexArrays(1, &bvao);
	glDeleteBuffers(1, &bvbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteProgram(program);
	glDeleteShader(fragment_shader);
	glDeleteShader(vertex_shader);

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
} catch (std::exception const &e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
