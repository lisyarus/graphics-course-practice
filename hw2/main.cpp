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

#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "tiny_obj_loader.h"
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

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texoord;

out vec3 position;
out vec3 normal;
out vec2 texcoord;

void main()
{
    position = (model * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = normalize(mat3(model) * in_normal);
    texcoord = vec2(in_texoord.x, 1 - in_texoord.y);
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core

uniform vec3 camera_position;

vec3 albedo;

uniform vec3 sun_direction;
uniform vec3 sun_color;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

uniform sampler2D sampler1;

uniform mat4 model;
uniform mat4 shadow_projection;

uniform sampler2DShadow sampler;

uniform int have_alpha;
uniform sampler2D alpha_sampler;

uniform float power;
uniform float glossiness;

uniform vec3 point_light_position;
uniform vec3 point_light_attenuation;
uniform vec3 point_light_color;

layout (location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction) {
    return albedo * max(0.0, dot(normal, direction));
}

vec3 specular(vec3 direction) {
    vec3 reflected_direction = 2.0 * normal * dot(normal, direction) - direction;
    vec3 view_direction = normalize(camera_position - position);
    return glossiness * albedo * pow(max(0.0, dot(reflected_direction, view_direction)), power);
}

vec3 phong(vec3 direction) {
    return diffuse(direction) + specular(direction);
}

void main()
{
    if(have_alpha == 1 && texture(alpha_sampler, texcoord).x < 0.5)
        discard;
    vec4 ndc = shadow_projection * model * vec4(position, 1.0);
    float ambient_light = 0.3;
    albedo = texture(sampler1, texcoord).xyz;
    vec3 color = albedo * ambient_light;

    vec3 to_point = normalize(point_light_position - position);
    float point_light_distance = distance(position, point_light_position);
    float factor = (point_light_attenuation.x + point_light_attenuation.y * point_light_distance + point_light_attenuation.z * point_light_distance * point_light_distance);
    vec3 point_light = phong(to_point) * point_light_color / factor;
    color += point_light;

    if(abs(ndc.x) < 1 && abs(ndc.y) < 1) {
        vec2 shadow_texcoord = ndc.xy * 0.5 + 0.5;
        float shadow_depth = ndc.z * 0.5 + 0.5;

        float sum = 0.0;
        float sum_w = 0.0;
        const int N = 7;
        float radius = 10.0;
        for (int x = -N; x <= N; ++x) {
            for (int y = -N; y <= N; ++y) {
                float c = exp(-float(x * x + y * y) / (radius*radius));
                sum += c * texture(sampler, vec3(shadow_texcoord + vec2(x,y) / vec2(textureSize(sampler, 0)), shadow_depth));
                sum_w += c;
            }
        }

       color += sun_color * phong(sun_direction) * sum / sum_w;
    } else {
        color += sun_color * phong(sun_direction);
    }
    out_color = vec4(color, 1.0);
}
)";

const char rectangle_vertex_shader_source[] =
        R"(#version 330 core
const vec2 VERTICES[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2(-0.5, -1.0),
    vec2(-1.0, -0.5),
    vec2(-0.5, -0.5),
    vec2(-1.0, -0.5),
    vec2(-0.5, -1.0)
);

out vec2 texcoord;

void main()
{
    texcoord = (VERTICES[gl_VertexID] + 1.0) * 2.0;
    gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);
}
)";

const char rectangle_fragment_shader_source[] =
        R"(#version 330 core

uniform sampler2D render_result;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(texture(render_result, texcoord).r);
}
)";

const char shadow_vertex_shader_source[] =
        R"(#version 330 core

layout (location = 0) in vec3 in_position;

uniform mat4 model;
uniform mat4 shadow_projection;

void main()
{
    gl_Position = shadow_projection * model * vec4(in_position, 1.0);
}
)";

const char shadow_fragment_shader_source[] =
        R"(#version 330 core
void main() {}
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

