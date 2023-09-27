//SDL2 flashing random color example
//Should work on iOS/Android/Mac/Windows/Linux

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h> //rand()

#include "image.h"
#include "log.h"
#include "pointcloud.h"
#include "program.h"
#include "texture.h"

static bool quitting = false;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static SDL_Renderer *renderer = NULL;

void Render(const Program* pointProg, const Texture* pointTex, const PointCloud* pointCloud)
{
    SDL_GL_MakeCurrent(window, gl_context);
    glm::vec4 clearColor(0.0f, 0.4f, 0.0f, 1.0f);
    clearColor.r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 projMat = glm::ortho(0.0f, (float)width, 0.0f, (float)height, -10.0f, 10.0f);

    pointProg->Apply();
    pointProg->SetUniform("modelViewProjMat", projMat);
    pointProg->SetUniform("color", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    glm::vec2 xyLowerLeft(0.0f, (height - width) / 2.0f);
    glm::vec2 xyUpperRight((float)width, (height + width) / 2.0f);

    xyLowerLeft += 25.0f;
    xyUpperRight -= 25.0f;

    glm::vec2 uvLowerLeft(0.0f, 0.0f);
    glm::vec2 uvUpperRight(1.0f, 1.0f);
    glm::vec3 position[] = {glm::vec3(xyLowerLeft, 0.0f), glm::vec3(xyUpperRight.x, xyLowerLeft.y, 0.0f),
                            glm::vec3(xyUpperRight, 0.0f), glm::vec3(xyLowerLeft.x, xyUpperRight.y, 0.0f)};
    pointProg->SetAttrib("position", position);

    const size_t NUM_INDICES = 6;
    uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
    glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);

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
        Log::printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 512, SDL_WINDOW_OPENGL);

    gl_context = SDL_GL_CreateContext(window);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
    {
        Log::printf("Failed to SDL Renderer: %s\n", SDL_GetError());
		return 1;
	}

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::printf("Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    SDL_AddEventWatch(Watch, NULL);

    std::unique_ptr<PointCloud> pointCloud(new PointCloud());
    if (!pointCloud->ImportPly("data/input.ply"))
    {
        Log::printf("Error loading PointCloud!\n");
        return 1;
    }

    Image pointImg;
    if (!pointImg.Load("texture/sphere.png"))
    {
        Log::printf("Error loading sphere.png\n");
        return 1;
    }
    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::ClampToEdge, WrapType::ClampToEdge};
    std::unique_ptr<Texture> pointTex(new Texture(pointImg, texParams));
    std::unique_ptr<Program> pointProg(new Program());
    if (!pointProg->Load("shader/point_vert.glsl", "shader/point_frag.glsl"))
    {
        Log::printf("Error loading point shader!\n");
        return 1;
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

        Render(pointProg.get(), pointTex.get(), pointCloud.get());
        SDL_Delay(2);
    }

    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
