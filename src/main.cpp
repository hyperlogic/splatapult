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
#include "util.h"
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
    auto positionVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, positionVec);
    auto uvVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, uvVec);
    auto colorVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colorVec);

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
    auto indexEBO = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, true);
#else
    auto indexEBO = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec);
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

    return pointCloud;
}

void Clear()
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
}

void RenderPointCloud(std::shared_ptr<const Program> pointProg, const std::shared_ptr<const Texture> pointTex,
                      std::shared_ptr<const PointCloud> pointCloud, std::shared_ptr<VertexArrayObject> pointCloudVAO,
                      const glm::mat4& cameraMat)
{

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 modelViewMat = glm::inverse(cameraMat);
    glm::mat4 projMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

    pointProg->Bind();
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
}

std::shared_ptr<VertexArrayObject> BuildSplatVAO(std::shared_ptr<const Program> splatProg)
{
    SDL_GL_MakeCurrent(window, gl_context);
    auto splatVAO = std::make_shared<VertexArrayObject>();

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    const float ASPECT_RATIO = (float)width / (float)height;

    // setup a rect in front of the camera.
    // these points are hardcoded in view space.
    std::vector<glm::vec3> viewPosVec;
    viewPosVec.reserve(4);
    const float DEPTH = -2.2f;
    viewPosVec.push_back(glm::vec3(0.5f, -0.5f, DEPTH));
    viewPosVec.push_back(glm::vec3(0.5f, 0.5f, DEPTH));
    viewPosVec.push_back(glm::vec3(-0.5f, 0.5f, DEPTH));
    viewPosVec.push_back(glm::vec3(-0.5f, -0.5f, DEPTH));

    /*
    std::vector<glm::vec3> colorVec;
    colorVec.reserve(4);
    colorVec.push_back(glm::vec4(1.0f, 1.0f, 1.0f
    */

    auto viewPosVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, viewPosVec);

    std::vector<uint32_t> indexVec = {0, 1, 2, 0, 2, 3};
    auto indexEBO = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec);

    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("viewPos"), viewPosVBO);
    splatVAO->SetElementBuffer(indexEBO);

    return splatVAO;
}

struct SplatInfo
{
    SplatInfo(float kIn, const glm::vec2& pIn, const glm::mat4& rhoInvMatIn) : k(kIn), p(pIn), rhoInvMat(rhoInvMatIn) {}
    float k;
    glm::vec2 p;
    glm::mat2 rhoInvMat;
};

float Gaussian2D(const glm::vec2& d, glm::mat2& V)
{
    float k = 1.0f / (2.0f * glm::pi<float>() * sqrtf(glm::determinant(V)));
    float e = glm::dot(d, glm::inverse(V) * d);
    return k * expf(-0.5f * e);
}

