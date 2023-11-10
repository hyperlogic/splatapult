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

struct GeomOut
{
    GeomOut(const glm::vec4& frag_colorIn, const glm::vec4& frag_cov2invIn, const glm::vec2& frag_pIn, const glm::vec4& gl_Position2In, const glm::vec2& fragPosIn) :
        frag_color(frag_colorIn), frag_cov2inv(frag_cov2invIn), frag_p(frag_pIn), gl_Position2(gl_Position2In), fragPos(fragPosIn) {}
    glm::vec4 frag_color;
    glm::vec4 frag_cov2inv;
    glm::vec2 frag_p;
    glm::vec4 gl_Position2;
    glm::vec2 fragPos;
};

// prototype for geom shader
void debug_splat_geom();

std::vector<GeomOut> geomOutVec;

void EmitVertex()
{
#ifdef DUMP
    PrintVec(gl_Position2, "    gl_Position2");
    PrintVec(frag_color, "    frag_color");
    PrintVec(frag_cov2inv, "    frag_cov2inv");
    PrintVec(frag_p, "    frag_p");
#endif

    glm::vec2 fragPos(gl_Position2.x / gl_Position2.w, gl_Position2.y / gl_Position2.w);
    float w = viewport.z;
    float h = viewport.w;
    fragPos.x = fragPos.x * w / 2.0f + w / 2.0f;
    fragPos.y = fragPos.y * h / 2.0f + h / 2.0f;
    geomOutVec.emplace_back(GeomOut(frag_color, frag_cov2inv, frag_p, gl_Position2, fragPos));
}

void EndPrimitive()
{
#ifdef DUMP
    Log::printf("    --------\n");
#endif
}

// frag shader input
glm::vec2 gl_FragCoord;

// frag shader output
glm::vec4 out_color;

void debug_splat_frag();

