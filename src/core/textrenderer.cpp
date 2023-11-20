#include "textrenderer.h"

#include <fstream>
#include <GL/glew.h>
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

    glm::mat4 viewProjMat = projMat * glm::inverse(cameraMat);
    for (auto&& tIter : textMap)
    {
        textProg->SetUniform("modelViewProjMat", viewProjMat * tIter.second.xform);
        textProg->SetUniform("color", tIter.second.color);
        textProg->SetAttrib("position", tIter.second.posVec.data());
        textProg->SetAttrib("uv", tIter.second.uvVec.data());
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tIter.second.posVec.size());
    }
}

// creates a new text and adds it to the scene
TextRenderer::TextKey TextRenderer::AddText(const glm::mat4 xform, const glm::vec4& color, const std::string& asciiString)
{
    Text text;
    text.xform = xform;
    text.color = color;
    text.posVec.reserve(asciiString.size() * 6);
    text.uvVec.reserve(asciiString.size() * 6);

    Glyph spaceGlyph;
    auto gIter = glyphMap.find((uint8_t)' ');
    if (gIter != glyphMap.end())
    {
        spaceGlyph = gIter->second;
    }

    glm::vec2 pen(0.0f, 0.0f);
    int i = 0;
    for (auto&& ch : asciiString)
    {
        auto gIter = glyphMap.find((uint8_t)ch);
        if (gIter == glyphMap.end())
        {
            continue;
        }
        const Glyph& g = gIter->second;

        if (ch == ' ')
        {
            pen += g.advance;
        }
        else if (ch == '\n')
        {
            pen = glm::vec2(0.0f, pen.y - 1.0f);
        }
        else if (ch == '\t')
        {
            int numSpaces = TAB_SIZE - (i % TAB_SIZE);
            pen += (float)numSpaces * spaceGlyph.advance;
        }
        else
        {
            text.posVec.push_back(pen + g.xyMin);
            text.posVec.push_back(pen + g.xyMax);
            text.posVec.push_back(pen + glm::vec2(g.xyMin.x, g.xyMax.y));
            text.posVec.push_back(pen + g.xyMin);
            text.posVec.push_back(pen + glm::vec2(g.xyMax.x, g.xyMin.y));
            text.posVec.push_back(pen + g.xyMax);
            text.uvVec.push_back(pen + g.uvMin);
            text.uvVec.push_back(pen + g.uvMax);
            text.uvVec.push_back(pen + glm::vec2(g.uvMin.x, g.uvMax.y));
            text.uvVec.push_back(pen + g.uvMin);
            text.uvVec.push_back(pen + glm::vec2(g.uvMax.x, g.uvMin.y));
            text.uvVec.push_back(pen + g.uvMax);
            pen += g.advance;
        }

        i++;
    }

    uint32_t textKey = nextKey++;
    textMap.insert(std::pair<uint32_t, Text>(textKey, text));

    return textKey;
}

// removes text object form the scene
void TextRenderer::RemoveText(TextKey key)
{
    textMap.erase(key);
}
