#include "shaderdebugrenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <tracy/Tracy.hpp>

#include "core/log.h"
#include "core/util.h"

// uniforms
glm::mat4 viewMat;  // used to project position into view coordinates.
glm::mat4 projMat;  // used to project view coordinates into clip coordinates.
glm::vec4 projParams;  // x = HEIGHT / tan(FOVY / 2), y = Z_NEAR, z = Z_FAR
glm::vec4 viewport;  // x, y, WIDTH, HEIGHT
glm::vec3 eye;

// vertex shader attribs
glm::vec4 position;  // center of the gaussian in object coordinates, (with alpha crammed in to w)
glm::vec4 r_sh0;  // sh coeff for red channel (up to third-order)
glm::vec4 g_sh0;  // sh coeff for green channel
glm::vec4 b_sh0;  // sh coeff for blue channel
glm::vec3 cov3_col0;
glm::vec3 cov3_col1;
glm::vec3 cov3_col2;

// vertex shader output
glm::vec4 geom_color;  // radiance of splat
glm::vec4 geom_cov2;  // 2D screen space covariance matrix of the gaussian
glm::vec2 geom_p;  // the 2D screen space center of the gaussian, (z is alpha)
glm::vec4 gl_Position;

// prototype for vertex shader
void debug_splat_vert();

// geom shader output
glm::vec4 frag_color;
glm::vec4 frag_cov2inv;
glm::vec2 frag_p;
glm::vec4 gl_Position2;

// prototype for geom shader
void debug_splat_geom();

void EmitVertex()
{
    PrintVec(gl_Position2, "    gl_Position2");
    PrintVec(frag_color, "    frag_color");
    PrintVec(frag_cov2inv, "    frag_cov2inv");
    PrintVec(frag_p, "    frag_p");
}

void EndPrimitive()
{
    Log::printf("    --------\n");
}

ShaderDebugRenderer::ShaderDebugRenderer(SDL_Renderer* sdlRendererIn) : SoftwareRenderer(sdlRendererIn)
{
}

ShaderDebugRenderer::~ShaderDebugRenderer()
{
}

bool ShaderDebugRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    size_t numPoints = gaussianCloud->size();
    posVec.reserve(numPoints);
    r_sh0Vec.reserve(numPoints);
    g_sh0Vec.reserve(numPoints);
    b_sh0Vec.reserve(numPoints);
    cov3_col0Vec.reserve(numPoints);
    cov3_col1Vec.reserve(numPoints);
    cov3_col2Vec.reserve(numPoints);

    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        // stick alpha into position.w
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        posVec.emplace_back(glm::vec4(g.position[0], g.position[1], g.position[2], alpha));

        r_sh0Vec.emplace_back(glm::vec4(g.f_dc[0], g.f_rest[0], g.f_rest[1], g.f_rest[2]));
        g_sh0Vec.emplace_back(glm::vec4(g.f_dc[1], g.f_rest[15], g.f_rest[16], g.f_rest[17]));
        b_sh0Vec.emplace_back(glm::vec4(g.f_dc[2], g.f_rest[30], g.f_rest[31], g.f_rest[32]));

#ifdef FULL_SH
        r_sh1Vec.emplace_back(glm::vec4(g.f_rest[3], g.f_rest[4], g.f_rest[5], g.f_rest[6]));
        r_sh2Vec.emplace_back(glm::vec4(g.f_rest[7], g.f_rest[8], g.f_rest[9], g.f_rest[10]));
        r_sh3Vec.emplace_back(glm::vec4(g.f_rest[11], g.f_rest[12], g.f_rest[13], g.f_rest[14]));
        g_sh1Vec.emplace_back(glm::vec4(g.f_rest[18], g.f_rest[19], g.f_rest[20], g.f_rest[21]));
        g_sh2Vec.emplace_back(glm::vec4(g.f_rest[22], g.f_rest[23], g.f_rest[24], g.f_rest[25]));
        g_sh3Vec.emplace_back(glm::vec4(g.f_rest[26], g.f_rest[27], g.f_rest[28], g.f_rest[29]));
        b_sh1Vec.emplace_back(glm::vec4(g.f_rest[33], g.f_rest[34], g.f_rest[35], g.f_rest[36]));
        b_sh2Vec.emplace_back(glm::vec4(g.f_rest[37], g.f_rest[38], g.f_rest[39], g.f_rest[40]));
        b_sh3Vec.emplace_back(glm::vec4(g.f_rest[41], g.f_rest[42], g.f_rest[43], g.f_rest[44]));
#endif

        glm::mat3 V = g.ComputeCovMat();
        cov3_col0Vec.push_back(V[0]);
        cov3_col1Vec.push_back(V[1]);
        cov3_col2Vec.push_back(V[2]);
    }
    return true;
}

// viewport = (x, y, width, height)
void ShaderDebugRenderer::RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
                                     const glm::vec4& viewport, const glm::vec2& nearFar)
{
    const int WIDTH = (int)viewport.z;
    const int HEIGHT = (int)viewport.w;

    SoftwareRenderer::RenderImpl(cameraMat, projMat, viewport, nearFar);

    float width = viewport.z;
    float height = viewport.w;
    float aspectRatio = width / height;
    glm::mat4 viewMat = glm::inverse(cameraMat);
    glm::vec3 eye = glm::vec3(cameraMat[3]);

    // set uniforms
    ::viewMat = viewMat;
    ::projMat = projMat;
    ::viewport = viewport;
    ::projParams = glm::vec4(0.0f, nearFar.x, nearFar.y, 0.0f);
    ::eye = eye;

    Log::printf("***************************************\n");
    PrintMat(::viewMat, "viewMat");
    PrintMat(::projMat, "projMat");
    PrintVec(::viewport, "viewport");
    PrintVec(::projParams, "projParams");
    PrintVec(::eye, "eye");

    for (size_t i = 0; i < posVec.size(); i++)
    {
        Log::printf("attribs[%d] =\n", (int)i);

        position = posVec[i];
        r_sh0 = r_sh0Vec[i];
        g_sh0 = g_sh0Vec[i];
        b_sh0 = b_sh0Vec[i];
        cov3_col0 = cov3_col0Vec[i];
        cov3_col1 = cov3_col1Vec[i];
        cov3_col2 = cov3_col2Vec[i];

        PrintVec(position, "    position");
        PrintVec(r_sh0, "    r_sh0");
        PrintVec(g_sh0, "    g_sh0");
        PrintVec(b_sh0, "    b_sh0");
        PrintVec(cov3_col0, "    cov3_col0");
        PrintVec(cov3_col1, "    cov3_col1");
        PrintVec(cov3_col2, "    cov3_col2");

        Log::printf("debug_splat_vert[%d] =\n", (int)i);

        debug_splat_vert();

        PrintVec(geom_color, "    geom_color");
        PrintVec(geom_cov2, "    geom_cov2");
        PrintVec(geom_p, "    geom_p");
        PrintVec(gl_Position, "    gl_Position");

        Log::printf("debug_splat_geom[%d] =\n", (int)i);
        debug_splat_geom();
    }
}
