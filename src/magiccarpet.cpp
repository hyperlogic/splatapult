/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "magiccarpet.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#endif

#include <array>
#include <glm/gtc/matrix_transform.hpp>

#include "core/image.h"
#include "core/log.h"
#include "core/util.h"

const float SNAP_TIME = 1.0f;
const float SNAP_ANGLE = glm::radians(30.0f);

const float DOUBLE_GRIP_TIME = 0.1f;

const float CARPET_RADIUS = 3.0f;
const float CARPET_TILE_COUNT = 3.0f;

glm::mat4 MagicCarpet::Pose::GetMat() const
{
    return MakeMat4(rot, pos);
}

void MagicCarpet::Pose::Dump(const std::string& name) const
{
    PrintVec(pos, name + ".pos");
    Log::D("%s.posValid = %s, %s.posTracked = %s\n", name.c_str(), posValid ? "true" : "false", name.c_str(), posTracked ? "true" : "false");
    PrintQuat(rot, name + ".rot");
    Log::D("%s.rotValid = %s, %s.rotTracked = %s\n", name.c_str(), rotValid ? "true" : "false", name.c_str(), rotTracked ? "true" : "false");
}

MagicCarpet::MagicCarpet(const glm::mat4& carpetMatIn, float moveSpeedIn) :
    sm(State::Normal),
    carpetMat(carpetMatIn),
    moveSpeed(moveSpeedIn)
{
    // Normal
    sm.AddState(State::Normal, "Normal",
                [this]() { snapTimer = 0.0f; },  // enter
                [this]() {}, // exit
                [this](float dt) { NormalProcess(dt); });  // process
    sm.AddTransition(State::Normal, State::LeftGrip, "leftGrip down", [this]()
    {
        return in.buttonState.leftGrip;
    });
    sm.AddTransition(State::Normal, State::RightGrip, "rightGrip down", [this]()
    {
        return in.buttonState.rightGrip;
    });

    // LeftGrip
    sm.AddState(State::LeftGrip, "LeftGrip",
                [this]() { GrabPoses(); },
                [this]() {},
                [this](float dt)
                {
                    if (GripCount() == 1)
                    {
                        gripTimer = DOUBLE_GRIP_TIME;
                    }
                    gripTimer -= dt;
                    glm::mat4 grabMat = MakeMat4(grabLeftPose.rot, grabLeftPose.pos);
                    glm::vec3 pos = in.leftPose.pos;
                    glm::quat rot = grabLeftPose.rot;
                    glm::mat4 currMat = MakeMat4(rot, pos);

                    // adjust the carpet mat
                    carpetMat = grabCarpetMat * grabMat * glm::inverse(currMat);
                });
    sm.AddTransition(State::LeftGrip, State::Normal, "leftGrip up", [this]()
    {
        return !in.buttonState.leftGrip;
    });
    sm.AddTransition(State::LeftGrip, State::DoubleGrip, "double grip", [this]()
    {
        return GripCount() == 2 && gripTimer < 0.0f;
    });

    // RightGrip
    sm.AddState(State::RightGrip, "RightGrip",
                [this]() { GrabPoses(); },
                [this]() {},
                [this](float dt)
                {
                    if (GripCount() == 1)
                    {
                        gripTimer = DOUBLE_GRIP_TIME;
                    }
                    gripTimer -= dt;
                    glm::mat4 grabMat = MakeMat4(grabRightPose.rot, grabRightPose.pos);
                    glm::vec3 pos = in.rightPose.pos;
                    glm::quat rot = grabRightPose.rot;
                    glm::mat4 currMat = MakeMat4(rot, pos);

                    // adjust the carpet mat
                    carpetMat = grabCarpetMat * grabMat * glm::inverse(currMat);
                });
    sm.AddTransition(State::RightGrip, State::Normal, "rightGrip up", [this]()
    {
        return !in.buttonState.rightGrip;
    });
    sm.AddTransition(State::RightGrip, State::DoubleGrip, "double grip", [this]()
    {
        return GripCount() == 2 && gripTimer < 0.0f;
    });

    // DoubleGrip
    sm.AddState(State::DoubleGrip, "DoubleGrip",
                [this]() { GrabPoses(); scaleMode = (TriggerCount() > 0); },
                [this]() {},
                [this](float dt)
                {
                    glm::vec3 d0 = grabRightPose.pos - grabLeftPose.pos;
                    glm::vec3 p0 = glm::mix(grabLeftPose.pos, grabRightPose.pos, 0.5f);
                    glm::vec3 x0 = SafeNormalize(d0, glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::vec3 y0 =  glm::vec3(0.0f, 1.0f, 0.0f);
                    glm::vec3 z0 = glm::normalize(glm::cross(x0, y0));
                    y0 = glm::normalize(glm::cross(z0, x0));
                    glm::mat4 grabMat(glm::vec4(x0, 0.0f), glm::vec4(y0, 0.0f), glm::vec4(z0, 0.0f), glm::vec4(p0, 1.0f));

                    glm::vec3 d1 = in.rightPose.pos - in.leftPose.pos;
                    glm::vec3 p1 = glm::mix(in.leftPose.pos, in.rightPose.pos, 0.5f);
                    glm::vec3 x1 = SafeNormalize(d1, glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::vec3 y1 = glm::vec3(0.0f, 1.0f, 0.0f);
                    glm::vec3 z1 = glm::normalize(glm::cross(x1, y1));
                    y1 = glm::normalize(glm::cross(z1, x1));
                    glm::mat4 currMat(glm::vec4(x1, 0.0f), glm::vec4(y1, 0.0f), glm::vec4(z1, 0.0f), glm::vec4(p1, 1.0f));
                    float s1 = glm::length(d1) / glm::length(d0);

                    if (scaleMode)
                    {
                        currMat *= MakeMat4(s1, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
                    }

                    // adjust the carpet mat
                    carpetMat = grabCarpetMat * grabMat * glm::inverse(currMat);
                });
    sm.AddTransition(State::DoubleGrip, State::LeftGrip, "grip count 1", [this]()
    {
        return GripCount() == 1 && !in.buttonState.rightGrip;
    });
    sm.AddTransition(State::DoubleGrip, State::RightGrip, "grip count 1", [this]()
    {
        return GripCount() == 1 && !in.buttonState.leftGrip;
    });
    sm.AddTransition(State::DoubleGrip, State::Normal, "grip count 0", [this]()
    {
        return GripCount() == 0;
    });
}

bool MagicCarpet::Init(bool isFramebufferSRGBEnabledIn)
{
    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;

    Image carpetImg;
    if (!carpetImg.Load("texture/carpet.png"))
    {
        Log::E("Error loading carpet.png\n");
        return false;
    }
    carpetImg.isSRGB = isFramebufferSRGBEnabledIn;
    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::Repeat, WrapType::Repeat};
    carpetTex = std::make_shared<Texture>(carpetImg, texParams);

    carpetProg = std::make_shared<Program>();
    if (!carpetProg->LoadVertFrag("shader/carpet_vert.glsl", "shader/carpet_frag.glsl"))
    {
        Log::E("Error loading carpet shaders!\n");
        return false;
    }

    carpetVao = std::make_shared<VertexArrayObject>();
    std::vector<glm::vec3> posVec = {
        glm::vec3(-CARPET_RADIUS, 0.0f, -CARPET_RADIUS),
        glm::vec3(CARPET_RADIUS, 0.0f, -CARPET_RADIUS),
        glm::vec3(CARPET_RADIUS, 0.0f, CARPET_RADIUS),
        glm::vec3(-CARPET_RADIUS, 0.0f, CARPET_RADIUS)
    };
    auto posBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    std::vector<glm::vec2> uvVec = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(CARPET_TILE_COUNT, 0.0f),
        glm::vec2(CARPET_TILE_COUNT, CARPET_TILE_COUNT),
        glm::vec2(0.0f, CARPET_TILE_COUNT)
    };
    auto uvBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, uvVec);

    // build element array
    std::vector<uint32_t> indexVec = {
        0, 2, 1,
        0, 3, 2
    };
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec);

    // setup vertex array object with buffers
    carpetVao->SetAttribBuffer(carpetProg->GetAttribLoc("position"), posBuffer);
    carpetVao->SetAttribBuffer(carpetProg->GetAttribLoc("uv"), uvBuffer);
    carpetVao->SetElementBuffer(indexBuffer);

    return true;
}

