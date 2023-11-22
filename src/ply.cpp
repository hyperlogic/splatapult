#include "ply.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "core/log.h"

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

bool Ply::Parse(std::ifstream& plyFile)
{
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

    size_t offset = 0;
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

        std::istringstream iss(line);
        iss >> token1 >> token2 >> token3;
        if (token1 != "property")
        {
            Log::E("Invalid header, expected property\n");
            return false;
        }
        if (token2 == "char" || token2 == "int8")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 1, Ply::Type::Char}));
            offset += 1;
        }
        else if (token2 == "uchar" || token2 == "uint8")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 1, Ply::Type::UChar}));
            offset += 1;
        }
        else if (token2 == "short" || token2 == "int16")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 2, Ply::Type::Short}));
            offset += 2;
        }
        else if (token2 == "ushort" || token2 == "uint16")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 2, Ply::Type::UShort}));
            offset += 2;
        }
        else if (token2 == "int" || token2 == "int32")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 4, Ply::Type::Int}));
            offset += 4;
        }
        else if (token2 == "uint" || token2 == "uint32")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 4, Ply::Type::UInt}));
            offset += 4;
        }
        else if (token2 == "float" || token2 == "float32")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 4, Ply::Type::Float}));
            offset += 4;
        }
        else if (token2 == "double" || token2 == "float64")
        {
            propertyMap.insert(std::pair<std::string, Property>(token3, {offset, 8, Ply::Type::Double}));
            offset += 8;
        }
        else
        {
            Log::E("Unsupported type \"%s\" for property \"%s\"\n", token2.c_str(), token3.c_str());
            return false;
        }
    }

    vertexSize = offset;

    // read rest of file into dataVec
    dataVec.resize(vertexSize * vertexCount);
    plyFile.read((char*)dataVec.data(), vertexSize * vertexCount);

    return true;
}

bool Ply::GetProperty(const std::string& key, Ply::Property& propertyOut) const
{
    auto iter = propertyMap.find(key);
    if (iter != propertyMap.end())
    {
        propertyOut = iter->second;
        return true;
    }
    return false;
}

void Ply::ForEachVertex(const VertexCallback& cb)
{
    const uint8_t* vertexPtr = dataVec.data();
    for (size_t i = 0; i < vertexCount; i++)
    {
        cb(vertexPtr, vertexSize);
        vertexPtr += vertexSize;
    }
}
