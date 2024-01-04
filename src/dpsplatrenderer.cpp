#include "dpsplatrenderer.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#endif

#include <glm/gtc/matrix_transform.hpp>

#ifndef __ANDROID__
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/image.h"
#include "core/log.h"
#include "core/texture.h"
#include "core/util.h"

static const uint32_t NUM_PASSES = 25;

DPSplatRenderer::DPSplatRenderer() : fboSize(0, 0)
{
}

DPSplatRenderer::~DPSplatRenderer()
{
    for (auto& rbo : rboVec)
    {
        glDeleteRenderbuffers(1, &rbo);
    }
    for (auto& fbo : fboVec)
    {
        glDeleteFramebuffers(1, &fbo);
    }
}

bool DPSplatRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud, bool isFramebufferSRGBEnabledIn,
                           bool useFullSHIn)
{
    GL_ERROR_CHECK("SplatRenderer::Init() begin");
    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;
    useFullSH = useFullSHIn;

    splatProg = std::make_shared<Program>();
    if (isFramebufferSRGBEnabled || useFullSH)
    {
        std::string defines = "";
        if (isFramebufferSRGBEnabled)
        {
            defines += "#define FRAMEBUFFER_SRGB\n";
        }
        if (useFullSH)
        {
            defines += "#define FULL_SH\n";
        }
        splatProg->AddMacro("DEFINES", defines);
    }
    if (!splatProg->LoadVertGeomFrag("shader/splat_vert.glsl", "shader/splat_geom.glsl", "shader/splat_frag.glsl"))
    {
        Log::E("Error loading splat shaders!\n");
        return false;
    }

    peelProg = std::make_shared<Program>();
    if (isFramebufferSRGBEnabled || useFullSH)
    {
        std::string defines = "";
        if (isFramebufferSRGBEnabled)
        {
            defines += "#define FRAMEBUFFER_SRGB\n";
        }
        if (useFullSH)
        {
            defines += "#define FULL_SH\n";
        }
        peelProg->AddMacro("DEFINES", defines);
    }
    if (!peelProg->LoadVertGeomFrag("shader/splat_vert.glsl", "shader/splat_geom.glsl", "shader/splat_peel_frag.glsl"))
    {
        Log::E("Error loading splat peel shaders!\n");
        return false;
    }

    desktopProg = std::make_shared<Program>();
    if (!desktopProg->LoadVertFrag("shader/desktop_vert.glsl", "shader/desktop_frag.glsl"))
    {
        Log::E("Error loading desktop shader!\n");
        return false;
    }

    BuildVertexArrayObject(gaussianCloud);

    for (uint32_t i = 0; i < NUM_PASSES; i++)
    {
        uint32_t fbo;
        glGenFramebuffers(1, &fbo);
        fboVec.push_back(fbo);
    }

    // AJT: HACK REMOVE
    Image carpetImg;
    if (!carpetImg.Load("texture/carpet.png"))
    {
        Log::E("Error loading carpet.png\n");
        return false;
    }
    carpetImg.isSRGB = isFramebufferSRGBEnabledIn;
    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::Repeat, WrapType::Repeat};
    carpetTex = std::make_shared<Texture>(carpetImg, texParams);

    GL_ERROR_CHECK("SplatRenderer::Init() end");

    return true;
}

void DPSplatRenderer::Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
}