void MagicCarpet::Process(const Pose& headPose, const Pose& leftPose, const Pose& rightPose,
                          const glm::vec2& leftStick, const glm::vec2& rightStick,
                          const ButtonState& buttonState, float dt)
{
    /*
    glm::mat4 leftMat = carpetMat * leftPose.GetMat();
    glm::mat4 rightMat = carpetMat * rightPose.GetMat();
    DebugDraw_Transform(leftMat);
    DebugDraw_Transform(rightMat);
    */

    in.headPose = headPose;
    in.leftPose = leftPose;
    in.rightPose = rightPose;
    in.leftStick = leftStick;
    in.rightStick = rightStick;
    in.buttonState = buttonState;

    sm.Process(dt);
}

void MagicCarpet::SetCarpetMat(const glm::mat4& carpetMatIn)
{
    carpetMat = carpetMatIn;
}

void MagicCarpet::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                         const glm::vec4& viewport, const glm::vec2& nearFar)
{
    carpetProg->Bind();

    glm::mat4 modelViewMat = glm::inverse(cameraMat) * carpetMat;
    carpetProg->SetUniform("modelViewProjMat", projMat * modelViewMat);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, carpetTex->texture);
    carpetProg->SetUniform("colorTex", 0);
    carpetVao->DrawElements(GL_TRIANGLES);
}