int main()
try {
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

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 8",
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

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto rectangle_vertex_shader = create_shader(GL_VERTEX_SHADER, rectangle_vertex_shader_source);
    auto rectangle_fragment_shader = create_shader(GL_FRAGMENT_SHADER, rectangle_fragment_shader_source);
    auto shadow_vertex_shader = create_shader(GL_VERTEX_SHADER, shadow_vertex_shader_source);
    auto shadow_fragment_shader = create_shader(GL_FRAGMENT_SHADER, shadow_fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);
    auto rectangle_program = create_program(rectangle_vertex_shader, rectangle_fragment_shader);
    auto shadow_program = create_program(shadow_vertex_shader, shadow_fragment_shader);

    GLuint model_location = glGetUniformLocation(program, "model");
    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint projection_location = glGetUniformLocation(program, "projection");
    GLuint shadow_projection_location0 = glGetUniformLocation(program, "shadow_projection");
    GLuint camera_position_location = glGetUniformLocation(program, "camera_position");
    GLuint sun_direction_location = glGetUniformLocation(program, "sun_direction");
    GLuint sun_color_location = glGetUniformLocation(program, "sun_color");
    GLuint sampler_location = glGetUniformLocation(program, "sampler1");
    GLuint have_alpha_location = glGetUniformLocation(program, "have_alpha");
    GLuint alpha_sampler_location = glGetUniformLocation(program, "alpha_sampler");
    GLuint glossiness_location = glGetUniformLocation(program, "glossiness");
    GLuint power_location = glGetUniformLocation(program, "power");
    GLuint point_light_attenuation = glGetUniformLocation(program, "point_light_attenuation");
    GLuint point_light_position = glGetUniformLocation(program, "point_light_position");
    GLuint light_color_location = glGetUniformLocation(program, "point_light_color");

    GLuint shadow_projection_location = glGetUniformLocation(shadow_program, "shadow_projection");
    GLuint shadow_model_location = glGetUniformLocation(shadow_program, "model");

    std::string project_root = PROJECT_ROOT;
    std::string obj_path = project_root + "/sponza/sponza.obj";
    std::string materials_dir = project_root + "/sponza/";

    tinyobj::ObjReader reader;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, obj_path.c_str(), materials_dir.c_str());

    obj_data scene;

    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                obj_data::vertex vvv;
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                vvv.position = {vx, vy, vz};
                if (idx.normal_index >= 0) {
                    tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    vvv.normal = {nx, ny, nz};
                }

                if (idx.texcoord_index >= 0) {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    vvv.texcoord = {tx, ty};
                }
                scene.vertices.push_back(vvv);
            }
            index_offset += fv;
            shapes[s].mesh.material_ids[f];
        }
    }

    std::map<std::string, GLuint> textures;
    auto load_texture = [&](std::string &name) {
        if (textures.contains(name)) return;
        if (name == "") return;
        glGenTextures(1, &textures[name]);
        glBindTexture(GL_TEXTURE_2D, textures[name]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        int texture_width, texture_height, texture_channels;
        std::string texture_path = materials_dir + name;
        std::replace(texture_path.begin(), texture_path.end(), '\\', '/');
        unsigned char *img = stbi_load(texture_path.c_str(), &texture_width,
                                       &texture_height,
                                       &texture_channels, 4);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(img);
    };
    for (auto material: materials) {
        load_texture(material.ambient_texname);
        load_texture(material.alpha_texname);
    }

    GLuint rectangle_vao;
    glGenVertexArrays(1, &rectangle_vao);
    glBindVertexArray(rectangle_vao);

    GLuint scene_vao, scene_vbo;
    glGenVertexArrays(1, &scene_vao);
    glBindVertexArray(scene_vao);

    glGenBuffers(1, &scene_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, scene_vbo);
    glBufferData(GL_ARRAY_BUFFER, scene.vertices.size() * sizeof(obj_data::vertex), scene.vertices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (24));

    int shadow_map_size = 4096;

    GLuint shadow_map_texture;
    glGenTextures(1, &shadow_map_texture);
    glBindTexture(GL_TEXTURE_2D, shadow_map_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_map_size, shadow_map_size, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    GLuint frame_buffer;

    glGenFramebuffers(1, &frame_buffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer);
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_texture, 0);

    assert(glCheckFramebufferStatus(frame_buffer) == GL_NO_ERROR);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;
    std::map<SDL_Keycode, bool> button_down;

    float camera_distance = 1.5f;
    float camera_angle = glm::pi<float>();

    std::vector<std::pair<int, int>> faces;

    for (auto shape: shapes) {
        int cur = 0;
        int id = 0;
        for (int i = 0; i < shape.mesh.material_ids.size(); i++) {
            if (shape.mesh.material_ids[i] == shape.mesh.material_ids[id] && id == 0) cur++;
            else {
                if (id == 0)
                    faces.emplace_back(shape.mesh.material_ids[id], 3 * cur);
                id = i;
            }
        }
        if (id == 0)
            faces.emplace_back(shape.mesh.material_ids[id], shape.mesh.indices.size());
        else
            faces.emplace_back(shape.mesh.material_ids[id], shape.mesh.indices.size() - 3 * cur);
    }

    auto draw_scene = [&](bool depth = false) {
        int first = 0;
        if (!depth) {
            glActiveTexture(GL_TEXTURE1);
            glUniform1i(sampler_location, 1);
        }

        for (auto face: faces) {
            int id = face.first;
            if (!depth) {
                if (textures.contains(materials[id].ambient_texname))
                    glBindTexture(GL_TEXTURE_2D, textures[materials[id].ambient_texname]);
                if (textures.contains(materials[id].alpha_texname)) {
                    glActiveTexture(GL_TEXTURE2);
                    glUniform1i(alpha_sampler_location, 2);
                    glUniform1i(have_alpha_location, 1);
                    glBindTexture(GL_TEXTURE_2D, textures[materials[id].alpha_texname]);
                    glActiveTexture(GL_TEXTURE1);
                } else {
                    glUniform1i(have_alpha_location, 0);
                }

                glUniform1f(glossiness_location, materials[id].specular[0]);
                glUniform1f(power_location, materials[id].shininess);
            }

            glDrawArrays(GL_TRIANGLES, first, face.second);

            first += face.second;
        }
        if (!depth)
            glActiveTexture(GL_TEXTURE0);
    };


    float z = 0;
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
            camera_distance -= 1000.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 1000.f * dt;

        if (button_down[SDLK_LEFT])
            camera_angle += 2.f * dt;
        if (button_down[SDLK_RIGHT])
            camera_angle -= 2.f * dt;

        if (button_down[SDLK_SPACE]) {
            time -= dt;
        }

        if (button_down[SDLK_t]) {
            z -= 10;
        }

        if (button_down[SDLK_g]) {
            z += 10;
        }

        float near = 1.f;
        float far = 5000.f;

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, glm::pi<float>() / 6.f, {1.f, 0.f, 0.f});
        view = glm::rotate(view, camera_angle, {0.f, 1.f, 0.f});
        view = glm::translate(view, {0.f, z, 0.f});

        float aspect = (float) height / (float) width;
        glm::mat4 projection = glm::perspective(glm::pi<float>() / 3.f, (width * 1.f) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        glm::vec3 sun_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f), 2.f, std::cos(time * 0.5f)));

        auto light_Z = -sun_direction;
        auto light_X = glm::normalize(glm::vec3(std::sin(time * 0.5f), -0.5f, std::cos(time * 0.5f)));
        auto light_Y = glm::cross(light_X, light_Z);
        glm::mat4 shadow_projection = glm::mat4(glm::transpose(glm::mat3(light_X, light_Y, light_Z))) * 0.0001f;
        shadow_projection[3][3] = 1.f;

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer);
        glViewport(0, 0, shadow_map_size, shadow_map_size);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_DEPTH_TEST);

        glUseProgram(shadow_program);

        glUniformMatrix4fv(shadow_model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(shadow_projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&shadow_projection));

        glBindVertexArray(scene_vao);
        draw_scene(true);

        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.8f, 0.8f, 1.f, 0.f);

        glEnable(GL_DEPTH_TEST);
        glCullFace(GL_BACK);

        glUseProgram(program);

        glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
        glUniformMatrix4fv(shadow_projection_location0, 1, GL_FALSE, reinterpret_cast<float *>(&shadow_projection));
        glUniform3fv(camera_position_location, 1, (float *) (&camera_position));
        glUniform3f(sun_color_location, 1.f, 1.f, 1.f);
        glUniform3fv(sun_direction_location, 1, reinterpret_cast<float *>(&sun_direction));
        glUniform3f(point_light_position, sin(time) * 300, 30, 250 * cos(time));
        glUniform3f(light_color_location, 1., 0.3, 0.);
        glUniform3f(point_light_attenuation, 1, 0.001, 0.0001);

        glBindVertexArray(scene_vao);
        draw_scene();

        glUseProgram(rectangle_program);
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(rectangle_vao);
        glBindTexture(GL_TEXTURE_2D, shadow_map_texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
