//SDL2 flashing random color example
//Should work on iOS/Android/Mac/Windows/Linux

#include <cassert>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
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

std::unique_ptr<PointCloud> LoadPointCloud()
{
    std::unique_ptr<PointCloud> pointCloud(new PointCloud());
    /*
    if (!pointCloud->ImportPly("data/input.ply"))
    {
        Log::printf("Error loading PointCloud!\n");
        return nullptr;
    }
    */

    //
    // make an example pointVec, that contain three lines one for each axis.
    //
    std::vector<PointCloud::Point>& pointVec = pointCloud->GetPointVec();
    const float AXIS_LENGTH = 500.0f;
    const int NUM_POINTS = 10;
    const float DELTA = (AXIS_LENGTH / (float)NUM_POINTS);
    pointVec.resize(NUM_POINTS * 3);
    // x axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i];
        p.position[0] = i * DELTA;
        p.position[1] = 0.0f;
        p.position[2] = 0.0f;
        p.color[0] = 255;
        p.color[1] = 0;
        p.color[2] = 0;
    }
    // y axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i + NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = i * DELTA;
        p.position[2] = 0.0f;
        p.color[0] = 0;
        p.color[1] = 255;
        p.color[2] = 0;
    }
    // z axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i + 2 * NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = 0.0f;
        p.position[2] = i * DELTA;
        p.color[0] = 0;
        p.color[1] = 0;
        p.color[2] = 255;
    }

    return pointCloud;
}

void Render(const Program* pointProg, const Texture* pointTex, const PointCloud* pointCloud)
{
    SDL_GL_MakeCurrent(window, gl_context);

    // pre-multiplied alpha blending
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glm::vec4 clearColor(0.0f, 0.4f, 0.0f, 1.0f);
    clearColor.r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 modelViewMat(1.0f); // identity
    glm::mat4 projMat = glm::ortho(0.0f, (float)width, 0.0f, (float)height, -10.0f, 10.0f);

    pointProg->Apply();
    pointProg->SetUniform("modelViewMat", modelViewMat);
    pointProg->SetUniform("projMat", projMat);
    pointProg->SetUniform("pointSize", 10.0f);

    // use texture unit 0 for colorTexture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pointTex->texture);
    pointProg->SetUniform("colorTex", 0);

    glm::vec2 uvLowerLeft(0.0f, 0.0f);
    glm::vec2 uvUpperRight(1.0f, 1.0f);

    // convert pointCloud into position array, where each position is repeated 4 times.
    // TODO: use interleaved array directly into pointCloud->pointVec?
    const int NUM_CORNERS = 4;
    std::vector<glm::vec3> positionVec;
    positionVec.reserve(pointCloud->GetPointVec().size() * NUM_CORNERS);
    std::vector<glm::vec2> uvVec;
    uvVec.reserve(pointCloud->GetPointVec().size() * NUM_CORNERS);
    std::vector<glm::vec4> colorVec;
    for (auto&& p : pointCloud->GetPointVec())
    {
        positionVec.push_back(glm::vec3(p.position[0], p.position[1], p.position[2]));
        positionVec.push_back(glm::vec3(p.position[0], p.position[1], p.position[2]));
        positionVec.push_back(glm::vec3(p.position[0], p.position[1], p.position[2]));
        positionVec.push_back(glm::vec3(p.position[0], p.position[1], p.position[2]));
        uvVec.push_back(uvLowerLeft);
        uvVec.push_back(glm::vec2(uvUpperRight.x, uvLowerLeft.y));
        uvVec.push_back(uvUpperRight);
        uvVec.push_back(glm::vec2(uvLowerLeft.x, uvUpperRight.y));
        glm::vec4 color(p.color[0] / 255.0f, p.color[1] / 255.0f, p.color[2] / 255.0f, 1.0f);
        colorVec.push_back(color);
        colorVec.push_back(color);
        colorVec.push_back(color);
        colorVec.push_back(color);
    }
    pointProg->SetAttrib("position", positionVec.data());
    pointProg->SetAttrib("uv", uvVec.data());
    pointProg->SetAttrib("color", colorVec.data());

    /*
    glm::vec3 center(width / 2.0f, height / 2.0f, 0.0f);
    glm::vec3 position[] = {center, center, center, center};
    pointProg->SetAttrib("position", position);
    */

    /*
    glm::vec2 uvLowerLeft(0.0f, 0.0f);
    glm::vec2 uvUpperRight(1.0f, 1.0f);
    glm::vec2 uvs[] = {uvLowerLeft, glm::vec2(uvUpperRight.x, uvLowerLeft.y),
                       uvUpperRight, glm::vec2(uvLowerLeft.x, uvUpperRight.y)};
    pointProg->SetAttrib("uv", uvs);
    */

    /*

    uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
    glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);
    */
    std::vector<uint16_t> indexVec;
    const size_t NUM_INDICES = 6;
    indexVec.reserve(pointCloud->GetPointVec().size() * NUM_INDICES);
    assert(pointCloud->GetPointVec().size() * 6 <= std::numeric_limits<uint16_t>::max());
    for (uint16_t i = 0; i < (uint16_t)pointCloud->GetPointVec().size(); i++)
    {
        indexVec.push_back(i * NUM_CORNERS + 0);
        indexVec.push_back(i * NUM_CORNERS + 1);
        indexVec.push_back(i * NUM_CORNERS + 2);
        indexVec.push_back(i * NUM_CORNERS + 0);
        indexVec.push_back(i * NUM_CORNERS + 2);
        indexVec.push_back(i * NUM_CORNERS + 3);
    }
    glDrawElements(GL_TRIANGLES, (GLsizei)(NUM_INDICES * pointCloud->GetPointVec().size()), GL_UNSIGNED_SHORT, indexVec.data());

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

    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_OPENGL);

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
    pointCloud = LoadPointCloud();
    if (!pointCloud)
    {
        Log::printf("Error loading PointCloud\n");
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