// viewport = (x, y, width, height)
void DPSplatRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                             const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ZoneScoped;

    glm::ivec2 viewportSize(viewport.z, viewport.w);
    if (viewportSize != fboSize)
    {
        fboSize = viewportSize;
        FrameBufferInit();
    }

    GL_ERROR_CHECK("DPSplatRenderer::Render() begin");

    float aspectRatio = (float)fboSize.x / (float)fboSize.y;
    glm::mat4 viewMat = glm::inverse(cameraMat);
    glm::vec3 eye = glm::vec3(cameraMat[3]);

    {
        ZoneScopedNC("draw", tracy::Color::Red4);

        glDisable(GL_BLEND);

        for (uint32_t i = 0; i < NUM_PASSES; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fboVec[i]);

            glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            std::shared_ptr<Program> prog = (i == 0) ? splatProg : peelProg;

            prog->Bind();
            prog->SetUniform("viewMat", viewMat);
            prog->SetUniform("projMat", projMat);
            prog->SetUniform("viewport", viewport);
            prog->SetUniform("projParams", glm::vec4(0.0f, nearFar.x, nearFar.y, 0.0f));
            prog->SetUniform("eye", eye);

            if (i > 0)
            {
                depthTexVec[i - 1]->Bind(0);
                //carpetTex->Bind(0);
                prog->SetUniform("depthTex", 0);
            }

            splatVao->Bind();
            glDrawArrays(GL_POINTS, 0, (GLsizei)posVec.size());
            splatVao->Unbind();

            GL_ERROR_CHECK("DPSplatRenderer::Render() pass");
        }
    }

    glEnable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ComposeLayers();
}

void DPSplatRenderer::BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    splatVao = std::make_shared<VertexArrayObject>();

    // convert gaussianCloud data into buffers
    size_t numPoints = gaussianCloud->size();
    posVec.reserve(numPoints);

    // sh coeff
    std::vector<glm::vec4> r_sh0Vec, g_sh0Vec, b_sh0Vec;
    std::vector<glm::vec4> r_sh1Vec, r_sh2Vec, r_sh3Vec, r_sh4Vec;
    std::vector<glm::vec4> g_sh1Vec, g_sh2Vec, g_sh3Vec, g_sh4Vec;
    std::vector<glm::vec4> b_sh1Vec, b_sh2Vec, b_sh3Vec, b_sh4Vec;

    // 3x3 cov matrix
    std::vector<glm::vec3> cov3_col0Vec, cov3_col1Vec, cov3_col2Vec;

    r_sh0Vec.reserve(numPoints);
    g_sh0Vec.reserve(numPoints);
    b_sh0Vec.reserve(numPoints);

    if (useFullSH)
    {
        r_sh1Vec.reserve(numPoints);
        r_sh2Vec.reserve(numPoints);
        r_sh3Vec.reserve(numPoints);
        g_sh1Vec.reserve(numPoints);
        g_sh2Vec.reserve(numPoints);
        g_sh3Vec.reserve(numPoints);
        b_sh1Vec.reserve(numPoints);
        b_sh2Vec.reserve(numPoints);
        b_sh3Vec.reserve(numPoints);
    }

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

        if (useFullSH)
        {
            r_sh1Vec.emplace_back(glm::vec4(g.f_rest[3], g.f_rest[4], g.f_rest[5], g.f_rest[6]));
            r_sh2Vec.emplace_back(glm::vec4(g.f_rest[7], g.f_rest[8], g.f_rest[9], g.f_rest[10]));
            r_sh3Vec.emplace_back(glm::vec4(g.f_rest[11], g.f_rest[12], g.f_rest[13], g.f_rest[14]));
            g_sh1Vec.emplace_back(glm::vec4(g.f_rest[18], g.f_rest[19], g.f_rest[20], g.f_rest[21]));
            g_sh2Vec.emplace_back(glm::vec4(g.f_rest[22], g.f_rest[23], g.f_rest[24], g.f_rest[25]));
            g_sh3Vec.emplace_back(glm::vec4(g.f_rest[26], g.f_rest[27], g.f_rest[28], g.f_rest[29]));
            b_sh1Vec.emplace_back(glm::vec4(g.f_rest[33], g.f_rest[34], g.f_rest[35], g.f_rest[36]));
            b_sh2Vec.emplace_back(glm::vec4(g.f_rest[37], g.f_rest[38], g.f_rest[39], g.f_rest[40]));
            b_sh3Vec.emplace_back(glm::vec4(g.f_rest[41], g.f_rest[42], g.f_rest[43], g.f_rest[44]));
        }

        glm::mat3 V = g.ComputeCovMat();
        cov3_col0Vec.push_back(V[0]);
        cov3_col1Vec.push_back(V[1]);
        cov3_col2Vec.push_back(V[2]);
    }
    auto positionBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);

    auto r_sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh0Vec);
    auto g_sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh0Vec);
    auto b_sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh0Vec);

    auto r_sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh1Vec);
    auto r_sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh2Vec);
    auto r_sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh3Vec);
    auto g_sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh1Vec);
    auto g_sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh2Vec);
    auto g_sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh3Vec);
    auto b_sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh1Vec);
    auto b_sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh2Vec);
    auto b_sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh3Vec);

    auto cov3_col0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col0Vec);
    auto cov3_col1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col1Vec);
    auto cov3_col2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col2Vec);

    // setup vertex array object with buffers
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("position"), positionBuffer);

    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh0"), r_sh0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh0"), g_sh0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh0"), b_sh0Buffer);

    if (useFullSH)
    {
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh1"), r_sh1Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh2"), r_sh2Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh3"), r_sh3Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh1"), g_sh1Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh2"), g_sh2Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh3"), g_sh3Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh1"), b_sh1Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh2"), b_sh2Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh3"), b_sh3Buffer);
    }

    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col0"), cov3_col0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col1"), cov3_col1Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col2"), cov3_col2Buffer);
}

