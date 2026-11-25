#include "EnginePCH.h"
#include "BodyInstance.h"

#include "BodySetup.h"
#include "PhysicsCore.h"
#include "PhysXConversion.h"
#include "PhysScene.h"
#include "Render/Mesh.h"
#include "Components/ShapeComponent.h"
#include <algorithm>
#include <PxPhysics.h>
#include <PxRigidDynamic.h>
#include <PxScene.h>
#include <extensions/PxRigidActorExt.h>
#include <extensions/PxRigidBodyExt.h>

using namespace physx;

UBodySetup::~UBodySetup()
{
	// 전역 오브젝트 정리 순회 시 소유자 포인터 해제
	UObject* OuterObj = GetOuter();
	if (!OuterObj || std::find(GUObjectArray.begin(), GUObjectArray.end(), OuterObj) == GUObjectArray.end())
	{
		return;
	}

	if (UStaticMesh* OwnerMesh = Cast<UStaticMesh>(OuterObj))
	{
		OwnerMesh->ClearBodySetup(this);
	}
	else if (UShapeComponent* OwnerShape = Cast<UShapeComponent>(OuterObj))
	{
		OwnerShape->ClearShapeBodySetup(this);
	}
}

namespace
{
	constexpr float DefaultDensity = 1000.0f;

	const PxQuat SphylBasis(PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));

	FVector Abs(const FVector& V) { return { std::abs(V.X), std::abs(V.Y), std::abs(V.Z) }; }
	FVector Mul(const FVector& A, const FVector& B) { return { A.X * B.X, A.Y * B.Y, A.Z * B.Z }; }

	PxTransform MakeLocalPose(const FShapeElem& Elem, const FVector& Scale)
	{
		return { K2PVector(Mul(Elem.Center, Scale)), K2PQuat(Elem.Rotation.Quaternion().Normalized()) };
	}

	void ApplyCollisionFlags(PxShape* Shape, ECollisionEnabled Enabled)
	{
		const bool bSim = Enabled == ECollisionEnabled::PhysicsOnly || Enabled == ECollisionEnabled::QueryAndPhysics;
		const bool bQuery = Enabled == ECollisionEnabled::QueryOnly || Enabled == ECollisionEnabled::QueryAndPhysics;
		const bool bTrigger = Enabled == ECollisionEnabled::QueryOnly;

		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bSim);
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bQuery);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, bTrigger);
	}

	int32 CreateShapes(PxRigidActor& Actor, const FAggregateGeom& Geom, const FVector& InScale, ECollisionEnabled Enabled)
	{
		PxMaterial& Material = *FPhysicsCore::GetDefaultMaterial();
		const FVector S = Abs(InScale);
		int32 Count = 0;

		auto Attach = [&](const PxGeometry& Geometry, const PxTransform& LocalPose)
		{
			if (PxShape* Shape = PxRigidActorExt::createExclusiveShape(Actor, Geometry, Material))
			{
				Shape->setLocalPose(LocalPose);
				ApplyCollisionFlags(Shape, Enabled);
				++Count;
			}
		};

		for (const FSphereElem& Element : Geom.SphereElems)
		{
			const float Radius = Element.Radius * std::max({ S.X, S.Y, S.Z });
			Attach(PxSphereGeometry(Radius), MakeLocalPose(Element, InScale));
		}

		for (const FBoxElem& Element : Geom.BoxElems)
		{
			const PxVec3 HalfExtents(0.5f * Element.X * S.X, 0.5f * Element.Y * S.Y, 0.5f * Element.Z * S.Z);
			Attach(PxBoxGeometry(HalfExtents), MakeLocalPose(Element, InScale));
		}

		for (const FCapsuleElem& Element : Geom.CapsuleElems)
		{
			const float Radius = Element.Radius * FMath::Max(S.X, S.Y);
			const float HalfHeight = FMath::Max(0.0f, (0.5f * Element.Length + Element.Radius) * S.Z - Radius);
			PxTransform Pose = MakeLocalPose(Element, InScale);
			Pose.q = Pose.q * SphylBasis;
			Attach(PxCapsuleGeometry(Radius, HalfHeight), Pose);
		}

		return Count;
	}
}

