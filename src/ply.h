/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <cassert>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Ply
{
public:
    bool Parse(std::ifstream& plyFile);

    enum class Type
    {
        Unknown,
        Char,
        UChar,
        Short,
        UShort,
        Int,
        UInt,
        Float,
        Double
    };

    struct Property
    {
        Property() : type(Type::Unknown) {}
        Property(size_t offsetIn, size_t sizeIn, Type typeIn) : offset(offsetIn), size(sizeIn), type(typeIn) {}

        size_t offset;
        size_t size;
        Type type;

        template <typename T>
        const T Get(const uint8_t* data) const
        {
            if (type == Type::Unknown)
            {
                return 0;
            }
            else
            {
                assert(size == sizeof(T));
                return *reinterpret_cast<const T*>(data + offset);
            }
        }
    };

    bool GetProperty(const std::string& key, Property& propertyOut) const;

    using VertexCallback = std::function<void(const uint8_t*, size_t)>;
    void ForEachVertex(const VertexCallback& cb);

    size_t GetVertexCount() const { return vertexCount; }

protected:
    std::unordered_map<std::string, Property> propertyMap;
    std::vector<uint8_t> dataVec;
    size_t vertexCount;
    size_t vertexSize;
};
