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
    TextKey AddText(const glm::mat4 xform, const glm::vec4& color, float lineHeight, const std::string& asciiString);
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
        glm::vec4 color;
        std::vector<glm::vec2> posVec;
        std::vector<glm::vec2> uvVec;
    };

    std::unordered_map<uint8_t, Glyph> glyphMap;
    float textureWidth;
    std::shared_ptr<Program> textProg;
    std::shared_ptr<Texture> fontTex;
    std::unordered_map<uint32_t, Text> textMap;
};