void FBodyInstance::InitBody(UBodySetup* InBodySetup, const FTransform& WorldTransform, UPrimitiveComponent* Owner, FPhysScene* InScene, PxAggregate* Aggregate)
{
	if (RigidActor)
	{
		return;
	}

	PxPhysics* Physics = FPhysicsCore::GetPhysics();
	PxScene* Scene = InScene ? InScene->GetPxScene() : nullptr;
	if (!Physics || !Scene || !InBodySetup || !Owner)
	{
		return;
	}

	const PxTransform Pose{
		K2PVector(WorldTransform.Location),
		K2PQuat(WorldTransform.Rotation.Normalized()),
	};
	PxRigidDynamic* Dynamic = Physics->createRigidDynamic(Pose);
	if (!Dynamic)
	{
		KRS_LOG(Error, "[PhysX] createRigidDynamic failed");
		return;
	}
	Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !bSimulatePhysics); // 두번째 인자가 true면 물리에 의해 움직임, false면 게임 로직에 의해 움직임
	if (bIsSkeletalBody)
	{
		Dynamic->setSolverIterationCounts(12, 4);
		Dynamic->setMaxDepenetrationVelocity(2.0f);
		Dynamic->setMaxAngularVelocity(12.0f);
	}

	if (CreateShapes(*Dynamic, InBodySetup->AggGeom, WorldTransform.Scale, CollisionEnabled) == 0)
	{
		Dynamic->release();
		return;
	}

	if (MassInKgOverride > 0.0f)
	{
		PxRigidBodyExt::setMassAndUpdateInertia(*Dynamic, MassInKgOverride);
	}
	else
	{
		PxRigidBodyExt::updateMassAndInertia(*Dynamic, DefaultDensity);
	}

	Dynamic->setLinearDamping(LinearDamping); // 선속도 감쇠 계수
	Dynamic->setAngularDamping(AngularDamping); // 각속도 감쇠 계수
	Dynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !bEnableGravity);

	Dynamic->userData = this;

	if (Aggregate)
	{
		Aggregate->addActor(*Dynamic);
	}
	else
	{
		Scene->addActor(*Dynamic);
	}

	RigidActor = Dynamic;
	BodySetup = InBodySetup;
	OwnerComponent = Owner;
	PhysScene = InScene;
	Scale3D = WorldTransform.Scale;
}

void FBodyInstance::TermBody()
{
	if (RigidActor)
	{
		RigidActor->userData = nullptr;
		RigidActor->release();
		RigidActor = nullptr;
	}
	BodySetup = nullptr;
	OwnerComponent = nullptr;
	PhysScene = nullptr;
}

physx::PxRigidDynamic* FBodyInstance::GetPxRigidDynamic() const
{
	return RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
}

bool FBodyInstance::IsDynamic() const
{
	const PxRigidDynamic* Dynamic = GetPxRigidDynamic();
	return Dynamic && !(Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC);
}

void FBodyInstance::SetBodyTransform(const FTransform& WorldTransform, ETeleportType TeleportType)
{
	PxRigidDynamic* Dynamic = GetPxRigidDynamic();
	if (!Dynamic)
	{
		return;
	}

	const PxTransform Pose{
		K2PVector(WorldTransform.Location),
		K2PQuat(WorldTransform.Rotation.Normalized()),
	};
	const bool bKinematic = Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC;

	if (bKinematic && TeleportType == ETeleportType::None)
	{
		Dynamic->setKinematicTarget(Pose);
	}
	else
	{
		Dynamic->setGlobalPose(Pose);
	}
}

FTransform FBodyInstance::GetUnrealWorldTransform() const
{
	if (!RigidActor)
	{
		return FTransform::Identity;
	}
	
	const PxTransform Pose = RigidActor->getGlobalPose();
	return FTransform{
		P2KQuat(Pose.q),
		P2KVector(Pose.p), 
		Scale3D
	};
}

void FBodyInstance::SetInstanceSimulatePhysics(bool bSimulate)
{
	bSimulatePhysics = bSimulate;

	PxRigidDynamic* Dynamic = GetPxRigidDynamic();
	if (!Dynamic)
	{
		return;
	}

	Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !bSimulate);
	if (bSimulate && Dynamic->getScene())
	{
		Dynamic->wakeUp();
	}
}

void FBodyInstance::SetCollisionEnabled(ECollisionEnabled InCollisionEnabled)
{
	CollisionEnabled = InCollisionEnabled;
	if (!RigidActor)
		return;

	const PxU32 ShapeCount = RigidActor->getNbShapes();
	TArray<PxShape*> Shapes;
	Shapes.SetNum(static_cast<int32>(ShapeCount));
	RigidActor->getShapes(Shapes.GetData(), ShapeCount);
	for (PxShape* Shape : Shapes)
		ApplyCollisionFlags(Shape, CollisionEnabled);
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelocityChange)
{
	if (!IsDynamic())
	{
		return;
	}

	GetPxRigidDynamic()->addForce(K2PVector(Impulse), bVelocityChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE);
}

void FBodyInstance::AddForce(const FVector& Force)
{
	if (!IsDynamic())
	{
		return;
	}

	GetPxRigidDynamic()->addForce(K2PVector(Force), PxForceMode::eFORCE);
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity)
{
	if (IsDynamic())
	{
		GetPxRigidDynamic()->setLinearVelocity(K2PVector(Velocity));
	}
}

FVector FBodyInstance::GetLinearVelocity() const
{
	const PxRigidDynamic* Dynamic = GetPxRigidDynamic();
	return Dynamic ? P2KVector(Dynamic->getLinearVelocity()) : FVector::ZeroVector;
}
