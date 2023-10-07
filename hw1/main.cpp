#include "glhelpers/glhelpers.hpp"

#include "grid.h"
#include "camera.h"
#include <SDL2/SDL_keycode.h>

int main() try
{
	initialize_backend();

	SDL_Window * window = SDL_CreateWindow("Graphics course practice 4",
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

	glClearColor(0.2, 0.2, 0.2, 0.f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;
	float angle = 0.f;
	float zoom = 0.f;
	bool draw_isoline = false;

	std::map<SDL_Keycode, bool> button_down;

	bool running = true;

	Grid grid;
	Camera cam(width, height);
	cam.SetZoom(zoom);
	
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
				cam.UpdateTop(width, height);
				break;
			}
			break;
		case SDL_KEYDOWN:
			button_down[event.key.keysym.sym] = true;
			if (event.key.keysym.sym == SDLK_e) {
				draw_isoline = !draw_isoline;
				grid.DrawIsolines(draw_isoline);
			}
			if (event.key.keysym.sym == SDLK_q) {
				grid.IncreaseQuality();
			}
			if (event.key.keysym.sym == SDLK_a) {
				grid.DecreaseQuality();
			}
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

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (button_down[SDLK_LEFT]) {
			angle += dt;
			grid.SetAngle(angle);
		}
		if (button_down[SDLK_RIGHT]) {
			angle -= dt;
			grid.SetAngle(angle);
		}
		if (button_down[SDLK_UP]) {
			zoom += dt * 20;
			cam.SetZoom(zoom);
		}
		if (button_down[SDLK_DOWN]) {
			zoom -= dt * 20;
			cam.SetZoom(zoom);
		}
		
		grid.Render(time, cam);

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
