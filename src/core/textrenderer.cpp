/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "textrenderer.h"

#include <fstream>

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES3/gl32.h>
#else
#include <GL/glew.h>
#endif

#include <nlohmann/json.hpp>

#include "core/image.h"
#include "core/log.h"
#include "core/util.h"
#include "core/program.h"
#include "core/texture.h"

const int TAB_SIZE = 4;
static TextRenderer::TextKey nextKey = 1;

TextRenderer::TextRenderer()
{

}

bool TextRenderer::Init(const std::string& fontJsonFilename, const std::string& fontPngFilename)
{
    std::ifstream f(GetRootPath() + fontJsonFilename);
    if (f.fail())
    {
        return false;
    }

    try
    {
        nlohmann::json j = nlohmann::json::parse(f);
        textureWidth = j["texture_width"].template get<float>();
        nlohmann::json metrics = j["glyph_metrics"];
        for (auto& iter : metrics.items())
        {
            int key = iter.value()["ascii_index"].template get<int>();
            if (key < 0 && key > std::numeric_limits<uint8_t>::max())
            {
                Log::W("TextRenderer(%s) glyph %d is out of range\n", fontJsonFilename.c_str(), key);
                continue;
            }
            Glyph g;
            nlohmann::json v2 = iter.value()["xy_lower_left"];
            g.xyMin = glm::vec2(v2[0].template get<float>(), v2[1].template get<float>());
            v2 = iter.value()["xy_upper_right"];
            g.xyMax = glm::vec2(v2[0].template get<float>(), v2[1].template get<float>());
            v2 = iter.value()["uv_lower_left"];
            g.uvMin = glm::vec2(v2[0].template get<float>(), v2[1].template get<float>());
            v2 = iter.value()["uv_upper_right"];
            g.uvMax = glm::vec2(v2[0].template get<float>(), v2[1].template get<float>());
            v2 = iter.value()["advance"];
            g.advance = glm::vec2(v2[0].template get<float>(), v2[1].template get<float>());

            glyphMap.insert(std::pair<uint8_t, Glyph>((uint8_t)key, g));
        }
        // TODO: support kerning table, for variable width fonts
    }
    catch (const nlohmann::json::exception& e)
    {
        std::string s = e.what();
        Log::E("TextRenderer::Init(%s) exception: %s\n", fontJsonFilename.c_str(), s.c_str());
        return false;
    }

    // find the spaceGlyph
    auto gIter = glyphMap.find((uint8_t)' ');
    assert(gIter != glyphMap.end());
    if (gIter != glyphMap.end())
    {
        spaceGlyph = gIter->second;
    }

    Image fontImg;
    if (!fontImg.Load(fontPngFilename))
    {
        Log::E("Error loading fontPng\n");
        return false;
    }

    // TODO: get gamma correct.
    //fontImg.isSRGB = isFramebufferSRGBEnabled;

    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::ClampToEdge, WrapType::ClampToEdge};
    fontTex = std::make_shared<Texture>(fontImg, texParams);

    textProg = std::make_shared<Program>();
    if (!textProg->LoadVertFrag("shader/text_vert.glsl", "shader/text_frag.glsl"))
    {
        Log::E("Error loading TextRenderer shader!\n");
        return false;
    }

    return true;
}

void TextRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                          const glm::vec4& viewport, const glm::vec2& nearFar)
{
    textProg->Bind();

    // use texture unit 0 for fontTexture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex->texture);
    textProg->SetUniform("fontTex", 0);

    glm::mat4 viewProjMat = projMat * glm::inverse(cameraMat);
    float aspect = viewport.w / viewport.z;
    glm::mat4 aspectMat = MakeMat4(glm::vec3(aspect, 1.0f, 1.0f), glm::quat(), glm::vec3(-aspect / aspect, 0.0f, 0.0f));
    for (auto&& tIter : textMap)
    {
        if (tIter.second.isScreenAligned)
        {
            textProg->SetUniform("modelViewProjMat", aspectMat * tIter.second.xform);
        }
        else
        {
            textProg->SetUniform("modelViewProjMat", viewProjMat * tIter.second.xform);
        }
        textProg->SetAttrib("position", tIter.second.posVec.data());
        textProg->SetAttrib("uv", tIter.second.uvVec.data());
        textProg->SetAttrib("color", tIter.second.colorVec.data());

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tIter.second.posVec.size());
    }
}

// creates a new text and adds it to the scene
TextRenderer::TextKey TextRenderer::AddWorldText(const glm::mat4 xform, const glm::vec4& color,
                                                 float lineHeight, const std::string& asciiString)
{
    Text text;
    text.xform = xform;
    text.posVec.reserve(asciiString.size() * 6);
    text.uvVec.reserve(asciiString.size() * 6);
    text.colorVec.reserve(asciiString.size() * 6);
    text.isScreenAligned = false;

    glm::vec3 pen(0.0f, 0.0f, 0.0f);
    BuildText(text, pen, lineHeight, color, asciiString);

    uint32_t textKey = nextKey++;
    textMap.insert(std::pair<uint32_t, Text>(textKey, text));

    return textKey;
}

