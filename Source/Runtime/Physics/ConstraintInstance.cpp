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
		? physx::PxTransform(physx::PxVec3(0.0f), K2PQuat(JointFrameRotation.Quaternion())) : K2PTransform(Frame1);
	const physx::PxTransform ParentFrame = bUseBoneOriginFrames
		? ParentActor->getGlobalPose().getInverse() * ChildActor->getGlobalPose() * ChildFrame
		: K2PTransform(Frame2);

	Joint = physx::PxD6JointCreate(*Physics, ChildActor, ChildFrame, ParentActor, ParentFrame);
	if (!Joint)
		return false;

	auto ToPxMotion = [](EConstraintAngularMotion Motion)
	{
		switch (Motion)
		{
		case EConstraintAngularMotion::Locked: return physx::PxD6Motion::eLOCKED;
		case EConstraintAngularMotion::Free: return physx::PxD6Motion::eFREE;
		default: return physx::PxD6Motion::eLIMITED;
		}
	};
	Joint->setMotion(physx::PxD6Axis::eTWIST, ToPxMotion(TwistMotion)); // 조인트 로컬 X축을 중심으로 비트는 회전
	Joint->setMotion(physx::PxD6Axis::eSWING1, ToPxMotion(Swing1Motion)); // 조인트 로컬 Y축 기울어지는 회전
	Joint->setMotion(physx::PxD6Axis::eSWING2, ToPxMotion(Swing2Motion)); // 조인트 로컬 Z축 기울어지는 회전
	Joint->setTwistLimit(physx::PxJointAngularLimitPair(TwistLowerRadians, TwistUpperRadians)); // 여기서 정한 각도까지만 회전 허용
	Joint->setSwingLimit(physx::PxJointLimitCone(Swing1LimitRadians, Swing2LimitRadians));
	Joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, !bDisableCollision); // 조인트로 연결된 두 바디끼리 충돌할지 설정
	return true;
}

void FConstraintInstance::UpdateConstraintFrames(FBodyInstance& Child, FBodyInstance& Parent) // 애니메이션 자세로 바디를 옮긴 뒤 래그돌을 시작할 때, 조인트가 옛 자세의 연결점을 계속 사용하지 않도록 갱신
{
	if (!Joint || !bUseBoneOriginFrames || !Child.GetPxRigidDynamic() || !Parent.GetPxRigidDynamic())
		return;
	const physx::PxTransform ChildFrame(physx::PxVec3(0.0f), K2PQuat(JointFrameRotation.Quaternion()));
	Joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, ChildFrame);
	Joint->setLocalPose(physx::PxJointActorIndex::eACTOR1,
		Parent.GetPxRigidDynamic()->getGlobalPose().getInverse() * Child.GetPxRigidDynamic()->getGlobalPose() * ChildFrame);
}

void FConstraintInstance::TermConstraint()
{
	if (Joint)
	{
		Joint->release();
		Joint = nullptr;
	}
}
