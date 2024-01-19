/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Program;
struct Texture;

class TextRenderer
{
public:

	TextRenderer();

	bool Init(const std::string& fontJsonFilename, const std::string& fontPngFilename);

	// viewport = (x, y, width, height)
	void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);

    using TextKey = uint32_t;

    // creates a new text and adds it to the scene
    TextKey AddWorldText(const glm::mat4 xform, const glm::vec4& color,
                    float lineHeight, const std::string& asciiString);
    TextKey AddScreenText(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                          const std::string& asciiString);
    TextKey AddScreenTextWithDropShadow(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                                        const glm::vec4& shadowColor, const std::string& asciiString);
    void SetTextXform(TextKey key, const glm::mat4 xform);

    // removes text object form the scene
    void RemoveText(TextKey key);

protected:

    struct Glyph
    {
        glm::vec2 xyMin;
        glm::vec2 xyMax;
        glm::vec2 uvMin;
        glm::vec2 uvMax;
        glm::vec2 advance;
    };

    struct Text
    {
        glm::mat4 xform;
        std::vector<glm::vec3> posVec;
        std::vector<glm::vec2> uvVec;
        std::vector<glm::vec4> colorVec;
        bool isScreenAligned;
    };

    void BuildText(Text& text, const glm::vec3& pen, float lineHeight, const glm::vec4& color,
                   const std::string& asciiString) const;
    TextKey AddScreenTextImpl(const glm::ivec2& pos, int numRows, const glm::vec4& color,
                              const std::string& asciiString, bool addDropShadow, const glm::vec4& shadowColor);

    std::unordered_map<uint8_t, Glyph> glyphMap;
    float textureWidth;
    std::shared_ptr<Program> textProg;
    std::shared_ptr<Texture> fontTex;
    std::unordered_map<uint32_t, Text> textMap;
    Glyph spaceGlyph;
};


