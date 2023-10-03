//SDL2 flashing random color example
//Should work on iOS/Android/Mac/Windows/Linux

#include <algorithm>
#include <cassert>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <memory>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h> //rand()

#include "flycam.h"
#include "image.h"
#include "joystick.h"
#include "log.h"
#include "pointcloud.h"
#include "program.h"
#include "texture.h"
#include "vertexbuffer.h"

static bool quitting = false;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static SDL_Renderer *renderer = NULL;

//#define SORT_POINTS

std::shared_ptr<VertexArrayObject> BuildPointCloudVAO(std::shared_ptr<const PointCloud> pointCloud, std::shared_ptr<const Program> pointProg)
{
    SDL_GL_MakeCurrent(window, gl_context);
    auto pointCloudVAO = std::make_shared<VertexArrayObject>();

    auto positionVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER);
    auto uvVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER);
    auto colorVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER);
    auto indexEBO = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER);

    glm::vec2 uvLowerLeft(0.0f, 0.0f);
    glm::vec2 uvUpperRight(1.0f, 1.0f);

    // convert pointCloud into position array, where each position is repeated 4 times.
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
    positionVBO->Store(positionVec);
    uvVBO->Store(uvVec);
    colorVBO->Store(colorVec);

    std::vector<uint32_t> indexVec;
    const size_t NUM_INDICES = 6;
    indexVec.reserve(pointCloud->GetPointVec().size() * NUM_INDICES);
    assert(pointCloud->GetPointVec().size() * 6 <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)pointCloud->GetPointVec().size(); i++)
    {
        indexVec.push_back(i * NUM_CORNERS + 0);
        indexVec.push_back(i * NUM_CORNERS + 1);
        indexVec.push_back(i * NUM_CORNERS + 2);
        indexVec.push_back(i * NUM_CORNERS + 0);
        indexVec.push_back(i * NUM_CORNERS + 2);
        indexVec.push_back(i * NUM_CORNERS + 3);
    }
#ifdef SORT_POINTS
    indexEBO->Store(indexVec, true);
#else
    indexEBO->Store(indexVec);
#endif

    pointCloudVAO->SetAttribBuffer(pointProg->GetAttribLoc("position"), positionVBO);
    pointCloudVAO->SetAttribBuffer(pointProg->GetAttribLoc("uv"), uvVBO);
    pointCloudVAO->SetAttribBuffer(pointProg->GetAttribLoc("color"), colorVBO);
    pointCloudVAO->SetElementBuffer(indexEBO);

    return pointCloudVAO;
}

std::shared_ptr<PointCloud> LoadPointCloud()
{
    auto pointCloud = std::make_shared<PointCloud>();

    if (!pointCloud->ImportPly("data/input.ply"))
    {
        Log::printf("Error loading PointCloud!\n");
        return nullptr;
    }

    /*
    //
    // make an example pointVec, that contain three lines one for each axis.
    //
    std::vector<PointCloud::Point>& pointVec = pointCloud->GetPointVec();
    const float AXIS_LENGTH = 1.0f;
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
    */

    return pointCloud;
}

void Render(std::shared_ptr<const Program> pointProg, const std::shared_ptr<const Texture> pointTex, std::shared_ptr<const PointCloud> pointCloud,
            std::shared_ptr<VertexArrayObject> pointCloudVAO, const glm::mat4& cameraMat)
{
    SDL_GL_MakeCurrent(window, gl_context);

    // pre-multiplied alpha blending
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glm::vec4 clearColor(0.0f, 0.4f, 0.0f, 1.0f);
    //clearColor.r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // enable writes to depth buffer
    glEnable(GL_DEPTH_TEST);

    // enable alpha test
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.01f);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 modelViewMat = glm::inverse(cameraMat);
    glm::mat4 projMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

    pointProg->Apply();
    pointProg->SetUniform("modelViewMat", modelViewMat);
    pointProg->SetUniform("projMat", projMat);
    pointProg->SetUniform("pointSize", 0.02f);

    // use texture unit 0 for colorTexture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pointTex->texture);
    pointProg->SetUniform("colorTex", 0);

