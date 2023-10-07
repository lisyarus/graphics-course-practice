#ifndef GLHELPERS_HPP
#define GLHELPERS_HPP


#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <vector>
#include <string_view>
#include <string>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <map>

std::string to_string(std::string_view str);
void sdl2_fail(std::string_view message);
void glew_fail(std::string_view message, GLenum error);

void initialize_backend();

#endif