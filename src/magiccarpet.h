#ifndef MAGICCARPET_H
#define MAGICCARPET_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>

// VR flycam
class MagicCarpet
{
public:
    MagicCarpet(const glm::mat4& carpetMatIn, float moveSpeedIn);

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
protected:
    float snapTimer;
    float moveSpeed;
    enum class State { Normal, Grab };
    State state;

    glm::vec3 grabPos;
    glm::quat grabRot;
    glm::mat4 grabCarpetMat;

    glm::mat4 carpetMat;
};

#endif