#ifdef SORT_POINTS
    // AJT: dont need to build this everyframe
    std::vector<glm::vec4> posVec;
    const size_t numPoints = pointCloud->GetPointVec().size();
    posVec.reserve(numPoints);
    // transform and copy points into view space.
    for (size_t i = 0; i < numPoints; i++)
    {
        posVec.push_back(modelViewMat * glm::vec4(pointCloud->GetPointVec()[i].position[0],
                                                  pointCloud->GetPointVec()[i].position[1],
                                                  pointCloud->GetPointVec()[i].position[2], 1.0f));
    }
    // sort indices by z.
    std::vector<uint32_t> indexVec;
    indexVec.reserve(numPoints);
    for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
    {
        indexVec.push_back(i);
    }
    std::sort(indexVec.begin(), indexVec.end(), [&posVec] (uint32_t a, uint32_t b)
    {
        return posVec[a].z < posVec[b].z;
    });
    // expand out indexVec for rendering
    const int NUM_CORNERS = 4;
    std::vector<uint32_t> newIndexVec;
    newIndexVec.reserve(numPoints * 4);
    for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
    {
        const int ii = indexVec[i];
        newIndexVec.push_back(ii * NUM_CORNERS + 0);
        newIndexVec.push_back(ii * NUM_CORNERS + 1);
        newIndexVec.push_back(ii * NUM_CORNERS + 2);
        newIndexVec.push_back(ii * NUM_CORNERS + 0);
        newIndexVec.push_back(ii * NUM_CORNERS + 2);
        newIndexVec.push_back(ii * NUM_CORNERS + 3);
    }

    // store newIndexVec into elementBuffer
    auto elementBuffer = pointCloudVAO->GetElementBuffer();
    elementBuffer->Update(newIndexVec);
#endif

    pointCloudVAO->Draw();

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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0)
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

    JOYSTICK_Init();

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::printf("Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    SDL_AddEventWatch(Watch, NULL);

    auto pointCloud = LoadPointCloud();
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
    auto pointTex = std::make_shared<Texture>(pointImg, texParams);
    auto pointProg = std::make_shared<Program>();
    if (!pointProg->Load("shader/point_vert.glsl", "shader/point_frag.glsl"))
    {
        Log::printf("Error loading point shader!\n");
        return 1;
    }

    // build static VertexArrayObject from the pointCloud
    auto pointCloudVAO = BuildPointCloudVAO(pointCloud, pointProg);

    const float MOVE_SPEED = 2.5f;
    const float ROT_SPEED = 0.25f;
    FlyCam flyCam(glm::vec3(0.0f, 0.0f, 10.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
    SDL_JoystickEventState(SDL_ENABLE);

    uint32_t frameCount = 1;
    uint32_t frameTicks = SDL_GetTicks();
    uint32_t lastTicks = SDL_GetTicks();
    while (!quitting)
    {
        // update dt
        uint32_t ticks = SDL_GetTicks();

        const int FPS_FRAMES = 100;
        if ((frameCount % FPS_FRAMES) == 0)
        {
            float delta = (ticks - frameTicks) / 1000.0f;
            float fps = (float)FPS_FRAMES / delta;
            frameTicks = ticks;
            Log::printf("fps = %.2f\n", fps);
        }
        float dt = (ticks - lastTicks) / 1000.0f;
        lastTicks = ticks;


        JOYSTICK_ClearFlags();

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
            case SDL_JOYAXISMOTION:
				JOYSTICK_UpdateMotion(&event.jaxis);
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				JOYSTICK_UpdateButton(&event.jbutton);
				break;
            }
        }

        Joystick* joystick = JOYSTICK_GetJoystick();
        if (joystick)
        {
            flyCam.SetInput(glm::vec2(joystick->axes[Joystick::LeftStickX], joystick->axes[Joystick::LeftStickY]),
                            glm::vec2(joystick->axes[Joystick::RightStickX], joystick->axes[Joystick::RightStickY]));
            flyCam.Process(dt);
        }
        Render(pointProg, pointTex, pointCloud, pointCloudVAO, flyCam.GetCameraMat());

        frameCount++;
    }

    JOYSTICK_Shutdown();
    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
