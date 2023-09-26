#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include <string>
#include <vector>

class PointCloud
{
public:
    PointCloud();

    bool ImportPly(const std::string& plyFilename);

protected:

    struct Point
    {
        float position[3];
        float normal[3];
        uint8_t color[3];
    };

    std::vector<Point> pointVec;
};

#endif