void MagicCarpet::NormalProcess(float dt)
{
    glm::vec3 horizVel;
    if (in.headPose.rotValid)
    {
        // get the forward and right vectors of the HMD
        glm::vec3 headForward = in.headPose.rot * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 headRight = in.headPose.rot * glm::vec3(1.0f, 0.0f, 0.0f);

        // project the HMD forward & right vectors onto the carpet, i.e. make sure they lie in the horizontal plane
        // AJT: TODO: Handle bad normalize
        glm::vec3 horizForward = glm::normalize(glm::vec3(headForward.x, 0.0f, headForward.z));
        glm::vec3 horizRight = glm::normalize(glm::vec3(headRight.x, 0.0f, headRight.z));

        // use leftStick to move horizontally
        horizVel = horizForward * in.leftStick.y * moveSpeed + horizRight * in.leftStick.x * moveSpeed;
    }
    else
    {
        horizVel = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    // handle snap turns
    snapTimer -= dt;
    if (fabs(in.rightStick.x) > 0.5f && snapTimer < 0.0f && in.headPose.posValid && in.headPose.posTracked)
    {
        // snap!
        float snapSign = in.rightStick.x > 0.0f ? -1.0f : 1.0f;

        // Rotate the carpet around the users HMD
        glm::vec3 snapPos = XformPoint(carpetMat, in.headPose.pos);
        glm::quat snapRot = glm::angleAxis(snapSign * SNAP_ANGLE, glm::normalize(XformVec(carpetMat, glm::vec3(0.0f, 1.0f, 0.0f))));
        carpetMat = MakeRotateAboutPointMat(snapPos, snapRot) * carpetMat;

        snapTimer = SNAP_TIME;
    }
    else if (fabs(in.rightStick.x) < 0.2f)
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

void MagicCarpet::GrabPoses()
{
    grabLeftPose = in.leftPose;
    grabRightPose = in.rightPose;
    grabCarpetMat = carpetMat;
}

int MagicCarpet::GripCount() const
{
    int count = 0;
    count += (in.buttonState.leftGrip) ? 1 : 0;
    count += (in.buttonState.rightGrip) ? 1 : 0;
    return count;
}

int MagicCarpet::TriggerCount() const
{
    int count = 0;
    count += (in.buttonState.leftTrigger) ? 1 : 0;
    count += (in.buttonState.rightTrigger) ? 1 : 0;
    return count;
}
