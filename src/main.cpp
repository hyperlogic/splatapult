// 3d gaussian splat renderer

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
#include "gaussiancloud.h"
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

const float Z_NEAR = 0.1f;
const float Z_FAR = 1000.0f;
const float FOVY = glm::radians(45.0f);

#define SORT_POINTS

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

void RenderPointCloud(std::shared_ptr<const Program> pointProg, const std::shared_ptr<const Texture> pointTex,
                      std::shared_ptr<const PointCloud> pointCloud, std::shared_ptr<VertexArrayObject> pointCloudVAO,
                      const glm::mat4& cameraMat)
{

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 modelViewMat = glm::inverse(cameraMat);
    glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);

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

std::shared_ptr<VertexArrayObject> BuildSplatVAO(std::shared_ptr<const GaussianCloud> gaussianCloud, std::shared_ptr<const Program> splatProg)
{
    SDL_GL_MakeCurrent(window, gl_context);
    auto splatVAO = std::make_shared<VertexArrayObject>();

    glm::vec2 uvLowerLeft(0.0f, 0.0f);
    glm::vec2 uvUpperRight(1.0f, 1.0f);

    // convert pointCloud into attribute arrays, where each splat will have 4 vertices.
    std::vector<glm::vec3> positionVec;
    std::vector<glm::vec2> uvVec;
    std::vector<glm::vec4> colorVec;
    std::vector<glm::vec3> cov3_col0Vec;
    std::vector<glm::vec3> cov3_col1Vec;
    std::vector<glm::vec3> cov3_col2Vec;

    const int NUM_CORNERS = 4;
    const int N = (int)gaussianCloud->GetGaussianVec().size() * NUM_CORNERS;

    positionVec.reserve(N);
    uvVec.reserve(N);
    colorVec.reserve(N);
    cov3_col0Vec.reserve(N);
    cov3_col1Vec.reserve(N);
    cov3_col2Vec.reserve(N);

    int i = 0;
    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        glm::vec3 pos(g.position[0], g.position[1], g.position[2]);
        positionVec.push_back(pos); positionVec.push_back(pos); positionVec.push_back(pos); positionVec.push_back(pos);

        uvVec.push_back(uvLowerLeft);
        uvVec.push_back(glm::vec2(uvUpperRight.x, uvLowerLeft.y));
        uvVec.push_back(uvUpperRight);
        uvVec.push_back(glm::vec2(uvLowerLeft.x, uvUpperRight.y));

        const float SH_C0 = 0.28209479177387814f;
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        glm::vec4 color(0.5f + SH_C0 * g.f_dc[0],
                        0.5f + SH_C0 * g.f_dc[1],
                        0.5f + SH_C0 * g.f_dc[2], alpha);
        //color = LinearToSRGB(color);
        colorVec.push_back(color); colorVec.push_back(color); colorVec.push_back(color); colorVec.push_back(color);

        // from paper V = R * S * transpose(S) * transpose(R);
        glm::quat rot(g.rot[0], g.rot[1], g.rot[2], g.rot[3]);
        glm::mat3 R(glm::normalize(rot));
        glm::mat3 S(glm::vec3(expf(g.scale[0]), 0.0f, 0.0f),
                    glm::vec3(0.0f, expf(g.scale[1]), 0.0f),
                    glm::vec3(0.0f, 0.0f, expf(g.scale[2])));
        glm::mat3 V = R * S * glm::transpose(S) * glm::transpose(R);

        /*
        float pointSize = 0.00001f;
        glm::mat3 V(glm::vec3(pointSize, 0.0f, 0.0f),
                    glm::vec3(0.0f, pointSize, 0.0f),
                    glm::vec3(0.0f, 0.0f, pointSize));
        */

        cov3_col0Vec.push_back(V[0]); cov3_col0Vec.push_back(V[0]); cov3_col0Vec.push_back(V[0]); cov3_col0Vec.push_back(V[0]);
        cov3_col1Vec.push_back(V[1]); cov3_col1Vec.push_back(V[1]); cov3_col1Vec.push_back(V[1]); cov3_col1Vec.push_back(V[1]);
        cov3_col2Vec.push_back(V[2]); cov3_col2Vec.push_back(V[2]); cov3_col2Vec.push_back(V[2]); cov3_col2Vec.push_back(V[2]);

        i++;
    }
    auto positionVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, positionVec);
    auto uvVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, uvVec);
    auto colorVBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colorVec);
    auto cov3_col0VBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col0Vec);
    auto cov3_col1VBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col1Vec);
    auto cov3_col2VBO = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col2Vec);

    std::vector<uint32_t> indexVec;
    const size_t NUM_INDICES = 6;
    indexVec.reserve(gaussianCloud->GetGaussianVec().size() * NUM_INDICES);
    assert(gaussianCloud->GetGaussianVec().size() * 6 <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)gaussianCloud->GetGaussianVec().size(); i++)
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

    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("position"), positionVBO);
    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("uv"), uvVBO);
    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("color"), colorVBO);
    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col0"), cov3_col0VBO);
    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col1"), cov3_col1VBO);
    splatVAO->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col2"), cov3_col2VBO);
    splatVAO->SetElementBuffer(indexEBO);

    return splatVAO;
}

