#include "magiccarpet.h"

#include "debugdraw.h"
#include "log.h"
#include "util.h"

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

MagicCarpet::MagicCarpet(const glm::mat4& roomMatIn) : roomMat(roomMatIn)
{
}

void MagicCarpet::Process(const Pose& headPose, const Pose& leftPose, const Pose& rightPose,
                          const glm::vec2& leftStick, const glm::vec2& rightStick,
                          const ButtonState& buttonState, float dt)
{
    glm::mat4 leftMat = leftPose.GetMat();
    glm::mat4 rightMat = rightPose.GetMat();
    DebugDraw_Transform(leftMat);
    DebugDraw_Transform(rightMat);

    // debug sticks
    glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    DebugDraw_Line(leftPose.pos, XformPoint(leftMat, glm::vec3(leftStick, 0.0f)), white);
    DebugDraw_Line(rightPose.pos, XformPoint(rightMat, glm::vec3(rightStick, 0.0f)), white);
}

void MagicCarpet::SetRoomMat(const glm::mat4& roomMatIn)
{
    roomMat = roomMatIn;
}
