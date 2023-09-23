#include <cmath>
#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
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
uniform int dash;
uniform float time;

layout (location = 0) out vec4 out_color;

void main()
{
    if (mod(dist + time, 40.0) >= 20.0 && dash == 1) {
        discard;
    } else {
        out_color = color;
    }
}
)";

GLuint create_shader(GLenum type, const char * source)
{
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
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct vec2
{
    float x;
    float y;
};

float dist(vec2 a, vec2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

struct vertex
{
    vec2 position;
    float dist;
    std::uint8_t color[4];
};

vec2 bezier(std::vector<vertex> const & vertices, float t)
{
    std::vector<vec2> points(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i)
        points[i] = vertices[i].position;

    // De Casteljau's algorithm
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
            points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
            points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
        }
    }
    return points[0];
}

void add_vertex(std::vector<vertex> & vertices, vec2 cur_vert, std::uint8_t color[4])
{
    if (vertices.empty()) {
        vertices.push_back({cur_vert, 0, *color});
    } else {
        vertices.push_back({cur_vert, vertices.back().dist + dist(vertices.back().position, cur_vert), *color});
    }
}

std::vector<vertex> vertex_bezier(std::vector<vertex> const & vertices, int quality)
{
    std::vector<vertex> ans;
    int count = (vertices.size() - 1) * quality;
    for (int i = 0; i <= count; i++) {
        auto cur_vert = bezier(vertices, i * 1.f / count);
        std::uint8_t color[4] = {255, 0, 0, 255};
        add_vertex(ans, cur_vert, color);
    }
    return ans;
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint dash_location = glGetUniformLocation(program, "dash");
    GLuint time_location = glGetUniformLocation(program, "time");

    glUseProgram(program);

    std::vector<vertex> vertices, beizer_verts;
    GLuint vao, vbo, vbob, vaob;
    /*glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
    float x = 0;
    glGetBufferSubData(GL_ARRAY_BUFFER, sizeof(vertex), 4, &x);

    std::cout << x << std::endl;*/
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, dist));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)offsetof(vertex, color));

    int quality = 4;

    glGenVertexArrays(1, &vaob);
    glBindVertexArray(vaob);
    glGenBuffers(1, &vbob);
    glBindBuffer(GL_ARRAY_BUFFER, vbob);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, dist));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)offsetof(vertex, color));

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    glLineWidth(5.f);
    glPointSize(10);

    float time = 0.f;

    bool running = true;
    bool need_redraw = false;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                vec2 cur_vert = {(float)event.button.x, (float)event.button.y};
                std::uint8_t color[4] = {0, 0, 0, 255};
                add_vertex(vertices, cur_vert, color);
            }
            else if (event.button.button == SDL_BUTTON_RIGHT && !vertices.empty())
            {
                vertices.pop_back();
            }
            beizer_verts = vertex_bezier(vertices, quality);
            need_redraw = true;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_LEFT && quality > 1)
            {
                --quality;
            }
            else if (event.key.keysym.sym == SDLK_RIGHT)
            {
                ++quality;
            }
            beizer_verts = vertex_bezier(vertices, quality);
            need_redraw = true;
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);

        const GLfloat view[16] = {
            2.f / width, 0, 0, -1.f,
            0, -2.f / height, 0, 1.f,
            0, 0, 1, 0,
            0, 0, 0, 1
        };

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        if (need_redraw) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ARRAY_BUFFER, vbob);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * beizer_verts.size(), beizer_verts.data(), GL_DYNAMIC_DRAW);
            need_redraw = false;
        }

        glBindVertexArray(vaob);
        glUniform1i(dash_location, 1);
        glUniform1f(time_location, time*50);
        glDrawArrays(GL_LINE_STRIP, 0, beizer_verts.size());

        glBindVertexArray(vao);
        glUniform1i(dash_location, 0);
        glDrawArrays(GL_LINE_STRIP, 0, vertices.size());
        glDrawArrays(GL_POINTS, 0, vertices.size());

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
