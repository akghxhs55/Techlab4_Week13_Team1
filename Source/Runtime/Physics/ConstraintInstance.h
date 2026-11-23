#pragma once

#include "Core/EngineString.h"
#include "Math/Transform.h"

class FBodyInstance;
namespace physx { class PxD6Joint; }

enum class EConstraintAngularMotion : uint8
{
	Locked,
	Limited,
	Free
};

struct FConstraintInstance
{
	FString ConstraintBone1;
	FString ConstraintBone2;
	FTransform Frame1;
	FTransform Frame2;
	FRotator JointFrameRotation;
	float TwistLimitRadians = 0.785398f;
	float TwistLowerRadians = -0.785398f;
	float TwistUpperRadians = 0.785398f;
	float Swing1LimitRadians = 0.523599f;
	float Swing2LimitRadians = 0.523599f;
	EConstraintAngularMotion TwistMotion = EConstraintAngularMotion::Limited;
	EConstraintAngularMotion Swing1Motion = EConstraintAngularMotion::Limited;
	EConstraintAngularMotion Swing2Motion = EConstraintAngularMotion::Limited;
	bool bDisableCollision = true;
	bool bUseBoneOriginFrames = true;

	bool InitConstraint(FBodyInstance& Child, FBodyInstance& Parent);
	void UpdateConstraintFrames(FBodyInstance& Child, FBodyInstance& Parent);
	void TermConstraint();
	bool IsValid() const { return Joint != nullptr; }

private:
	physx::PxD6Joint* Joint = nullptr;
};
