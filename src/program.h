#ifndef PROGRAM_H
#define PROGRAM_H

#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>

struct Program
{
    Program();
    ~Program();
    bool Load(const std::string& vertFilename, const std::string& fragFilename);
    void Apply() const;

    int GetUniformLoc(const std::string& name) const;
    int GetAttribLoc(const std::string& name) const;

    template <typename T>
    void SetUniform(const std::string& name, T value) const
    {
        auto iter = uniforms.find(name);
        if (iter != uniforms.end())
        {
            SetUniform(iter->second.loc, value);
        }
        else
        {
            std::cerr << "could not find uniform " << name << " for program " << name << std::endl;
        }
    }

    void SetUniform(int loc, int value) const;
    void SetUniform(int loc, float value) const;
    void SetUniform(int loc, const glm::vec2& value) const;
    void SetUniform(int loc, const glm::vec3& value) const;
    void SetUniform(int loc, const glm::vec4& value) const;
    void SetUniform(int loc, const glm::mat4& value) const;

    template <typename T>
    void SetAttrib(const std::string& name, T* values, size_t stride = 0) const
    {
        auto iter = attribs.find(name);
        if (iter != attribs.end())
        {
            SetAttrib(iter->second.loc, values, stride);
        }
        else
        {
            std::cerr << "could not find attrib " << name << " for program " << name << std::endl;
        }
    }

    void SetAttrib(int loc, float* values, size_t stride = 0) const;
    void SetAttrib(int loc, glm::vec2* values, size_t stride = 0) const;
    void SetAttrib(int loc, glm::vec3* values, size_t stride = 0) const;
    void SetAttrib(int loc, glm::vec4* values, size_t stride = 0) const;

    int program;
    int vertShader;
    int fragShader;

    struct Variable
    {
        int size;
        uint32_t type;
        int loc;
    };

    std::unordered_map<std::string, Variable> uniforms;
    std::unordered_map<std::string, Variable> attribs;
    std::string debugName;
};

#endif
