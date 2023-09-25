//SDL2 flashing random color example
//Should work on iOS/Android/Mac/Windows/Linux

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h> //rand()
#include <glm/glm.hpp>

static bool quitting = false;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static SDL_Renderer *renderer = NULL;

void render() {
    SDL_GL_MakeCurrent(window, gl_context);
    glm::vec4 clearColor(0.0f, 0.4f, 0.0f, 1.0f);
    clearColor.r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);

    SDL_GL_SwapWindow(window);
}


int SDLCALL watch(void *userdata, SDL_Event* event) {

    if (event->type == SDL_APP_WILLENTERBACKGROUND) {
        quitting = true;
    }

    return 1;
}

int main(int argc, char *argv[]) {

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("sdl2stub", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 512, SDL_WINDOW_OPENGL);

    gl_context = SDL_GL_CreateContext(window);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
        SDL_Log("Failed to SDL Renderer: %s", SDL_GetError());
		return -1;
	}

    SDL_AddEventWatch(watch, NULL);

    while (!quitting) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quitting = true;
            }
        }

        render();
        SDL_Delay(2);
    }

    SDL_DelEventWatch(watch, NULL);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