TextRenderer::TextKey TextRenderer::AddScreenText(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                                                  const std::string& asciiString)
{
    const bool addDropShadow = false;
    return AddScreenTextImpl(pos, numRows, color, asciiString, addDropShadow, glm::vec4());
}

TextRenderer::TextKey TextRenderer::AddScreenTextWithDropShadow(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                                                                const glm::vec4& shadowColor, const std::string& asciiString)
{
    const bool addDropShadow = true;
    return AddScreenTextImpl(pos, numRows, color, asciiString, addDropShadow, shadowColor);
}

void TextRenderer::SetTextXform(TextKey key, const glm::mat4 xform)
{
    auto tIter = textMap.find(key);
    if (tIter != textMap.end())
    {
        tIter->second.xform = xform;
    }
}

// removes text object form the scene
void TextRenderer::RemoveText(TextKey key)
{
    textMap.erase(key);
}

void TextRenderer::BuildText(Text& text, const glm::vec3& pen, float lineHeight, const glm::vec4& color,
                             const std::string& asciiString) const
{
    int r = 0;
    int c = 0;
    glm::vec2 penxy = pen;
    float depth = pen.z;
    for (auto&& ch : asciiString)
    {
        if (ch == ' ')
        {
            penxy += lineHeight * spaceGlyph.advance;
            c++;
        }
        else if (ch == '\n')
        {
            penxy = lineHeight * glm::vec2(0.0f, (float)-(r + 1));
            r++;
        }
        else if (ch == '\t')
        {
            int numSpaces = TAB_SIZE - (c % TAB_SIZE);
            penxy += lineHeight * (float)numSpaces * spaceGlyph.advance;
            c += numSpaces;
        }
        else
        {
            auto gIter = glyphMap.find((uint8_t)ch);
            if (gIter == glyphMap.end())
            {
                continue;
            }
            const Glyph& g = gIter->second;

            text.posVec.push_back(glm::vec3(penxy + lineHeight * g.xyMin, depth));
            text.posVec.push_back(glm::vec3(penxy + lineHeight * g.xyMax, depth));
            text.posVec.push_back(glm::vec3(penxy + lineHeight * glm::vec2(g.xyMin.x, g.xyMax.y), depth));
            text.posVec.push_back(glm::vec3(penxy + lineHeight * g.xyMin, depth));
            text.posVec.push_back(glm::vec3(penxy + lineHeight * glm::vec2(g.xyMax.x, g.xyMin.y), depth));
            text.posVec.push_back(glm::vec3(penxy + lineHeight * g.xyMax, depth));
            text.uvVec.push_back(g.uvMin);
            text.uvVec.push_back(g.uvMax);
            text.uvVec.push_back(glm::vec2(g.uvMin.x, g.uvMax.y));
            text.uvVec.push_back(g.uvMin);
            text.uvVec.push_back(glm::vec2(g.uvMax.x, g.uvMin.y));
            text.uvVec.push_back(g.uvMax);
            for (int i = 0; i < 6; i++)
            {
                text.colorVec.push_back(color);
            }

            penxy += lineHeight * g.advance;
            c++;
        }
    }
}

TextRenderer::TextKey TextRenderer::AddScreenTextImpl(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                                                      const std::string& asciiString, bool addDropShadow,
                                                      const glm::vec4& shadowColor)
{
    const float TEXT_LINE_HEIGHT = 2.0f / numRows;
    glm::vec3 origin(0.1f * TEXT_LINE_HEIGHT, 1.0f - 0.75f * TEXT_LINE_HEIGHT, 0.0f);
    glm::vec3 offset((float)pos.x * spaceGlyph.advance.x * TEXT_LINE_HEIGHT, (float)pos.y * -TEXT_LINE_HEIGHT, 0.0f);
    Text text;
    text.xform = MakeMat4(glm::quat(), origin + offset);
    size_t vecSize = addDropShadow ? (asciiString.size() * 6 * 2) : (asciiString.size() * 6);
    text.posVec.reserve(vecSize);
    text.uvVec.reserve(vecSize);
    text.colorVec.reserve(vecSize);
    text.isScreenAligned = true;

    if (addDropShadow)
    {
        glm::vec3 shadowPen = glm::vec3(0.05f * TEXT_LINE_HEIGHT, -0.05f * TEXT_LINE_HEIGHT, 0.1f);
        BuildText(text, shadowPen, TEXT_LINE_HEIGHT, shadowColor, asciiString);
    }

    glm::vec3 pen(0.0f, 0.0f, 0.0f);
    BuildText(text, pen, TEXT_LINE_HEIGHT, color, asciiString);

    uint32_t textKey = nextKey++;
    textMap.insert(std::pair<uint32_t, Text>(textKey, text));

    return textKey;
}


