#include "magiccarpet.h"

#include <array>

#include "debugdraw.h"
#include "log.h"
#include "util.h"

const float ROOM_RADIUS = 2.0f;
const size_t NUM_CORNERS = 4;
static std::array<glm::vec3, NUM_CORNERS> roomCorners = {
    glm::vec3(-ROOM_RADIUS, 0.0f, -ROOM_RADIUS),
    glm::vec3(ROOM_RADIUS, 0.0f, -ROOM_RADIUS),
    glm::vec3(ROOM_RADIUS, 0.0f, ROOM_RADIUS),
    glm::vec3(-ROOM_RADIUS, 0.0f, ROOM_RADIUS)
};

glm::mat4 MagicCarpet::Pose::GetMat() const
{
    return MakeMat4(rot, pos);
}

void MagicCarpet::Pose::Dump(const std::string& name) const
{
    PrintVec(pos, name + ".pos");
    Log::printf("%s.posValid = %s, %s.posTracked = %s\n", name.c_str(), posValid ? "true" : "false", name.c_str(), posTracked ? "true" : "false");
    PrintQuat(rot, name + ".rot");
    Log::printf("%s.rotValid = %s, %s.rotTracked = %s\n", name.c_str(), rotValid ? "true" : "false", name.c_str(), rotTracked ? "true" : "false");
}

MagicCarpet::MagicCarpet(const glm::mat4& roomMatIn, float moveSpeedIn) : roomMat(roomMatIn), moveSpeed(moveSpeedIn)
{
}

void MagicCarpet::Process(const Pose& headPose, const Pose& leftPose, const Pose& rightPose,
                          const glm::vec2& leftStick, const glm::vec2& rightStick,
                          const ButtonState& buttonState, float dt)
{
    glm::mat4 leftMat = roomMat * leftPose.GetMat();
    glm::mat4 rightMat = roomMat * rightPose.GetMat();
    DebugDraw_Transform(leftMat);
    DebugDraw_Transform(rightMat);

    // debug sticks
    glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    DebugDraw_Line(glm::vec3(leftMat[3]), XformPoint(leftMat, glm::vec3(leftStick, 0.0f)), white);
    DebugDraw_Line(glm::vec3(rightMat[3]), XformPoint(rightMat, glm::vec3(rightStick, 0.0f)), white);

    // use leftStick +y to move forward in the horizontal forward direction
    glm::vec3 forward = headPose.rot * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right = headPose.rot * glm::vec3(1.0f, 0.0f, 0.0f);
    // TODO: safe normalize
    glm::vec3 hForward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
    glm::vec3 hRight = glm::normalize(glm::vec3(right.x, 0.0f, right.z));

    glm::vec3 roomVel = XformVec(roomMat, hForward * leftStick.y * moveSpeed + hRight * leftStick.x * moveSpeed);

    // move the room!
    roomMat[3][0] += roomVel.x * dt;
    roomMat[3][1] += roomVel.y * dt;
    roomMat[3][2] += roomVel.z * dt;

    // draw floor
    std::array<glm::vec3, NUM_CORNERS> worldCorners;
    for (int i = 0; i < roomCorners.size(); i++)
    {
        worldCorners[i] = XformPoint(roomMat, roomCorners[i]);
    }
    DebugDraw_Line(worldCorners[0], worldCorners[1], white);
    DebugDraw_Line(worldCorners[1], worldCorners[2], white);
    DebugDraw_Line(worldCorners[2], worldCorners[3], white);
    DebugDraw_Line(worldCorners[3], worldCorners[0], white);
    DebugDraw_Line(worldCorners[0], worldCorners[2], white);
    DebugDraw_Line(worldCorners[1], worldCorners[3], white);
}

void MagicCarpet::SetRoomMat(const glm::mat4& roomMatIn)
{
    roomMat = roomMatIn;
}
