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
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

#include "obj_parser.hpp"

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

const char dragon_vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 normal;
out vec3 position;

void main()
{
    gl_Position = projection * view * model * vec4(in_position, 1.0);
    position = (model * vec4(in_position, 1.0)).xyz;
    normal = normalize(mat3(model) * in_normal);
}
)";

const char dragon_fragment_shader_source[] =
R"(#version 330 core

uniform vec3 camera_position;

in vec3 normal;
in vec3 position;
in float ao;

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 light_direction = vec3(normalize(vec3(1.0, 2.0, 3.0)));
    vec3 light_color = vec3(0.8, 0.3, 0.0);
    vec3 ambient_light = vec3(0.2, 0.2, 0.4);

    vec3 reflected = 2.0 * normal * dot(normal, light_direction) - light_direction;
    vec3 camera_direction = normalize(camera_position - position);

    vec3 albedo = vec3(1.0, 1.0, 1.0);

    vec3 light = ambient_light + light_color * (max(0.0, dot(normal, light_direction)) + pow(max(0.0, dot(camera_direction, reflected)), 64.0));
    vec3 color = albedo * light;
    out_color = vec4(color, 1.0);
}
)";

const char rectangle_vertex_shader_source[] =
R"(#version 330 core

uniform vec2 center;
uniform vec2 size;

out vec2 texcoord;

vec2 vertices[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void main()
{
    vec2 vertex = vertices[gl_VertexID];
    gl_Position = vec4(vertex * size + center, 0.0, 1.0);
    texcoord = vertex * 0.5 + vec2(0.5);
}
)";

const char rectangle_fragment_shader_source[] =
R"(#version 330 core

in vec2 texcoord;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(texcoord, 0.0, 1.0);
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

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 7",
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

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto dragon_vertex_shader = create_shader(GL_VERTEX_SHADER, dragon_vertex_shader_source);
    auto dragon_fragment_shader = create_shader(GL_FRAGMENT_SHADER, dragon_fragment_shader_source);
    auto dragon_program = create_program(dragon_vertex_shader, dragon_fragment_shader);

    GLuint model_location = glGetUniformLocation(dragon_program, "model");
    GLuint view_location = glGetUniformLocation(dragon_program, "view");
    GLuint projection_location = glGetUniformLocation(dragon_program, "projection");

    GLuint camera_position_location = glGetUniformLocation(dragon_program, "camera_position");

    std::string project_root = PROJECT_ROOT;
    std::string dragon_model_path = project_root + "/dragon.obj";
    obj_data dragon = parse_obj(dragon_model_path);

    GLuint dragon_vao, dragon_vbo, dragon_ebo;
    glGenVertexArrays(1, &dragon_vao);
    glBindVertexArray(dragon_vao);

    glGenBuffers(1, &dragon_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, dragon_vbo);
    glBufferData(GL_ARRAY_BUFFER, dragon.vertices.size() * sizeof(dragon.vertices[0]), dragon.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &dragon_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dragon_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, dragon.indices.size() * sizeof(dragon.indices[0]), dragon.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void*)(12));

    auto rectangle_vertex_shader = create_shader(GL_VERTEX_SHADER, rectangle_vertex_shader_source);
    auto rectangle_fragment_shader = create_shader(GL_FRAGMENT_SHADER, rectangle_fragment_shader_source);
    auto rectangle_program = create_program(rectangle_vertex_shader, rectangle_fragment_shader);

    GLuint center_location = glGetUniformLocation(rectangle_program, "center");
    GLuint size_location = glGetUniformLocation(rectangle_program, "size");

    GLuint rectangle_vao;
    glGenVertexArrays(1, &rectangle_vao);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    float view_angle = 0.f;
    float camera_distance = 0.5f;
    float model_angle = glm::pi<float>() / 2.f;
    float model_scale = 1.f;

    bool running = true;
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
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            button_down[event.key.keysym.sym] = false;
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        if (button_down[SDLK_UP])
            camera_distance -= 1.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 1.f * dt;

        if (button_down[SDLK_LEFT])
            model_angle -= 2.f * dt;
        if (button_down[SDLK_RIGHT])
            model_angle += 2.f * dt;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        float near = 0.1f;
        float far = 100.f;

        glm::mat4 model(1.f);
        model = glm::rotate(model, model_angle, {0.f, 1.f, 0.f});
        model = glm::scale(model, glm::vec3(model_scale));

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});

        glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        glUseProgram(dragon_program);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));

        glUniform3fv(camera_position_location, 1, (float*)(&camera_position));

        glBindVertexArray(dragon_vao);
        glDrawElements(GL_TRIANGLES, dragon.indices.size(), GL_UNSIGNED_INT, nullptr);

        glUseProgram(rectangle_program);
        glUniform2f(center_location, -0.5f, -0.5f);
        glUniform2f(size_location, 0.5f, 0.5f);
        glBindVertexArray(rectangle_vao);
//        glDrawArrays(GL_TRIANGLES, 0, 6);

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
