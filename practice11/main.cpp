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
#include <random>
#include <map>
#include <cmath>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "obj_parser.hpp"
#include "stb_image.h"

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
        R"(#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in float in_size;
layout (location = 2) in float in_angle;

out float size;
out float angle;
void main()
{
    size = in_size;
    angle = in_angle;
    gl_Position = vec4(in_position, 1.0);
}
)";

const char geometry_shader_source[] =
        R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 camera_position;

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in float size[];
in float angle[];
out vec2 texcoord;
out float new_size;

vec3 rotate(vec3 v, vec3 z, float a) {
    return v * cos(a) + cross(z, v) * sin(a) + z * dot(z, v) * (1 - cos(a));
}

void main()
{
    vec3 center = gl_in[0].gl_Position.xyz;

    vec3 Z = normalize(camera_position - center);
    vec3 X = normalize(vec3(Z.z, 0, -Z.x));
    vec3 Y = normalize(cross(X, Z));

    X = rotate(X, Z, angle[0]);
    Y = rotate(Y, Z, angle[0]);

    gl_Position = projection * view * model * vec4(center + size[0] * (X + Y), 1.0);
    texcoord = vec2(1, 1);
    new_size = size[0];
    EmitVertex();

    gl_Position = projection * view * model * vec4(center + size[0] * (X - Y), 1.0);
    texcoord = vec2(1, 0);
    new_size = size[0];
    EmitVertex();

    gl_Position = projection * view * model * vec4(center + size[0] * (-X + Y), 1.0);
    texcoord = vec2(0, 1);
    new_size = size[0];
    EmitVertex();

    gl_Position = projection * view * model * vec4(center - size[0] * (X + Y), 1.0);
    texcoord = vec2(0, 0);
    new_size = size[0];
    EmitVertex();

    EndPrimitive();
}

)";

const char fragment_shader_source[] =
        R"(#version 330 core

layout (location = 0) out vec4 out_color;
in vec2 texcoord;

uniform sampler2D sampler;
uniform sampler1D sampler1d;

in float new_size;

void main()
{
    float alpha = (new_size - 0.2) / 0.2 * texture(sampler, texcoord).r;
    out_color = vec4(texture(sampler1d, alpha).rgb, alpha);
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

template<typename ... Shaders>
GLuint create_program(Shaders ... shaders) {
    GLuint result = glCreateProgram();
    (glAttachShader(result, shaders), ...);
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

struct particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float size;
    float angle;
    float angular_speed;
    float own_max_y;
};

GLuint load_texture(std::string const &path) {
    int width, height, channels;
    auto pixels = stbi_load(path.data(), &width, &height, &channels, 4);
    GLuint result;
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_2D, result);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    assert(glGetError() == 0);
    stbi_image_free(pixels);

    return result;
}

int main() try {
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

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 11",
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

    glClearColor(0.f, 0.f, 0.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto geometry_shader = create_shader(GL_GEOMETRY_SHADER, geometry_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, geometry_shader, fragment_shader);

    GLuint model_location = glGetUniformLocation(program, "model");
    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint projection_location = glGetUniformLocation(program, "projection");
    GLuint camera_position_location = glGetUniformLocation(program, "camera_position");
    GLuint sampler_location = glGetUniformLocation(program, "sampler");
    GLuint sampler1d_location = glGetUniformLocation(program, "sampler1d");

    std::default_random_engine rng;

    auto init_particle = [&](particle &p) {
        p.position.x = std::uniform_real_distribution<float>{-1.f, 1.f}(rng);
        p.position.y = 0.f;
        p.position.z = std::uniform_real_distribution<float>{-1.f, 1.f}(rng);
        p.size = std::uniform_real_distribution<float>{0.2, 0.4}(rng);
        p.velocity.x = std::uniform_real_distribution<float>{-0.2, 0.2}(rng);
        p.velocity.y = std::uniform_real_distribution<float>{0.3, 4}(rng);
        p.velocity.z = std::uniform_real_distribution<float>{-0.2, 0.2}(rng);
        p.angular_speed = std::uniform_real_distribution<float>{0.1, 1.0}(rng);
        p.own_max_y = std::uniform_real_distribution<float>{3, 7}(rng);
    };

    std::vector<particle> particles;

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(particle), (void *) (0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(particle), (void *) 24);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(particle), (void *) 28);

    const std::string project_root = PROJECT_ROOT;
    const std::string particle_texture_path = project_root + "/particle.png";

    GLuint albedo_texture = load_texture(particle_texture_path);

    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_1D, texture);
    GLubyte pixels[] = {
//            0, 0, 0,       // Black
            255, 0, 0,   // Red
            255, 165, 0,   // Orange
            255, 255, 0,   // Yellow
            255, 255, 255,  // White
    };


    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, (void *) pixels);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    assert(glGetError() == 0);

    glPointSize(5.f);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    float view_angle = 0.f;
    float camera_distance = 2.f;
    float camera_height = 0.5f;

    float camera_rotation = 0.f;

    bool paused = false;

    const float A = 0.2;
    const float C = 0.1;
    const float D = 0.001;

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
                            glViewport(0, 0, width, height);
                            break;
                    }
                    break;
                case SDL_KEYDOWN:
                    button_down[event.key.keysym.sym] = true;
                    if (event.key.keysym.sym == SDLK_SPACE)
                        paused = !paused;
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
            camera_distance -= 3.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 3.f * dt;

        if (button_down[SDLK_LEFT])
            camera_rotation -= 3.f * dt;
        if (button_down[SDLK_RIGHT])
            camera_rotation += 3.f * dt;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        float near = 0.1f;
        float far = 100.f;

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, -camera_height, -camera_distance});
        view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});

        glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();


        if (!paused) {
            for (auto &particle: particles) {
                particle.velocity.y += dt * A;
                particle.position += dt * particle.velocity;
                particle.velocity *= exp(-C * dt);
                particle.size *= exp(-D * dt);
                particle.angle += dt * particle.angular_speed;

                if (particle.position.y > particle.own_max_y) init_particle(particle);
            }

            if (particles.size() < 300) {
                particles.push_back({});
                init_particle(particles.back());
            }
        }


        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(particle), particles.data(), GL_STATIC_DRAW);

        glUseProgram(program);

        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniform3fv(camera_position_location, 1, reinterpret_cast<float *>(&camera_position));

        glUniform1i(sampler_location, 0);
        glUniform1i(sampler1d_location, 1);

        glBindVertexArray(vao);
        glBindTexture(GL_TEXTURE_2D, albedo_texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, texture);
        glActiveTexture(GL_TEXTURE0);

        glDrawArrays(GL_POINTS, 0, particles.size());
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
