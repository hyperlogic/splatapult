/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "ply.h"

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/log.h"

static uint32_t propertyTypeSizeArr[(size_t)BinaryAttribute::Type::NumTypes] = {
    0, // Unknown
    1, // Char
    1, // UChar
    2, // Short
    2, // UShort
    4, // Int
    4, // UInt
    4, // Float
    8 // Double
};

static bool CheckLine(std::ifstream& plyFile, const std::string& validLine)
{
    std::string line;
    return std::getline(plyFile, line) && line == validLine;
}

static bool GetNextPlyLine(std::ifstream& plyFile, std::string& lineOut)
{
    while (std::getline(plyFile, lineOut))
    {
        // skip comment lines
        if (lineOut.find("comment", 0) != 0)
        {
            return true;
        }
    }
    return false;
}

Ply::Ply() : vertexCount(0), vertexSize(0)
{
    ;
}

bool Ply::ParseHeader(std::ifstream& plyFile)
{
    ZoneScopedNC("Ply::ParseHeader", tracy::Color::Green);

    // validate start of header
    std::string token1, token2, token3;

    // check header starts with "ply".
    if (!GetNextPlyLine(plyFile, token1))
    {
        Log::E("Unexpected error reading next line\n");
        return false;
    }
    if (token1 != "ply")
    {
        Log::E("Invalid ply file\n");
        return false;
    }

    // check format
    if (!GetNextPlyLine(plyFile, token1))
    {
        Log::E("Unexpected error reading next line\n");
        return false;
    }
    if (token1 != "format binary_little_endian 1.0" && token1 != "format binary_big_endian 1.0")
    {
        Log::E("Invalid ply file, expected format\n");
        return false;
    }
    if (token1 != "format binary_little_endian 1.0")
    {
        Log::E("Unsupported ply file, only binary_little_endian supported\n");
        return false;
    }

    // parse "element vertex {number}"
    std::string line;
    if (!GetNextPlyLine(plyFile, line))
    {
        Log::E("Unexpected error reading next line\n");
        return false;
    }
    std::istringstream iss(line);
    if (!((iss >> token1 >> token2 >> vertexCount) && (token1 == "element") && (token2 == "vertex")))
    {
        Log::E("Invalid ply file, expected \"element vertex {number}\"\n");
        return false;
    }

    // TODO: support other "element" types faces, edges etc?
    // at the moment I only care about ply files with vertex elements.

    while (true)
    {
        if (!GetNextPlyLine(plyFile, line))
        {
            Log::E("unexpected error reading line\n");
            return false;
        }

        if (line == "end_header")
        {
            break;
        }

        iss.str(line);
        iss.clear();
        iss >> token1 >> token2 >> token3;
        if (token1 != "property")
        {
            Log::E("Invalid header, expected property\n");
            return false;
        }
        if (token2 == "char" || token2 == "int8")
        {
            AddProperty(token3, BinaryAttribute::Type::Char);
        }
        else if (token2 == "uchar" || token2 == "uint8")
        {
            AddProperty(token3, BinaryAttribute::Type::UChar);
        }
        else if (token2 == "short" || token2 == "int16")
        {
            AddProperty(token3, BinaryAttribute::Type::Short);
        }
        else if (token2 == "ushort" || token2 == "uint16")
        {
            AddProperty(token3, BinaryAttribute::Type::UShort);
        }
        else if (token2 == "int" || token2 == "int32")
        {
            AddProperty(token3, BinaryAttribute::Type::Int);
        }
        else if (token2 == "uint" || token2 == "uint32")
        {
            AddProperty(token3, BinaryAttribute::Type::UInt);
        }
        else if (token2 == "float" || token2 == "float32")
        {
            AddProperty(token3, BinaryAttribute::Type::Float);
        }
        else if (token2 == "double" || token2 == "float64")
        {
            AddProperty(token3, BinaryAttribute::Type::Double);
        }
        else
        {
            Log::E("Unsupported type \"%s\" for property \"%s\"\n", token2.c_str(), token3.c_str());
            return false;
        }
    }

    return true;
}

bool Ply::Parse(std::ifstream& plyFile)
{
    if (!ParseHeader(plyFile))
    {
        return false;
    }

    // read rest of file into data ptr
    {
        ZoneScopedNC("Ply::Parse() read data", tracy::Color::Yellow);
        AllocData(vertexCount);
        plyFile.read((char*)data.get(), vertexSize * vertexCount);
    }

    return true;
}

bool Ply::GetProperty(const std::string& key, BinaryAttribute& binaryAttributeOut) const
{
    auto iter = propertyMap.find(key);
    if (iter != propertyMap.end())
    {
        binaryAttributeOut = iter->second;
        return true;
    }
    return false;
}

void Ply::AddProperty(const std::string& key, BinaryAttribute::Type type)
{
    using PropInfoPair = std::pair<std::string, BinaryAttribute>;

    uint32_t propSize = propertyTypeSizeArr[(int)type];
    propertyMap.emplace(PropInfoPair(key, BinaryAttribute{type, propSize, (uint32_t)vertexSize}));
    vertexSize += propSize;
}

void Ply::AllocData(size_t numVertices)
{
    data.reset(new uint8_t[vertexSize * numVertices]);
}

void Ply::ForEachVertex(const VertexCallback& cb) const
{
    const uint8_t* ptr = data.get();
    for (size_t i = 0; i < vertexCount; i++)
    {
        cb(ptr, vertexSize);
        ptr += vertexSize;
    }
}

