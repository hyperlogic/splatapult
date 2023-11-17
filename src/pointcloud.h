#pragma once

#include <string>
#include <vector>

class PointCloud
{
public:
    PointCloud();

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    struct Point
    {
        float position[3];
        float normal[3];
        uint8_t color[3];
    };

    size_t size() const { return pointVec.size(); }
    const std::vector<Point>& GetPointVec() const { return pointVec; }

    // AJT: HACK FOR NOW give full access to pointVec
    std::vector<Point>& GetPointVec() { return pointVec; }

protected:

    std::vector<Point> pointVec;
};
