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
#include <unordered_map>

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

uniform mat4 transform;
uniform mat4 view;

const float a = 0.5;

const vec2 VERTICES[12] = vec2[12](
    vec2(-a, 0.0),                      // A
    vec2(-a / 2., sqrt(3) / 2 * a),     // B
    vec2(a / 2, sqrt(3) / 2 * a),       // C

    vec2(-a, 0.0),                      // A
    vec2(a, 0.0),                       // D
    vec2(a / 2, sqrt(3) / 2 * a),       // C

    vec2(-a, 0.0),                      // A
    vec2(a, 0.0),                       // D
    vec2(a / 2, -sqrt(3) / 2 * a),      // E

    vec2(-a / 2, -sqrt(3) / 2 * a),     // F
    vec2(a / 2, -sqrt(3) / 2 * a),      // E
    vec2(-a, 0.0)                       // A
);

const vec3 COLORS[12] = vec3[12](
    vec3(1.0, 0.0, 0.0),                // A
    vec3(0.0, 1.0, 0.0),                // B
    vec3(0.0, 0.0, 1.0),                // C

    vec3(1.0, 0.0, 0.0),                // A
    vec3(1.0, 0.0, 1.0),                // D
    vec3(0.0, 0.0, 1.0),                // C

    vec3(1.0, 0.0, 0.0),                // A
    vec3(1.0, 0.0, 1.0),                // D
    vec3(0.0, 1.0, 1.0),                // E

    vec3(1.0, 1.0, 0.0),                // F    
    vec3(0.0, 1.0, 1.0),                // E
    vec3(1.0, 0.0, 0.0)                 // A
);

out vec3 color;

void main()
{
    vec2 position = VERTICES[gl_VertexID];
    gl_Position = view * transform * vec4(position, 0.0, 1.0);
    color = COLORS[gl_VertexID];
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec3 color;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(color, 1.0);
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

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 2",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    SDL_GL_SetSwapInterval(0);

    GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = create_program(vertex_shader, fragment_shader);
    glUseProgram(program);

    float time = 0.0;
 
    float speed_x = 0.0;
    float speed_y = 0.0;

    float transform[16] = {
        cos(time), sin(time), 0, 0,
        -sin(time), cos(time), 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 1
    };

    float view[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    auto transform_uniform = glGetUniformLocation(program, "transform");
    glUniformMatrix4fv(transform_uniform, 1, GL_TRUE, transform);

    auto view_uniform = glGetUniformLocation(program, "view");
    glUniformMatrix4fv(view_uniform, 1, GL_TRUE, view);

    GLuint vao;
    glGenVertexArrays(1, &vao);

    std::unordered_map<SDL_Scancode, bool> key_down;

    auto last_frame_start = std::chrono::high_resolution_clock::now();

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
            key_down[event.key.keysym.scancode] = true;
            break;
        case SDL_KEYUP:
            key_down[event.key.keysym.scancode] = false;
            break;
        }

        if (key_down[SDL_SCANCODE_LEFT]) {
            speed_x = -1.0;
        } 

        if (key_down[SDL_SCANCODE_RIGHT]) {
            speed_x = 1.0;
        } 

        if (key_down[SDL_SCANCODE_UP]) {
            speed_y = 1.0;
        } 

        if (key_down[SDL_SCANCODE_DOWN]) {
            speed_y = -1.0;
        } 

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        // float dt = 0.016;
        last_frame_start = now;

        // time += dt;
        // transform[0] = cos(time) / 2;
		// transform[1] = sin(time) / 2;
		// transform[4] = -sin(time) / 2;
		// transform[5] = cos(time) / 2;

        // float x = sin(time) / 2;
		// float y = cos(time) / 2;
        // transform[3] = x;
		// transform[7] = y;

        float aspect_ratio = (float) width / height;
        view[0] = 1.0 / aspect_ratio;

        transform[3] += speed_x * dt;
		transform[7] += speed_y * dt;

        speed_x = 0.0;
        speed_y = 0.0;

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glUniformMatrix4fv(transform_uniform, 1, GL_TRUE, transform);
        glUniformMatrix4fv(view_uniform, 1, GL_TRUE, view);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 12);

        SDL_GL_SwapWindow(window);

        std::cout << "dt = " << dt << std::endl;
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