std::array<glm::vec3, 351> baryVec = {
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.96f, 0.0f, 0.04f),
    glm::vec3(0.92f, 0.0f, 0.08f),
    glm::vec3(0.88f, 0.0f, 0.12f),
    glm::vec3(0.84f, 0.0f, 0.16f),
    glm::vec3(0.8f, 0.0f, 0.2f),
    glm::vec3(0.76f, 0.0f, 0.24f),
    glm::vec3(0.72f, 0.0f, 0.28f),
    glm::vec3(0.6799999999999999f, 0.0f, 0.32f),
    glm::vec3(0.64f, 0.0f, 0.36f),
    glm::vec3(0.6f, 0.0f, 0.4f),
    glm::vec3(0.56f, 0.0f, 0.44f),
    glm::vec3(0.52f, 0.0f, 0.48f),
    glm::vec3(0.48f, 0.0f, 0.52f),
    glm::vec3(0.43999999999999995f, 0.0f, 0.56f),
    glm::vec3(0.4f, 0.0f, 0.6f),
    glm::vec3(0.36f, 0.0f, 0.64f),
    glm::vec3(0.31999999999999995f, 0.0f, 0.68f),
    glm::vec3(0.28f, 0.0f, 0.72f),
    glm::vec3(0.24f, 0.0f, 0.76f),
    glm::vec3(0.19999999999999996f, 0.0f, 0.8f),
    glm::vec3(0.16000000000000003f, 0.0f, 0.84f),
    glm::vec3(0.12f, 0.0f, 0.88f),
    glm::vec3(0.07999999999999996f, 0.0f, 0.92f),
    glm::vec3(0.040000000000000036f, 0.0f, 0.96f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(0.96f, 0.04f, 0.0f),
    glm::vec3(0.9199999999999999f, 0.04f, 0.04f),
    glm::vec3(0.88f, 0.04f, 0.08f),
    glm::vec3(0.84f, 0.04f, 0.12f),
    glm::vec3(0.7999999999999999f, 0.04f, 0.16f),
    glm::vec3(0.76f, 0.04f, 0.2f),
    glm::vec3(0.72f, 0.04f, 0.24f),
    glm::vec3(0.6799999999999999f, 0.04f, 0.28f),
    glm::vec3(0.6399999999999999f, 0.04f, 0.32f),
    glm::vec3(0.6f, 0.04f, 0.36f),
    glm::vec3(0.5599999999999999f, 0.04f, 0.4f),
    glm::vec3(0.52f, 0.04f, 0.44f),
    glm::vec3(0.48f, 0.04f, 0.48f),
    glm::vec3(0.43999999999999995f, 0.04f, 0.52f),
    glm::vec3(0.3999999999999999f, 0.04f, 0.56f),
    glm::vec3(0.36f, 0.04f, 0.6f),
    glm::vec3(0.31999999999999995f, 0.04f, 0.64f),
    glm::vec3(0.2799999999999999f, 0.04f, 0.68f),
    glm::vec3(0.24f, 0.04f, 0.72f),
    glm::vec3(0.19999999999999996f, 0.04f, 0.76f),
    glm::vec3(0.15999999999999992f, 0.04f, 0.8f),
    glm::vec3(0.12f, 0.04f, 0.84f),
    glm::vec3(0.07999999999999996f, 0.04f, 0.88f),
    glm::vec3(0.039999999999999925f, 0.04f, 0.92f),
    glm::vec3(0.0f, 0.04f, 0.96f),
    glm::vec3(0.92f, 0.08f, 0.0f),
    glm::vec3(0.88f, 0.08f, 0.04f),
    glm::vec3(0.8400000000000001f, 0.08f, 0.08f),
    glm::vec3(0.8f, 0.08f, 0.12f),
    glm::vec3(0.76f, 0.08f, 0.16f),
    glm::vec3(0.72f, 0.08f, 0.2f),
    glm::vec3(0.68f, 0.08f, 0.24f),
    glm::vec3(0.64f, 0.08f, 0.28f),
    glm::vec3(0.6000000000000001f, 0.08f, 0.32f),
    glm::vec3(0.56f, 0.08f, 0.36f),
    glm::vec3(0.52f, 0.08f, 0.4f),
    glm::vec3(0.48000000000000004f, 0.08f, 0.44f),
    glm::vec3(0.44000000000000006f, 0.08f, 0.48f),
    glm::vec3(0.4f, 0.08f, 0.52f),
    glm::vec3(0.36f, 0.08f, 0.56f),
    glm::vec3(0.32000000000000006f, 0.08f, 0.6f),
    glm::vec3(0.28f, 0.08f, 0.64f),
    glm::vec3(0.24f, 0.08f, 0.68f),
    glm::vec3(0.20000000000000007f, 0.08f, 0.72f),
    glm::vec3(0.16000000000000003f, 0.08f, 0.76f),
    glm::vec3(0.12f, 0.08f, 0.8f),
    glm::vec3(0.08000000000000007f, 0.08f, 0.84f),
    glm::vec3(0.040000000000000036f, 0.08f, 0.88f),
    glm::vec3(0.0f, 0.08f, 0.92f),
    glm::vec3(0.88f, 0.12f, 0.0f),
    glm::vec3(0.84f, 0.12f, 0.04f),
    glm::vec3(0.8f, 0.12f, 0.08f),
    glm::vec3(0.76f, 0.12f, 0.12f),
    glm::vec3(0.72f, 0.12f, 0.16f),
    glm::vec3(0.6799999999999999f, 0.12f, 0.2f),
    glm::vec3(0.64f, 0.12f, 0.24f),
    glm::vec3(0.6f, 0.12f, 0.28f),
    glm::vec3(0.56f, 0.12f, 0.32f),
    glm::vec3(0.52f, 0.12f, 0.36f),
    glm::vec3(0.48f, 0.12f, 0.4f),
    glm::vec3(0.44f, 0.12f, 0.44f),
    glm::vec3(0.4f, 0.12f, 0.48f),
    glm::vec3(0.36f, 0.12f, 0.52f),
    glm::vec3(0.31999999999999995f, 0.12f, 0.56f),
    glm::vec3(0.28f, 0.12f, 0.6f),
    glm::vec3(0.24f, 0.12f, 0.64f),
    glm::vec3(0.19999999999999996f, 0.12f, 0.68f),
    glm::vec3(0.16000000000000003f, 0.12f, 0.72f),
    glm::vec3(0.12f, 0.12f, 0.76f),
    glm::vec3(0.07999999999999996f, 0.12f, 0.8f),
    glm::vec3(0.040000000000000036f, 0.12f, 0.84f),
    glm::vec3(0.0f, 0.12f, 0.88f),
    glm::vec3(0.84f, 0.16f, 0.0f),
    glm::vec3(0.7999999999999999f, 0.16f, 0.04f),
    glm::vec3(0.76f, 0.16f, 0.08f),
    glm::vec3(0.72f, 0.16f, 0.12f),
    glm::vec3(0.6799999999999999f, 0.16f, 0.16f),
    glm::vec3(0.6399999999999999f, 0.16f, 0.2f),
    glm::vec3(0.6f, 0.16f, 0.24f),
    glm::vec3(0.5599999999999999f, 0.16f, 0.28f),
    glm::vec3(0.52f, 0.16f, 0.32f),
    glm::vec3(0.48f, 0.16f, 0.36f),
    glm::vec3(0.43999999999999995f, 0.16f, 0.4f),
    glm::vec3(0.39999999999999997f, 0.16f, 0.44f),
    glm::vec3(0.36f, 0.16f, 0.48f),
    glm::vec3(0.31999999999999995f, 0.16f, 0.52f),
    glm::vec3(0.2799999999999999f, 0.16f, 0.56f),
    glm::vec3(0.24f, 0.16f, 0.6f),
    glm::vec3(0.19999999999999996f, 0.16f, 0.64f),
    glm::vec3(0.15999999999999992f, 0.16f, 0.68f),
    glm::vec3(0.12f, 0.16f, 0.72f),
    glm::vec3(0.07999999999999996f, 0.16f, 0.76f),
    glm::vec3(0.039999999999999925f, 0.16f, 0.8f),
    glm::vec3(0.0f, 0.16f, 0.84f),
    glm::vec3(0.8f, 0.2f, 0.0f),
    glm::vec3(0.76f, 0.2f, 0.04f),
    glm::vec3(0.7200000000000001f, 0.2f, 0.08f),
    glm::vec3(0.68f, 0.2f, 0.12f),
    glm::vec3(0.64f, 0.2f, 0.16f),
    glm::vec3(0.6000000000000001f, 0.2f, 0.2f),
    glm::vec3(0.56f, 0.2f, 0.24f),
    glm::vec3(0.52f, 0.2f, 0.28f),
    glm::vec3(0.48000000000000004f, 0.2f, 0.32f),
    glm::vec3(0.44000000000000006f, 0.2f, 0.36f),
    glm::vec3(0.4f, 0.2f, 0.4f),
    glm::vec3(0.36000000000000004f, 0.2f, 0.44f),
    glm::vec3(0.32000000000000006f, 0.2f, 0.48f),
    glm::vec3(0.28f, 0.2f, 0.52f),
    glm::vec3(0.24f, 0.2f, 0.56f),
    glm::vec3(0.20000000000000007f, 0.2f, 0.6f),
    glm::vec3(0.16000000000000003f, 0.2f, 0.64f),
    glm::vec3(0.12f, 0.2f, 0.68f),
    glm::vec3(0.08000000000000007f, 0.2f, 0.72f),
    glm::vec3(0.040000000000000036f, 0.2f, 0.76f),
    glm::vec3(0.0f, 0.2f, 0.8f),
    glm::vec3(0.76f, 0.24f, 0.0f),
    glm::vec3(0.72f, 0.24f, 0.04f),
    glm::vec3(0.68f, 0.24f, 0.08f),
    glm::vec3(0.64f, 0.24f, 0.12f),
    glm::vec3(0.6f, 0.24f, 0.16f),
    glm::vec3(0.56f, 0.24f, 0.2f),
    glm::vec3(0.52f, 0.24f, 0.24f),
    glm::vec3(0.48f, 0.24f, 0.28f),
    glm::vec3(0.44f, 0.24f, 0.32f),
    glm::vec3(0.4f, 0.24f, 0.36f),
    glm::vec3(0.36f, 0.24f, 0.4f),
    glm::vec3(0.32f, 0.24f, 0.44f),
    glm::vec3(0.28f, 0.24f, 0.48f),
    glm::vec3(0.24f, 0.24f, 0.52f),
    glm::vec3(0.19999999999999996f, 0.24f, 0.56f),
    glm::vec3(0.16000000000000003f, 0.24f, 0.6f),
    glm::vec3(0.12f, 0.24f, 0.64f),
    glm::vec3(0.07999999999999996f, 0.24f, 0.68f),
    glm::vec3(0.040000000000000036f, 0.24f, 0.72f),
    glm::vec3(0.0f, 0.24f, 0.76f),
    glm::vec3(0.72f, 0.28f, 0.0f),
    glm::vec3(0.6799999999999999f, 0.28f, 0.04f),
    glm::vec3(0.64f, 0.28f, 0.08f),
    glm::vec3(0.6f, 0.28f, 0.12f),
    glm::vec3(0.5599999999999999f, 0.28f, 0.16f),
    glm::vec3(0.52f, 0.28f, 0.2f),
    glm::vec3(0.48f, 0.28f, 0.24f),
    glm::vec3(0.43999999999999995f, 0.28f, 0.28f),
    glm::vec3(0.39999999999999997f, 0.28f, 0.32f),
    glm::vec3(0.36f, 0.28f, 0.36f),
    glm::vec3(0.31999999999999995f, 0.28f, 0.4f),
    glm::vec3(0.27999999999999997f, 0.28f, 0.44f),
    glm::vec3(0.24f, 0.28f, 0.48f),
    glm::vec3(0.19999999999999996f, 0.28f, 0.52f),
    glm::vec3(0.15999999999999992f, 0.28f, 0.56f),
    glm::vec3(0.12f, 0.28f, 0.6f),
    glm::vec3(0.07999999999999996f, 0.28f, 0.64f),
    glm::vec3(0.039999999999999925f, 0.28f, 0.68f),
    glm::vec3(0.0f, 0.28f, 0.72f),
    glm::vec3(0.6799999999999999f, 0.32f, 0.0f),
    glm::vec3(0.6399999999999999f, 0.32f, 0.04f),
    glm::vec3(0.6f, 0.32f, 0.08f),
    glm::vec3(0.5599999999999999f, 0.32f, 0.12f),
    glm::vec3(0.5199999999999999f, 0.32f, 0.16f),
    glm::vec3(0.4799999999999999f, 0.32f, 0.2f),
    glm::vec3(0.43999999999999995f, 0.32f, 0.24f),
    glm::vec3(0.3999999999999999f, 0.32f, 0.28f),
    glm::vec3(0.35999999999999993f, 0.32f, 0.32f),
    glm::vec3(0.31999999999999995f, 0.32f, 0.36f),
    glm::vec3(0.2799999999999999f, 0.32f, 0.4f),
    glm::vec3(0.23999999999999994f, 0.32f, 0.44f),
    glm::vec3(0.19999999999999996f, 0.32f, 0.48f),
    glm::vec3(0.15999999999999992f, 0.32f, 0.52f),
    glm::vec3(0.11999999999999988f, 0.32f, 0.56f),
    glm::vec3(0.07999999999999996f, 0.32f, 0.6f),
    glm::vec3(0.039999999999999925f, 0.32f, 0.64f),
    glm::vec3(-1.1102230246251565e-16f, 0.32f, 0.68f),
    glm::vec3(0.64f, 0.36f, 0.0f),
    glm::vec3(0.6f, 0.36f, 0.04f),
    glm::vec3(0.56f, 0.36f, 0.08f),
    glm::vec3(0.52f, 0.36f, 0.12f),
    glm::vec3(0.48f, 0.36f, 0.16f),
    glm::vec3(0.44f, 0.36f, 0.2f),
    glm::vec3(0.4f, 0.36f, 0.24f),
    glm::vec3(0.36f, 0.36f, 0.28f),
    glm::vec3(0.32f, 0.36f, 0.32f),
    glm::vec3(0.28f, 0.36f, 0.36f),
    glm::vec3(0.24f, 0.36f, 0.4f),
    glm::vec3(0.2f, 0.36f, 0.44f),
    glm::vec3(0.16000000000000003f, 0.36f, 0.48f),
    glm::vec3(0.12f, 0.36f, 0.52f),
    glm::vec3(0.07999999999999996f, 0.36f, 0.56f),
    glm::vec3(0.040000000000000036f, 0.36f, 0.6f),
    glm::vec3(0.0f, 0.36f, 0.64f),
    glm::vec3(0.6f, 0.4f, 0.0f),
    glm::vec3(0.5599999999999999f, 0.4f, 0.04f),
    glm::vec3(0.52f, 0.4f, 0.08f),
    glm::vec3(0.48f, 0.4f, 0.12f),
    glm::vec3(0.43999999999999995f, 0.4f, 0.16f),
    glm::vec3(0.39999999999999997f, 0.4f, 0.2f),
    glm::vec3(0.36f, 0.4f, 0.24f),
    glm::vec3(0.31999999999999995f, 0.4f, 0.28f),
    glm::vec3(0.27999999999999997f, 0.4f, 0.32f),
    glm::vec3(0.24f, 0.4f, 0.36f),
    glm::vec3(0.19999999999999996f, 0.4f, 0.4f),
    glm::vec3(0.15999999999999998f, 0.4f, 0.44f),
    glm::vec3(0.12f, 0.4f, 0.48f),
    glm::vec3(0.07999999999999996f, 0.4f, 0.52f),
    glm::vec3(0.039999999999999925f, 0.4f, 0.56f),
    glm::vec3(0.0f, 0.4f, 0.6f),
    glm::vec3(0.56f, 0.44f, 0.0f),
    glm::vec3(0.52f, 0.44f, 0.04f),
    glm::vec3(0.48000000000000004f, 0.44f, 0.08f),
    glm::vec3(0.44000000000000006f, 0.44f, 0.12f),
    glm::vec3(0.4f, 0.44f, 0.16f),
    glm::vec3(0.36000000000000004f, 0.44f, 0.2f),
    glm::vec3(0.32000000000000006f, 0.44f, 0.24f),
    glm::vec3(0.28f, 0.44f, 0.28f),
    glm::vec3(0.24000000000000005f, 0.44f, 0.32f),
    glm::vec3(0.20000000000000007f, 0.44f, 0.36f),
    glm::vec3(0.16000000000000003f, 0.44f, 0.4f),
    glm::vec3(0.12000000000000005f, 0.44f, 0.44f),
    glm::vec3(0.08000000000000007f, 0.44f, 0.48f),
    glm::vec3(0.040000000000000036f, 0.44f, 0.52f),
    glm::vec3(0.0f, 0.44f, 0.56f),
    glm::vec3(0.52f, 0.48f, 0.0f),
    glm::vec3(0.48000000000000004f, 0.48f, 0.04f),
    glm::vec3(0.44f, 0.48f, 0.08f),
    glm::vec3(0.4f, 0.48f, 0.12f),
    glm::vec3(0.36f, 0.48f, 0.16f),
    glm::vec3(0.32f, 0.48f, 0.2f),
    glm::vec3(0.28f, 0.48f, 0.24f),
    glm::vec3(0.24f, 0.48f, 0.28f),
    glm::vec3(0.2f, 0.48f, 0.32f),
    glm::vec3(0.16000000000000003f, 0.48f, 0.36f),
    glm::vec3(0.12f, 0.48f, 0.4f),
    glm::vec3(0.08000000000000002f, 0.48f, 0.44f),
    glm::vec3(0.040000000000000036f, 0.48f, 0.48f),
    glm::vec3(0.0f, 0.48f, 0.52f),
    glm::vec3(0.48f, 0.52f, 0.0f),
    glm::vec3(0.44f, 0.52f, 0.04f),
    glm::vec3(0.39999999999999997f, 0.52f, 0.08f),
    glm::vec3(0.36f, 0.52f, 0.12f),
    glm::vec3(0.31999999999999995f, 0.52f, 0.16f),
    glm::vec3(0.27999999999999997f, 0.52f, 0.2f),
    glm::vec3(0.24f, 0.52f, 0.24f),
    glm::vec3(0.19999999999999996f, 0.52f, 0.28f),
    glm::vec3(0.15999999999999998f, 0.52f, 0.32f),
    glm::vec3(0.12f, 0.52f, 0.36f),
    glm::vec3(0.07999999999999996f, 0.52f, 0.4f),
    glm::vec3(0.03999999999999998f, 0.52f, 0.44f),
    glm::vec3(0.0f, 0.52f, 0.48f),
    glm::vec3(0.43999999999999995f, 0.56f, 0.0f),
    glm::vec3(0.39999999999999997f, 0.56f, 0.04f),
    glm::vec3(0.35999999999999993f, 0.56f, 0.08f),
    glm::vec3(0.31999999999999995f, 0.56f, 0.12f),
    glm::vec3(0.2799999999999999f, 0.56f, 0.16f),
    glm::vec3(0.23999999999999994f, 0.56f, 0.2f),
    glm::vec3(0.19999999999999996f, 0.56f, 0.24f),
    glm::vec3(0.15999999999999992f, 0.56f, 0.28f),
    glm::vec3(0.11999999999999994f, 0.56f, 0.32f),
    glm::vec3(0.07999999999999996f, 0.56f, 0.36f),
    glm::vec3(0.039999999999999925f, 0.56f, 0.4f),
    glm::vec3(-5.551115123125783e-17f, 0.56f, 0.44f),
    glm::vec3(0.4f, 0.6f, 0.0f),
    glm::vec3(0.36000000000000004f, 0.6f, 0.04f),
    glm::vec3(0.32f, 0.6f, 0.08f),
    glm::vec3(0.28f, 0.6f, 0.12f),
    glm::vec3(0.24000000000000002f, 0.6f, 0.16f),
    glm::vec3(0.2f, 0.6f, 0.2f),
    glm::vec3(0.16000000000000003f, 0.6f, 0.24f),
    glm::vec3(0.12f, 0.6f, 0.28f),
    glm::vec3(0.08000000000000002f, 0.6f, 0.32f),
    glm::vec3(0.040000000000000036f, 0.6f, 0.36f),
    glm::vec3(0.0f, 0.6f, 0.4f),
    glm::vec3(0.36f, 0.64f, 0.0f),
    glm::vec3(0.32f, 0.64f, 0.04f),
    glm::vec3(0.27999999999999997f, 0.64f, 0.08f),
    glm::vec3(0.24f, 0.64f, 0.12f),
    glm::vec3(0.19999999999999998f, 0.64f, 0.16f),
    glm::vec3(0.15999999999999998f, 0.64f, 0.2f),
    glm::vec3(0.12f, 0.64f, 0.24f),
    glm::vec3(0.07999999999999996f, 0.64f, 0.28f),
    glm::vec3(0.03999999999999998f, 0.64f, 0.32f),
    glm::vec3(0.0f, 0.64f, 0.36f),
    glm::vec3(0.31999999999999995f, 0.68f, 0.0f),
    glm::vec3(0.27999999999999997f, 0.68f, 0.04f),
    glm::vec3(0.23999999999999994f, 0.68f, 0.08f),
    glm::vec3(0.19999999999999996f, 0.68f, 0.12f),
    glm::vec3(0.15999999999999995f, 0.68f, 0.16f),
    glm::vec3(0.11999999999999994f, 0.68f, 0.2f),
    glm::vec3(0.07999999999999996f, 0.68f, 0.24f),
    glm::vec3(0.039999999999999925f, 0.68f, 0.28f),
    glm::vec3(-5.551115123125783e-17f, 0.68f, 0.32f),
    glm::vec3(0.28f, 0.72f, 0.0f),
    glm::vec3(0.24000000000000002f, 0.72f, 0.04f),
    glm::vec3(0.2f, 0.72f, 0.08f),
    glm::vec3(0.16000000000000003f, 0.72f, 0.12f),
    glm::vec3(0.12000000000000002f, 0.72f, 0.16f),
    glm::vec3(0.08000000000000002f, 0.72f, 0.2f),
    glm::vec3(0.040000000000000036f, 0.72f, 0.24f),
    glm::vec3(0.0f, 0.72f, 0.28f),
    glm::vec3(0.24f, 0.76f, 0.0f),
    glm::vec3(0.19999999999999998f, 0.76f, 0.04f),
    glm::vec3(0.15999999999999998f, 0.76f, 0.08f),
    glm::vec3(0.12f, 0.76f, 0.12f),
    glm::vec3(0.07999999999999999f, 0.76f, 0.16f),
    glm::vec3(0.03999999999999998f, 0.76f, 0.2f),
    glm::vec3(0.0f, 0.76f, 0.24f),
    glm::vec3(0.19999999999999996f, 0.8f, 0.0f),
    glm::vec3(0.15999999999999995f, 0.8f, 0.04f),
    glm::vec3(0.11999999999999995f, 0.8f, 0.08f),
    glm::vec3(0.07999999999999996f, 0.8f, 0.12f),
    glm::vec3(0.03999999999999995f, 0.8f, 0.16f),
    glm::vec3(-5.551115123125783e-17f, 0.8f, 0.2f),
    glm::vec3(0.16000000000000003f, 0.84f, 0.0f),
    glm::vec3(0.12000000000000002f, 0.84f, 0.04f),
    glm::vec3(0.08000000000000003f, 0.84f, 0.08f),
    glm::vec3(0.040000000000000036f, 0.84f, 0.12f),
    glm::vec3(2.7755575615628914e-17f, 0.84f, 0.16f),
    glm::vec3(0.12f, 0.88f, 0.0f),
    glm::vec3(0.07999999999999999f, 0.88f, 0.04f),
    glm::vec3(0.039999999999999994f, 0.88f, 0.08f),
    glm::vec3(0.0f, 0.88f, 0.12f),
    glm::vec3(0.07999999999999996f, 0.92f, 0.0f),
    glm::vec3(0.03999999999999996f, 0.92f, 0.04f),
    glm::vec3(-4.163336342344337e-17f, 0.92f, 0.08f),
    glm::vec3(0.040000000000000036f, 0.96f, 0.0f),
    glm::vec3(3.469446951953614e-17f, 0.96f, 0.04f),
    glm::vec3(0.0f, 1.0f, 0.0f),
};

