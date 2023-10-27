#include "magiccarpet.h"

#include <array>

#include "debugdraw.h"
#include "log.h"
#include "util.h"

const float SNAP_TIME = 1.0f;
const float SNAP_ANGLE = glm::radians(30.0f);

const float CARPET_RADIUS = 2.0f;
const size_t NUM_CORNERS = 4;
static std::array<glm::vec3, NUM_CORNERS> carpetCorners = {
    glm::vec3(-CARPET_RADIUS, 0.0f, -CARPET_RADIUS),
    glm::vec3(CARPET_RADIUS, 0.0f, -CARPET_RADIUS),
    glm::vec3(CARPET_RADIUS, 0.0f, CARPET_RADIUS),
    glm::vec3(-CARPET_RADIUS, 0.0f, CARPET_RADIUS)
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

MagicCarpet::MagicCarpet(const glm::mat4& carpetMatIn, float moveSpeedIn) : carpetMat(carpetMatIn), moveSpeed(moveSpeedIn)
{
}

void MagicCarpet::Process(const Pose& headPose, const Pose& leftPose, const Pose& rightPose,
                          const glm::vec2& leftStick, const glm::vec2& rightStick,
                          const ButtonState& buttonState, float dt)
{
    glm::mat4 leftMat = carpetMat * leftPose.GetMat();
    glm::mat4 rightMat = carpetMat * rightPose.GetMat();
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
    glm::vec3 carpetVel = hForward * leftStick.y * moveSpeed + hRight * leftStick.x * moveSpeed;

    snapTimer -= dt;
    Log::printf("snapTimer = %.5f!\n", snapTimer);
    if (fabs(rightStick.x) > 0.5f && snapTimer < 0.0f)
    {
        // snap
        float snapSign = rightStick.x > 0.0f ? -1.0f : 1.0f;
        glm::vec3 snapPos = XformPoint(carpetMat, headPose.pos);
        glm::quat snapRot = glm::angleAxis(snapSign * SNAP_ANGLE, XformVec(carpetMat, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::mat4 snapPosMat = MakeMat4(glm::quat(), snapPos);
        glm::mat4 invSnapPosMat = MakeMat4(glm::quat(), -snapPos);
        glm::mat4 snapRotMat = MakeMat4(snapRot);
        carpetMat = snapPosMat * snapRotMat * invSnapPosMat * carpetMat;
        snapTimer = SNAP_TIME;
        Log::printf("SNAP!\n");
    }
    else if (fabs(rightStick.x) < 0.2f)
    {
        // reset snap
        snapTimer = 0.0f;
    }

    // move the carpet!
    glm::vec3 vel = XformVec(carpetMat, carpetVel);
    carpetMat[3][0] += vel.x * dt;
    carpetMat[3][1] += vel.y * dt;
    carpetMat[3][2] += vel.z * dt;

    // draw the carpet
    std::array<glm::vec3, NUM_CORNERS> worldCorners;
    for (int i = 0; i < carpetCorners.size(); i++)
    {
        worldCorners[i] = XformPoint(carpetMat, carpetCorners[i]);
    }
    DebugDraw_Line(worldCorners[0], worldCorners[1], white);
    DebugDraw_Line(worldCorners[1], worldCorners[2], white);
    DebugDraw_Line(worldCorners[2], worldCorners[3], white);
    DebugDraw_Line(worldCorners[3], worldCorners[0], white);
    DebugDraw_Line(worldCorners[0], worldCorners[2], white);
    DebugDraw_Line(worldCorners[1], worldCorners[3], white);
}

void MagicCarpet::SetCarpetMat(const glm::mat4& carpetMatIn)
{
    carpetMat = carpetMatIn;
}
