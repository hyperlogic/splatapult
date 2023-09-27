#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include <string>
#include <vector>

class PointCloud
{
public:
    PointCloud();

    bool ImportPly(const std::string& plyFilename);

    struct Point
    {
        float position[3];
        float normal[3];
        uint8_t color[3];
    };

    const std::vector<Point>& GetPointVec() const { return pointVec; }

    // AJT: HACK FOR NOW give full access to pointVec
    std::vector<Point>& GetPointVec() { return pointVec; }

protected:

    std::vector<Point> pointVec;
};

#endif