void DPSplatRenderer::FrameBufferInit()
{
    Texture::Params texParams = {FilterType::Linear, FilterType::Linear, WrapType::ClampToEdge, WrapType::ClampToEdge};

    for (auto& rbo : rboVec)
    {
        glDeleteRenderbuffers(1, &rbo);
    }

    for (uint32_t i = 0; i < NUM_PASSES; i++)
    {
        colorTexVec.push_back(std::make_shared<Texture>(fboSize.x, fboSize.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, texParams));
        depthTexVec.push_back(std::make_shared<Texture>(fboSize.x, fboSize.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, texParams));

        // Attach it to the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fboVec[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexVec[i]->texture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexVec[i]->texture, 0);

        /*
        uint32_t rbo;
        glGenRenderbuffers(1, &rbo);
        rboVec.push_back(rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, fboSize.x, fboSize.y);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        */

        uint32_t fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        {
            Log::E("DPSplatRenderer bad fbo[%d] status = %d\n", i, fboStatus);
        }
    }

    GL_ERROR_CHECK("DPSplatRenderer::FrameBufferInit()");
}

void DPSplatRenderer::ComposeLayers()
{
    glDisable(GL_DEPTH_TEST);
    for (int32_t i = (int32_t)NUM_PASSES - 1; i >= 0; i--)
    {
        glm::mat4 projMat = glm::ortho(0.0f, (float)fboSize.x, 0.0f, (float)fboSize.y, -10.0f, 10.0f);

        desktopProg->Bind();
        desktopProg->SetUniform("modelViewProjMat", projMat);
        desktopProg->SetUniform("color", glm::vec4(1.0f));

        // use texture unit 0 for colorTexture
        colorTexVec[i]->Bind(0);
        desktopProg->SetUniform("colorTexture", 0);

        glm::vec2 xyLowerLeft(0.0f, 0.0f);
        glm::vec2 xyUpperRight((float)fboSize.x, (float)fboSize.y);
        glm::vec2 uvLowerLeft(0.0f, 0.0f);
        glm::vec2 uvUpperRight(1.0f, 1.0f);

        float depth = -9.0f;
        glm::vec3 positions[] = {glm::vec3(xyLowerLeft, depth), glm::vec3(xyUpperRight.x, xyLowerLeft.y, depth),
                                 glm::vec3(xyUpperRight, depth), glm::vec3(xyLowerLeft.x, xyUpperRight.y, depth)};
        desktopProg->SetAttrib("position", positions);

        glm::vec2 uvs[] = {uvLowerLeft, glm::vec2(uvUpperRight.x, uvLowerLeft.y),
                           uvUpperRight, glm::vec2(uvLowerLeft.x, uvUpperRight.y)};
        desktopProg->SetAttrib("uv", uvs);

        const size_t NUM_INDICES = 6;
        uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
        glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);
    }
    glEnable(GL_DEPTH_TEST);
}
