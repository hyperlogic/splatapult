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

#include "core/binaryattribute.h"

class Ply
{
public:
    Ply();
    bool Parse(std::ifstream& plyFile);

    bool GetProperty(const std::string& key, BinaryAttribute& attributeOut) const;
    void AddProperty(const std::string& key, BinaryAttribute::Type type);
    void AllocData(size_t numVertices);

    using VertexCallback = std::function<void(const uint8_t*, size_t)>;
    void ForEachVertex(const VertexCallback& cb) const;

    size_t GetVertexCount() const { return vertexCount; }

protected:
    bool ParseHeader(std::ifstream& plyFile);

    std::unordered_map<std::string, BinaryAttribute> propertyMap;
    std::unique_ptr<uint8_t> data;
    size_t vertexCount;
    size_t vertexSize;
};
