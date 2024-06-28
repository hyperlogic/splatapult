/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once


#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Ply
{
public:
    Ply();
    bool Parse(std::ifstream& plyFile);

    enum class PropertyType : uint16_t
    {
        Unknown,
        Char,
        UChar,
        Short,
        UShort,
        Int,
        UInt,
        Float,
        Double,
        NumTypes
    };

    struct PropertyInfo
    {
        PropertyInfo() : type(PropertyType::Unknown) {}
        PropertyInfo(size_t offsetIn, size_t sizeIn, PropertyType typeIn, uint16_t indexIn) : offset(offsetIn), size(sizeIn), type(typeIn), index(indexIn) {}

        size_t offset;
        size_t size;
        PropertyType type;
        uint16_t index;

        template <typename T>
        const T Get(const uint8_t* data) const
        {
            if (type == PropertyType::Unknown)
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

    bool GetPropertyInfo(const std::string& key, PropertyInfo& propertyInfoOut) const;
    void AddPropertyInfo(const std::string& key, PropertyType type);
    void AllocData(size_t numVertices);

    using VertexCallback = std::function<void(const uint8_t*, size_t)>;
    void ForEachVertex(const VertexCallback& cb) const;

    size_t GetVertexCount() const { return vertexCount; }

protected:
    bool ParseHeader(std::ifstream& plyFile);

    std::unordered_map<std::string, PropertyInfo> propertyInfoMap;
    std::unique_ptr<uint8_t> data;
    size_t vertexCount;
    size_t vertexSize;
};
