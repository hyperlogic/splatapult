#include "magiccarpet.h"

#include <array>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/debugdraw.h"
#include "core/image.h"
#include "core/log.h"
#include "core/util.h"

const float SNAP_TIME = 1.0f;
const float SNAP_ANGLE = glm::radians(30.0f);

const float GRAB_SWITCH_TIME = 1.0f;

const float CARPET_RADIUS = 3.0f;
const float CARPET_TILE_COUNT = 3.0f;

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
    sm(State::Normal),
    carpetMat(carpetMatIn),
    moveSpeed(moveSpeedIn)
{
    // Normal state
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

    // Left Grip
    sm.AddState(State::LeftGrip, "LeftGrip",
                [this]() { grabRot = in.leftPose.rot; grabPos = in.leftPose.pos; grabCarpetMat = carpetMat; },
                [this]() {},
                [this](float dt)
                {
                    glm::mat4 grabMat = MakeMat4(grabRot, grabPos);
                    glm::vec3 pos = in.leftPose.pos;
                    glm::quat rot = grabRot;
                    glm::mat4 currMat = MakeMat4(rot, pos);

                    // adjust the carpet mat
                    carpetMat = grabCarpetMat * grabMat * glm::inverse(currMat);
                });
    sm.AddTransition(State::LeftGrip, State::Normal, "leftGrip up", [this]()
    {
        return !in.buttonState.leftGrip;
    });

    // Right Grip
    sm.AddState(State::RightGrip, "RightGrip",
                [this]() { grabRot = in.rightPose.rot; grabPos = in.rightPose.pos; grabCarpetMat = carpetMat; },
                [this]() {},
                [this](float dt)
                {
                    glm::mat4 grabMat = MakeMat4(grabRot, grabPos);
                    glm::vec3 pos = in.rightPose.pos;
                    glm::quat rot = grabRot;
                    glm::mat4 currMat = MakeMat4(rot, pos);

                    // adjust the carpet mat
                    carpetMat = grabCarpetMat * grabMat * glm::inverse(currMat);
                });
    sm.AddTransition(State::RightGrip, State::Normal, "rightGrip up", [this]()
    {
        return !in.buttonState.rightGrip;
    });
}

bool MagicCarpet::Init()
{
    Image carpetImg;
    if (!carpetImg.Load("texture/carpet.png"))
    {
        Log::printf("Error loading carpet.png\n");
        return false;
    }
    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::Repeat, WrapType::Repeat};
    carpetTex = std::make_shared<Texture>(carpetImg, texParams);

    carpetProg = std::make_shared<Program>();
    if (!carpetProg->LoadVertFrag("shader/carpet_vert.glsl", "shader/carpet_frag.glsl"))
    {
        Log::printf("Error loading carpet shaders!\n");
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
    // get the forward and right vectors of the HMD
    glm::vec3 headForward = in.headPose.rot * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 headRight = in.headPose.rot * glm::vec3(1.0f, 0.0f, 0.0f);

    // project the HMD forward & right vectors onto the carpet, i.e. make sure they lie in the horizontal plane
    // AJT: TODO: Handle bad normalize
    glm::vec3 horizForward = glm::normalize(glm::vec3(headForward.x, 0.0f, headForward.z));
    glm::vec3 horizRight = glm::normalize(glm::vec3(headRight.x, 0.0f, headRight.z));

    // use leftStick to move horizontally
    glm::vec3 horizVel = horizForward * in.leftStick.y * moveSpeed + horizRight * in.leftStick.x * moveSpeed;

    // handle snap turns
    snapTimer -= dt;
    if (fabs(in.rightStick.x) > 0.5f && snapTimer < 0.0f)
    {
        // snap!
        float snapSign = in.rightStick.x > 0.0f ? -1.0f : 1.0f;

        // Rotate the carpet around the users HMD
        glm::vec3 snapPos = XformPoint(carpetMat, in.headPose.pos);
        glm::quat snapRot = glm::angleAxis(snapSign * SNAP_ANGLE, XformVec(carpetMat, glm::vec3(0.0f, 1.0f, 0.0f)));
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
