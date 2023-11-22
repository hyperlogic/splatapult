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
        if (tIter.second.screenSpace)
        {
            textProg->SetUniform("modelViewProjMat", aspectMat * tIter.second.xform);
        }
        else
        {
            textProg->SetUniform("modelViewProjMat", viewProjMat * tIter.second.xform);
        }
        textProg->SetUniform("color", tIter.second.color);
        textProg->SetAttrib("position", tIter.second.posVec.data());
        textProg->SetAttrib("uv", tIter.second.uvVec.data());

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tIter.second.posVec.size());
    }
}

void TextRenderer::BuildText(Text& text, glm::vec2& pen, float lineHeight, const std::string& asciiString) const
{
    int r = 0;
    int c = 0;
    for (auto&& ch : asciiString)
    {
        if (ch == ' ')
        {
            pen += lineHeight * spaceGlyph.advance;
            c++;
        }
        else if (ch == '\n')
        {
            pen = lineHeight * glm::vec2(0.0f, (float)-(r + 1));
            r++;
        }
        else if (ch == '\t')
        {
            int numSpaces = TAB_SIZE - (c % TAB_SIZE);
            pen += lineHeight * (float)numSpaces * spaceGlyph.advance;
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

            text.posVec.push_back(pen + lineHeight * g.xyMin);
            text.posVec.push_back(pen + lineHeight * g.xyMax);
            text.posVec.push_back(pen + lineHeight * glm::vec2(g.xyMin.x, g.xyMax.y));
            text.posVec.push_back(pen + lineHeight * g.xyMin);
            text.posVec.push_back(pen + lineHeight * glm::vec2(g.xyMax.x, g.xyMin.y));
            text.posVec.push_back(pen + lineHeight * g.xyMax);
            text.uvVec.push_back(g.uvMin);
            text.uvVec.push_back(g.uvMax);
            text.uvVec.push_back(glm::vec2(g.uvMin.x, g.uvMax.y));
            text.uvVec.push_back(g.uvMin);
            text.uvVec.push_back(glm::vec2(g.uvMax.x, g.uvMin.y));
            text.uvVec.push_back(g.uvMax);
            pen += lineHeight * g.advance;
            c++;
        }
    }
}

// creates a new text and adds it to the scene
TextRenderer::TextKey TextRenderer::AddText(const glm::mat4 xform, const glm::vec4& color,
                                            float lineHeight, const std::string& asciiString)
{
    Text text;
    text.xform = xform;
    text.color = color;
    text.posVec.reserve(asciiString.size() * 6);
    text.uvVec.reserve(asciiString.size() * 6);
    text.screenSpace = false;

    glm::vec2 pen(0.0f, 0.0f);
    BuildText(text, pen, lineHeight, asciiString);

    uint32_t textKey = nextKey++;
    textMap.insert(std::pair<uint32_t, Text>(textKey, text));

    return textKey;
}

TextRenderer::TextKey TextRenderer::AddText2D(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                                              const std::string& asciiString)
{
    const float TEXT_LINE_HEIGHT = 2.0f / numRows;
    glm::vec3 offset(0.1f * TEXT_LINE_HEIGHT, 1.0f - 0.75f * TEXT_LINE_HEIGHT, 0.0f);
    Text text;
    text.xform = MakeMat4(glm::quat(), offset + glm::vec3((float)pos.x * spaceGlyph.advance.x * TEXT_LINE_HEIGHT, (float)pos.y * -TEXT_LINE_HEIGHT, 0.0f));
    text.color = color;
    text.posVec.reserve(asciiString.size() * 6);
    text.uvVec.reserve(asciiString.size() * 6);
    text.screenSpace = true;

    glm::vec2 pen(0.0f, 0.0f);
    BuildText(text, pen, TEXT_LINE_HEIGHT, asciiString);

    uint32_t textKey = nextKey++;
    textMap.insert(std::pair<uint32_t, Text>(textKey, text));

    return textKey;
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
