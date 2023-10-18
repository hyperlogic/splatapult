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
#include <tracy/Tracy.hpp>

#include "cameras.h"
#include "debugdraw.h"
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

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

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

void Resize(int newWidth, int newHeight)
{
    SDL_GL_MakeCurrent(window, gl_context);

    glViewport(0, 0, newWidth, newHeight);
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

    if (!pointCloud->ImportPly("data/train/input.ply"))
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

void RenderPointCloud(std::shared_ptr<const Program> pointProg, const std::shared_ptr<const Texture> pointTex,
                      std::shared_ptr<const PointCloud> pointCloud, std::shared_ptr<VertexArrayObject> pointCloudVAO,
                      const glm::mat4& cameraMat)
{

    ZoneScoped;

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
    std::vector<float> depthVec;
    const size_t numPoints = pointCloud->GetPointVec().size();
    {
        ZoneScopedNC("xform", tracy::Color::Red4);

        depthVec.reserve(numPoints);

        // transform forward vector into world space
        glm::vec3 forward = glm::mat3(cameraMat) * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 eye = glm::vec3(cameraMat[3]);

        // transform and copy points into view space.
        for (size_t i = 0; i < numPoints; i++)
        {
            glm::vec3 pos = glm::vec3(pointCloud->GetPointVec()[i].position[0],
                                      pointCloud->GetPointVec()[i].position[1],
                                      pointCloud->GetPointVec()[i].position[2]);
            depthVec.push_back(glm::dot(pos - eye, forward));
        }
    }

    // sort indices by z.
    std::vector<uint32_t> indexVec;
    {
        ZoneScopedNC("build indexVec", tracy::Color::DarkGray);

        indexVec.reserve(numPoints);
        for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
        {
            indexVec.push_back(i);
        }
    }

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        std::sort(indexVec.begin(), indexVec.end(), [&depthVec] (uint32_t a, uint32_t b)
        {
            return depthVec[a] > depthVec[b];
        });
    }

    // expand out indexVec for rendering
    const int NUM_CORNERS = 4;
    std::vector<uint32_t> newIndexVec;
    {
        ZoneScopedNC("expand", tracy::Color::DarkGray);

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
    }

    // store newIndexVec into elementBuffer
    {
        ZoneScopedNC("glBufferSubData", tracy::Color::DarkGray);

        auto elementBuffer = pointCloudVAO->GetElementBuffer();
        elementBuffer->Update(newIndexVec);
    }
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
        colorVec.push_back(color); colorVec.push_back(color); colorVec.push_back(color); colorVec.push_back(color);

        glm::mat3 V = g.ComputeCovMat();

        //glm::vec3 S(0.001f, 0.001f, 0.0001f);
        //glm::mat3 V(glm::vec3(S.x, 0.0f, 0.0f), glm::vec3(0.0f, S.y, 0.0f), glm::vec3(0.0f, 0.0f, S.z));

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

    if (!gaussianCloud->ImportPly("data/train/point_cloud.ply"))
    {
        Log::printf("Error loading GaussianCloud!\n");
        return nullptr;
    }

    /*
    //
    // make an example GaussianClound, that contain red, green and blue axes.
    //
    std::vector<GaussianCloud::Gaussian>& gaussianVec = gaussianCloud->GetGaussianVec();
    const float AXIS_LENGTH = 1.0f;
    const int NUM_SPLATS = 10;
    const float DELTA = (AXIS_LENGTH / (float)NUM_SPLATS);
    gaussianVec.resize(NUM_SPLATS * 3 + 1);
    const float S = logf(0.05f);
    const float SH_C0 = 0.28209479177387814f;
    const float SH_ONE = 1.0f / (2.0f * SH_C0);
    const float SH_ZERO = -1.0f / (2.0f * SH_C0);
    // x axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian& g = gaussianVec[i];
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = i * DELTA + DELTA;
        g.position[1] = 0.0f;
        g.position[2] = 0.0f;
        // red
        g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    }
    // y axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian& g = gaussianVec[i + NUM_SPLATS];
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = i * DELTA + DELTA;
        g.position[2] = 0.0f;
        // green
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    }
    // z axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian& g = gaussianVec[i + 2 * NUM_SPLATS];
        g.position[0] = 0.0f;
        g.position[1] = 0.0f;
        g.position[2] = i * DELTA + DELTA;
        // blue
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ONE;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    }
    GaussianCloud::Gaussian& g = gaussianVec[3 * NUM_SPLATS];
    g.position[0] = 0.0f;
    g.position[1] = 0.0f;
    g.position[2] = 0.0f;
    // white
    g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ONE;
    g.opacity = 100.0f;
    g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
    g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    */

    return gaussianCloud;
}

