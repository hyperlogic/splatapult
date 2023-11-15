#ifndef MAGICCARPET_H
#define MAGICCARPET_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <string>

#include "core/texture.h"
#include "core/program.h"
#include "core/vertexbuffer.h"

// VR flycam
class MagicCarpet
{
public:
    MagicCarpet(const glm::mat4& carpetMatIn, float moveSpeedIn);

    bool Init();

    struct Pose
    {
        Pose() : pos(), rot(), posValid(false), posTracked(false), rotValid(false), rotTracked(false) {}
        glm::mat4 GetMat() const;
        void Dump(const std::string& name) const;
        glm::vec3 pos;
        glm::quat rot;
        bool posValid;
        bool posTracked;
        bool rotValid;
        bool rotTracked;
    };

    struct ButtonState
    {
        ButtonState() : leftTrigger(false), rightTrigger(false), leftGrip(false), rightGrip(false) {}
        bool leftTrigger;
        bool rightTrigger;
        bool leftGrip;
        bool rightGrip;
    };

    void Process(const Pose& headPose, const Pose& leftPose, const Pose& rightPose,
                 const glm::vec2& leftStick, const glm::vec2& rightStick,
                 const ButtonState& buttonState, float dt);

    const glm::mat4& GetCarpetMat() const { return carpetMat; }
    void SetCarpetMat(const glm::mat4& carpetMatIn);

    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);

protected:
    float snapTimer;
    float moveSpeed;
    enum class State { Normal, MoveGrab };
    State state;

    glm::vec3 grabPos;
    glm::quat grabRot;
    glm::mat4 grabCarpetMat;

    glm::mat4 carpetMat;

    std::shared_ptr<Texture> carpetTex;
    std::shared_ptr<Program> carpetProg;
    std::shared_ptr<VertexArrayObject> carpetVao;
};

#endif