// Algorithm from Zwicker et. al 2001 "EWS Volume Splatting"
// u = center of Splat in obj coords
// V = varience matrix of spalt in object coords
SplatInfo ComputeSplatInfo(const glm::vec3& u, const glm::mat3& V, const glm::mat4& viewMat, const glm::mat4& projMat, const glm::vec4 viewport)
{
    // compute camera coords, t
    glm::vec4 t = viewMat * glm::vec4(u, 1.0f);

    // HACK: flip z (assume looking down z axis) not -z
    t.z = -t.z;

    // compute Jacobian J
    float l = glm::length(glm::vec3(t));
    glm::mat3 J(glm::vec3(1.0f / t.z, 0.0f, t.x / l),
                glm::vec3(0.0f, 1.0f / t.z, t.y / l),
                glm::vec3(-t.x / (t.z * t.z), -t.y / (t.z * t.z), t.z / l));

    // compute the variance matrix V_prime
    glm::mat3 W(viewMat);
    glm::mat3 JW = J * W;
    glm::mat3 V_prime = JW * V * glm::transpose(JW);

    // project t to screen coords, x
    glm::vec3 x = glm::project(u, viewMat, projMat, viewport);

    // setup the resampling filter rho[k]
    // AJT: TODO SKIP low-pass filter, part, I DON"T KNOW WHAT the variance should be on that.
    glm::mat2 V_hat_prime = glm::mat2(V_prime);
    float k1 = 1.0f / (glm::determinant(glm::inverse(J) * glm::inverse(W)));
    float k2 = 1.0f / (2.0f * glm::pi<float>() * sqrtf(glm::determinant(V_hat_prime)));

    // AJT: REMOVE, compute G at center of screen. viewport.z
    PrintVec(glm::vec2(k1, k2), "k1, k2");
    PrintMat(V_hat_prime, "V_hat_prime");
    PrintVec(u, "u");
    PrintVec(t, "t");
    PrintVec(x, "x");

    PrintMat(V, "V");
    PrintMat(W, "W");
    PrintMat(glm::inverse(W), "inv W");
    PrintMat(J, "J");
    PrintMat(glm::inverse(J), "inv J");
    PrintMat(V_prime, "V_prime");
    PrintMat(V_hat_prime, "V_hat_prime");
    PrintMat(glm::inverse(V_hat_prime), "inv V_hat_prime");

    glm::vec2 p(viewport.z / 2.0f, viewport.w / 2.0f);
    float g = k1 * Gaussian2D(p - glm::vec2(x), V_hat_prime);
    p += glm::vec2(1.0f, 1.0f);
    float g_offset = k1 * Gaussian2D(p - glm::vec2(x), V_hat_prime);

    Log::printf("g(center) = %.5f\n", g);
    Log::printf("g(center + offset) = %.5f\n", g_offset);

    return SplatInfo(k1 * k2, glm::vec2(x), glm::inverse(V_hat_prime));
}

void RenderSplat(std::shared_ptr<const Program> splatProg, std::shared_ptr<VertexArrayObject> splatVAO, const glm::mat4& cameraMat)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 viewMat = glm::inverse(cameraMat);
    glm::mat4 projMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

    glm::vec3 u(0.0f, 0.0f, 0.0f);
    glm::mat3 V(1.0f);

    glm::vec4 viewport(0.0f, 0.0f, (float)width, (float)height);
    SplatInfo splatInfo = ComputeSplatInfo(u, V, viewMat, projMat, viewport);

    splatProg->Bind();
    splatProg->SetUniform("projMat", projMat);

    // AJT: TODO These should be attribs.
    //splatProg->SetUniform("k", splatInfo.k);
    splatProg->SetUniform("p", splatInfo.p);
    //splatProg->SetUniform("rhoInvMat", splatInfo.rhoInvMat);

    // AJT: HACK MANUALY SET 2D guassian parameters
    float sigma = 100.0f;
    glm::mat2 V2(glm::vec2(1.0f / (sigma * sigma), 0.0f),
                 glm::vec2(0.0f, 1.0f / (sigma * sigma)));

    PrintMat(V2, "V2");
    //float k = 1.0f / (2.0f * glm::pi<float>() * sqrtf(glm::determinant(V2)));
    float k = 1.0f;
    splatProg->SetUniform("k", k);
    //splatProg->SetUniform("p", glm::vec2((float)width / 2.0f, (float)height / 2.0f));
    splatProg->SetUniform("rhoInvMat", V2);


    splatVAO->Draw();
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
        Log::printf("Error loading point shaders!\n");
        return 1;
    }

    // build static VertexArrayObject from the pointCloud
    auto pointCloudVAO = BuildPointCloudVAO(pointCloud, pointProg);

    auto splatProg = std::make_shared<Program>();
    if (!splatProg->Load("shader/splat_vert.glsl", "shader/splat_frag.glsl"))
    {
        Log::printf("Error loading splat shaders!\n");
        return 1;
    }
    auto splatVAO = BuildSplatVAO(splatProg);

    const float MOVE_SPEED = 2.5f;
    const float ROT_SPEED = 1.0f;
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

        Clear();
        RenderPointCloud(pointProg, pointTex, pointCloud, pointCloudVAO, flyCam.GetCameraMat());
        RenderSplat(splatProg, splatVAO, flyCam.GetCameraMat());
        frameCount++;

        SDL_GL_SwapWindow(window);

    }

    JOYSTICK_Shutdown();
    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