std::shared_ptr<GaussianCloud> LoadGaussianCloud()
{
    auto gaussianCloud = std::make_shared<GaussianCloud>();
    if (!gaussianCloud->ImportPly("data/point_cloud.ply"))
    {
        Log::printf("Error loading GaussianCloud!\n");
        return nullptr;
    }

    return gaussianCloud;
}

void RenderSplats(std::shared_ptr<const Program> splatProg, std::shared_ptr<const GaussianCloud> gaussianCloud,
                  std::shared_ptr<VertexArrayObject> splatVAO, const glm::mat4& cameraMat)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glm::mat4 viewMat = glm::inverse(cameraMat);
    glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);

    splatProg->Bind();
    splatProg->SetUniform("viewMat", viewMat);
    splatProg->SetUniform("projMat", projMat);
    splatProg->SetUniform("projParams", glm::vec4((float)height / tanf(FOVY / 2.0f), Z_NEAR, Z_FAR, 0.0f));
    splatProg->SetUniform("viewport", glm::vec4(0.0f, 0.0f, (float)width, (float)height));

#ifdef SORT_POINTS
    // AJT: dont need to build this everyframe
    std::vector<glm::vec4> posVec;
    const size_t numPoints = gaussianCloud->GetGaussianVec().size();
    posVec.reserve(numPoints);
    // transform and copy points into view space.
    for (size_t i = 0; i < numPoints; i++)
    {
        posVec.push_back(viewMat * glm::vec4(gaussianCloud->GetGaussianVec()[i].position[0],
                                             gaussianCloud->GetGaussianVec()[i].position[1],
                                             gaussianCloud->GetGaussianVec()[i].position[2], 1.0f));
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
    auto elementBuffer = splatVAO->GetElementBuffer();
    elementBuffer->Update(newIndexVec);
#endif

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

    const int32_t WIDTH = 1024;
    const int32_t HEIGHT = 768;
    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);

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

    auto gaussianCloud = LoadGaussianCloud();

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

    auto splatVAO = BuildSplatVAO(gaussianCloud, splatProg);

    const float MOVE_SPEED = 2.5f;
    const float ROT_SPEED = 1.0f;


    /*
      [{"id": 0, "img_name": "00001", "width": 1959, "height": 1090,
  "position": [-3.0089893469241797, -0.11086489695181866, -3.7527640949141428],
  "rotation": [[0.876134201218856, 0.06925962026449776, 0.47706599800804744],
               [-0.04747421839895102, 0.9972110940209488, -0.057586739349882114],
               [-0.4797239414934443, 0.027805376500959853, 0.8769787916452908]],
  "fy": 1164.6601287484507, "fx": 1159.5880733038064},
    */
    // hack for train model
    glm::vec3 pos(-3.0089893469241797f, -0.11086489695181866f, -3.7527640949141428f);
    glm::mat3 rot(glm::vec3(0.876134201218856f, 0.06925962026449776f, 0.47706599800804744f),
                  glm::vec3(-0.04747421839895102, 0.9972110940209488, -0.057586739349882114),
                  glm::vec3(-0.4797239414934443, 0.027805376500959853, 0.8769787916452908));

    //rot = glm::transpose(rot);
    //FlyCam flyCam(glm::vec3(0.25f, 0.25f, 3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
    FlyCam flyCam(pos, glm::quat(0.0f, 0.0f, 1.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
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
        //RenderPointCloud(pointProg, pointTex, pointCloud, pointCloudVAO, flyCam.GetCameraMat());
        RenderSplats(splatProg, gaussianCloud, splatVAO, flyCam.GetCameraMat());
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
