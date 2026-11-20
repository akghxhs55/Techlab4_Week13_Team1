#include "EnginePCH.h"
#include "ConstraintInstance.h"

#include "BodyInstance.h"
#include "PhysicsCore.h"
#include "PhysXConversion.h"
#include <extensions/PxD6JointCreate.h>
#include <extensions/PxD6Joint.h>
#include <extensions/PxJointLimit.h>
#include <PxRigidDynamic.h>

bool FConstraintInstance::InitConstraint(FBodyInstance& Child, FBodyInstance& Parent)
{
	TermConstraint();
	auto* ChildActor = Child.GetPxRigidDynamic();
	auto* ParentActor = Parent.GetPxRigidDynamic();
	auto* Physics = FPhysicsCore::GetPhysics();
	if (!Physics || !ChildActor || !ParentActor)
		return false;

	const physx::PxTransform ChildFrame = bUseBoneOriginFrames
		? physx::PxTransform(physx::PxIdentity) : K2PTransform(Frame1);
	const physx::PxTransform ParentFrame = bUseBoneOriginFrames
		? ParentActor->getGlobalPose().getInverse() * ChildActor->getGlobalPose()
		: K2PTransform(Frame2);
	Joint = physx::PxD6JointCreate(*Physics, ChildActor, ChildFrame, ParentActor, ParentFrame);
	if (!Joint)
		return false;

	Joint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
	Joint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
	Joint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
	Joint->setTwistLimit(physx::PxJointAngularLimitPair(-TwistLimitRadians, TwistLimitRadians));
	Joint->setSwingLimit(physx::PxJointLimitCone(Swing1LimitRadians, Swing2LimitRadians));
	Joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, !bDisableCollision);
	return true;
}

void FConstraintInstance::UpdateConstraintFrames(FBodyInstance& Child, FBodyInstance& Parent)
{
	if (!Joint || !bUseBoneOriginFrames || !Child.GetPxRigidDynamic() || !Parent.GetPxRigidDynamic())
		return;
	Joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform(physx::PxIdentity));
	Joint->setLocalPose(physx::PxJointActorIndex::eACTOR1,
		Parent.GetPxRigidDynamic()->getGlobalPose().getInverse() * Child.GetPxRigidDynamic()->getGlobalPose());
}

void FConstraintInstance::TermConstraint()
{
	if (Joint)
	{
		Joint->release();
		Joint = nullptr;
	}
}
