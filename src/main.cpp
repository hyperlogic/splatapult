//SDL2 flashing random color example
//Should work on iOS/Android/Mac/Windows/Linux

#include <glm/glm.hpp>
#include <memory>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h> //rand()

#include "pointcloud.h"

static bool quitting = false;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static SDL_Renderer *renderer = NULL;

void Render()
{
    SDL_GL_MakeCurrent(window, gl_context);
    glm::vec4 clearColor(0.0f, 0.4f, 0.0f, 1.0f);
    clearColor.r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);

    SDL_GL_SwapWindow(window);
}

int SDLCALL Watch(void *userdata, SDL_Event* event)
{
    if (event->type == SDL_APP_WILLENTERBACKGROUND)
    {
        quitting = true;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0)
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 512, SDL_WINDOW_OPENGL);

    gl_context = SDL_GL_CreateContext(window);

    SDL_Log("Welcome! from 3dgstoy");

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
    {
        SDL_Log("Failed to SDL Renderer: %s", SDL_GetError());
		return 2;
	}

    SDL_AddEventWatch(Watch, NULL);

    std::unique_ptr<PointCloud> pointCloud(new PointCloud());
    if (!pointCloud->ImportPly("../../data/input.ply"))
    {
        SDL_Log("Error loading PointCloud!");
        return 3;
    }
    else
    {
        SDL_Log("Success!");
    }

    while (!quitting)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quitting = true;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // ESC to quit
                if (event.key.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    quitting = true;
                }
                break;
            }
        }

        Render();
        SDL_Delay(2);
    }

    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
