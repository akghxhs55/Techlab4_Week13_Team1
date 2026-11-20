#pragma once

#include "Core/EngineString.h"
#include "Math/Transform.h"

class FBodyInstance;
namespace physx { class PxD6Joint; }

struct FConstraintInstance
{
	FString ConstraintBone1;
	FString ConstraintBone2;
	FTransform Frame1;
	FTransform Frame2;
	float TwistLimitRadians = 0.785398f;
	float Swing1LimitRadians = 0.523599f;
	float Swing2LimitRadians = 0.523599f;
	bool bDisableCollision = true;
	bool bUseBoneOriginFrames = true;

	bool InitConstraint(FBodyInstance& Child, FBodyInstance& Parent);
	void UpdateConstraintFrames(FBodyInstance& Child, FBodyInstance& Parent);
	void TermConstraint();
	bool IsValid() const { return Joint != nullptr; }

private:
	physx::PxD6Joint* Joint = nullptr;
};