#ifndef M_ABS
#define M_ABS(a) (((a) < 0) ? -(a) : (a))
#endif

static void raster_line(uint8_t *dest, int width, int height, const glm::vec2& p0, const glm::vec2& p1, glm::ivec4& color)
{
    uint8_t *data = dest;
    int x0 = (int)p0.x;
    int y0 = height - (int)p0.y;
    int x1 = (int)p1.x;
    int y1 = height - (int)p1.y;
    int w = width;
    int h = height;
    int dx =  M_ABS(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -M_ABS(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int comp = 4;
    while (1) {

        if (x0 > -1 && y0 > -1 && x0 < w && y0 < h) { /* safe, but should be taken out of the loop for speed (clipping ?) */
            uint8_t* pixel = data + (y0 * w + x0) * comp;
            for (int c = 0; c < comp; c++)
                pixel[c] = color[c];
        }

        if (x0 == x1 && y0 == y1)
            break;

        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
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

    ClearPixels();
    //SoftwareRenderer::RenderImpl(cameraMat, projMat, viewport, nearFar);

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

#ifdef DUMP
    Log::printf("***************************************\n");
    PrintMat(::viewMat, "viewMat");
    PrintMat(::projMat, "projMat");
    PrintVec(::viewport, "viewport");
    PrintVec(::projParams, "projParams");
    PrintVec(::eye, "eye");
#endif

    for (size_t i = 0; i < posVec.size(); i++)
    {
#ifdef DUMP
        Log::printf("attribs[%d] =\n", (int)i);
#endif
        position = posVec[i];
        r_sh0 = r_sh0Vec[i];
        g_sh0 = g_sh0Vec[i];
        b_sh0 = b_sh0Vec[i];
        cov3_col0 = cov3_col0Vec[i];
        cov3_col1 = cov3_col1Vec[i];
        cov3_col2 = cov3_col2Vec[i];
#ifdef DUMP
        PrintVec(position, "    position");
        PrintVec(r_sh0, "    r_sh0");
        PrintVec(g_sh0, "    g_sh0");
        PrintVec(b_sh0, "    b_sh0");
        PrintVec(cov3_col0, "    cov3_col0");
        PrintVec(cov3_col1, "    cov3_col1");
        PrintVec(cov3_col2, "    cov3_col2");

        Log::printf("debug_splat_vert[%d] =\n", (int)i);
#endif
        debug_splat_vert();
#ifdef DUMP
        PrintVec(geom_color, "    geom_color");
        PrintVec(geom_cov2, "    geom_cov2");
        PrintVec(geom_p, "    geom_p");
        PrintVec(gl_Position, "    gl_Position");

        Log::printf("debug_splat_geom[%d] =\n", (int)i);
#endif
        geomOutVec.clear();

        debug_splat_geom();

        uint8_t r = (uint8_t)(glm::clamp(geom_color.x, 0.0f, 1.0f) * 255.0f);
        uint8_t g = (uint8_t)(glm::clamp(geom_color.y, 0.0f, 1.0f) * 255.0f);
        uint8_t b = (uint8_t)(glm::clamp(geom_color.z, 0.0f, 1.0f) * 255.0f);

        SetThickPixel((int)geom_p.x, (int)geom_p.y, r, g, b);

        for (auto& go : geomOutVec)
        {
            // so I can see it
            SetThickPixel((int)go.fragPos.x, (int)go.fragPos.y, r, g, b);
        }

        if (geomOutVec.size() >= 4)
        {
            glm::ivec4 color(b, g, r, 255);

            // triangle
            SampleTri(geomOutVec[0], geomOutVec[1], geomOutVec[2]);
            raster_line((uint8_t*)rawPixels, WIDTH, HEIGHT, geomOutVec[0].fragPos, geomOutVec[1].fragPos, color);
            raster_line((uint8_t*)rawPixels, WIDTH, HEIGHT, geomOutVec[0].fragPos, geomOutVec[2].fragPos, color);
            raster_line((uint8_t*)rawPixels, WIDTH, HEIGHT, geomOutVec[1].fragPos, geomOutVec[2].fragPos, color);

            // triangle
            SampleTri(geomOutVec[1], geomOutVec[2], geomOutVec[3]);
            raster_line((uint8_t*)rawPixels, WIDTH, HEIGHT, geomOutVec[1].fragPos, geomOutVec[2].fragPos, color);
            raster_line((uint8_t*)rawPixels, WIDTH, HEIGHT, geomOutVec[1].fragPos, geomOutVec[3].fragPos, color);
            raster_line((uint8_t*)rawPixels, WIDTH, HEIGHT, geomOutVec[2].fragPos, geomOutVec[3].fragPos, color);
        }
    }
}

void ShaderDebugRenderer::SampleTri(const GeomOut& p0, const GeomOut& p1, const GeomOut& p2)
{
    frag_color = p0.frag_color;
    frag_cov2inv = p0.frag_cov2inv;
    frag_p = p0.frag_p;

    for (auto&& b : baryVec)
    {
        gl_FragCoord = b[0] * p0.fragPos + b[1] * p1.fragPos + b[2] * p2.fragPos;
        debug_splat_frag();

        uint8_t r = (uint8_t)(glm::clamp(out_color.x, 0.0f, 1.0f) * 255.0f);
        uint8_t g = (uint8_t)(glm::clamp(out_color.y, 0.0f, 1.0f) * 255.0f);
        uint8_t b = (uint8_t)(glm::clamp(out_color.z, 0.0f, 1.0f) * 255.0f);

        if (out_color.w > 0.05f)
        {
            SetThickPixel((int)gl_FragCoord.x, (int)gl_FragCoord.y, r, g, b);
        }
        else
        {
            SetThickPixel((int)gl_FragCoord.x, (int)gl_FragCoord.y, 255, 255, 255);
        }
    }
}
