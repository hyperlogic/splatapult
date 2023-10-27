#include "magiccarpet.h"

#include <array>
#include <glm/gtc/matrix_transform.hpp>

#include "core/debugdraw.h"
#include "core/log.h"
#include "core/util.h"

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

MagicCarpet::MagicCarpet(const glm::mat4& carpetMatIn, float moveSpeedIn) :
    carpetMat(carpetMatIn),
    moveSpeed(moveSpeedIn),
    state(State::Normal)
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

    // debug draw sticks
    glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    DebugDraw_Line(glm::vec3(leftMat[3]), XformPoint(leftMat, glm::vec3(leftStick, 0.0f)), white);
    DebugDraw_Line(glm::vec3(rightMat[3]), XformPoint(rightMat, glm::vec3(rightStick, 0.0f)), white);

    if (state == State::Normal &&
        (buttonState.leftGrip && buttonState.rightGrip) &&
        leftPose.posValid && leftPose.rotValid &&
        rightPose.posValid && leftPose.rotValid)
    {
        // enter grab state
        state = State::Grab;

        // capture state of hand controllers
        //leftGrabPose = leftPose;
        //rightGrabPose = rightPose;

        // capture averge state of hand controllers
        grabPos = glm::mix(leftPose.pos, rightPose.pos, 0.5f);
        grabRot = SafeMix(leftPose.rot, rightPose.rot, 0.5f);
        grabCarpetMat = carpetMat;
    }
    else if (state == State::Grab && (!buttonState.leftGrip || !buttonState.rightGrip))
    {
        // exit grab state
        state = State::Normal;
    }

    if (state == State::Normal)
    {
        // get the forward and right vectors of the HMD
        glm::vec3 headForward = headPose.rot * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 headRight = headPose.rot * glm::vec3(1.0f, 0.0f, 0.0f);

        // project the HMD forward & right vectors onto the carpet, i.e. make sure they lie in the horizontal plane
        // AJT: TODO: Handle bad normalize
        glm::vec3 horizForward = glm::normalize(glm::vec3(headForward.x, 0.0f, headForward.z));
        glm::vec3 horizRight = glm::normalize(glm::vec3(headRight.x, 0.0f, headRight.z));

        // use leftStick to move horizontally
        glm::vec3 horizVel = horizForward * leftStick.y * moveSpeed + horizRight * leftStick.x * moveSpeed;

        // handle snap turns
        snapTimer -= dt;
        if (fabs(rightStick.x) > 0.5f && snapTimer < 0.0f)
        {
            // snap!
            float snapSign = rightStick.x > 0.0f ? -1.0f : 1.0f;

            // Rotate the carpet around the users HMD
            glm::vec3 snapPos = XformPoint(carpetMat, headPose.pos);
            glm::quat snapRot = glm::angleAxis(snapSign * SNAP_ANGLE, XformVec(carpetMat, glm::vec3(0.0f, 1.0f, 0.0f)));
            carpetMat = MakeRotateAboutPointMat(snapPos, snapRot) * carpetMat;

            snapTimer = SNAP_TIME;
        }
        else if (fabs(rightStick.x) < 0.2f)
        {
            // reset snap
            snapTimer = 0.0f;
        }

        // move the carpet!
        glm::vec3 vel = XformVec(carpetMat, horizVel);
        carpetMat[3][0] += vel.x * dt;
        carpetMat[3][1] += vel.y * dt;
        carpetMat[3][2] += vel.z * dt;
    }
    else if (state == State::Grab)
    {
        // debug draw a line between the hand controllers
        DebugDraw_Line(XformPoint(carpetMat, leftPose.pos), XformPoint(carpetMat, rightPose.pos), white);
        glm::mat4 grabMat = MakeMat4(grabRot, grabPos);
        DebugDraw_Transform(carpetMat * grabMat);

        // capture averge state of hand controllers
        glm::vec3 pos = glm::mix(leftPose.pos, rightPose.pos, 0.5f);
        glm::quat rot = SafeMix(leftPose.rot, rightPose.rot, 0.5f);
        glm::mat4 currMat = MakeMat4(rot, pos);
        DebugDraw_Transform(carpetMat * currMat);

        // adjust the carpet mat
        carpetMat = grabCarpetMat * grabMat * glm::inverse(currMat);
    }

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