void RenderSplats(std::shared_ptr<const Program> splatProg, std::shared_ptr<const GaussianCloud> gaussianCloud,
                  std::shared_ptr<VertexArrayObject> splatVAO, const glm::mat4& cameraMat)
{
    ZoneScoped;

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
    std::vector<float> depthVec;
    const size_t numPoints = gaussianCloud->GetGaussianVec().size();
    {
        ZoneScopedNC("xform", tracy::Color::Red4);

        depthVec.reserve(numPoints);

        // transform forward vector into world space
        glm::vec3 forward = glm::mat3(cameraMat) * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 eye = glm::vec3(cameraMat[3]);

        // transform and copy points into view space.
        for (size_t i = 0; i < numPoints; i++)
        {
            glm::vec3 pos = glm::vec3(gaussianCloud->GetGaussianVec()[i].position[0],
                                      gaussianCloud->GetGaussianVec()[i].position[1],
                                      gaussianCloud->GetGaussianVec()[i].position[2]);
            float depth = glm::dot(pos - eye, forward);
            depthVec.push_back(depth);
        }
    }

    // sort indices by z.
    std::vector<uint32_t> indexVec;
    {
        ZoneScopedNC("build indexVec", tracy::Color::DarkGray);

        indexVec.reserve(numPoints);
        for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
        {
            indexVec.push_back(i);
        }
    }

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        std::sort(indexVec.begin(), indexVec.end(), [&depthVec] (uint32_t a, uint32_t b)
        {
            return depthVec[a] > depthVec[b];
        });
    }

    // expand out indexVec for rendering
    const int NUM_CORNERS = 4;
    std::vector<uint32_t> newIndexVec;
    {
        ZoneScopedNC("expand", tracy::Color::DarkGray);

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
    }

    // store newIndexVec into elementBuffer
    {
        ZoneScopedNC("glBufferSubData", tracy::Color::DarkGray);

        auto elementBuffer = splatVAO->GetElementBuffer();
        elementBuffer->Update(newIndexVec);
    }
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
    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

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

    SDL_GL_MakeCurrent(window, gl_context);
    if (!DebugDraw_Init())
    {
        Log::printf("DebugDraw Init failed\n");
        return 1;
    }

    SDL_AddEventWatch(Watch, NULL);

    auto cameras = std::make_shared<Cameras>();
    if (!cameras->ImportJson("data/train/cameras.json"))
    {
        Log::printf("Error loading cameras.json\n");
        return 1;
    }

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
    FlyCam flyCam(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);

    int cameraIndex = 0;
    flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);

    //FlyCam flyCam(glm::vec3(0.0f, 0.0f, 5.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
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
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    // The window was resized
                    int newWidth = event.window.data1;
                    int newHeight = event.window.data2;
                    // Handle the resize event, e.g., adjust your viewport, etc.
                    Resize(newWidth, newHeight);
                    SDL_RenderSetLogicalSize(renderer, newWidth, newHeight);
                }
                break;
            }
        }

        Joystick* joystick = JOYSTICK_GetJoystick();
        if (joystick)
        {
            float roll = 0.0f;
            roll -= (joystick->buttonStateFlags & (1 << Joystick::LeftBumper)) ? 1.0f : 0.0f;
            roll += (joystick->buttonStateFlags & (1 << Joystick::RightBumper)) ? 1.0f : 0.0f;
            flyCam.SetInput(glm::vec2(joystick->axes[Joystick::LeftStickX], joystick->axes[Joystick::LeftStickY]),
                            glm::vec2(joystick->axes[Joystick::RightStickX], joystick->axes[Joystick::RightStickY]), roll);
            flyCam.Process(dt);
        }

        Clear();

        int width, height;
        SDL_GetWindowSize(window, &width, &height);

        glm::mat4 cameraMat = flyCam.GetCameraMat();

        //RenderPointCloud(pointProg, pointTex, pointCloud, pointCloudVAO, cameraMat);
        RenderSplats(splatProg, gaussianCloud, splatVAO, cameraMat);

        //DebugDraw_Transform(cam);

        glm::mat4 modelViewMat = glm::inverse(cameraMat);
        glm::mat4 projMat = glm::perspective(FOVY, (float)width / (float)height, Z_NEAR, Z_FAR);
        DebugDraw_Render(projMat * modelViewMat);
        frameCount++;

        SDL_GL_SwapWindow(window);

        // cycle camera mat
        cameraIndex = (cameraIndex + 1) % cameras->GetCameraVec().size();
        flyCam.SetCameraMat(cameras->GetCameraVec()[cameraIndex]);

        FrameMark;
    }

    DebugDraw_Shutdown();
    JOYSTICK_Shutdown();
    SDL_DelEventWatch(Watch, NULL);
    SDL_GL_DeleteContext(gl_context);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
