#include "EnginePCH.h"
#include "PhysScene.h"

#include <PxPhysics.h>
#include <PxRigidDynamic.h>
#include <PxScene.h>
#include <PxSceneDesc.h>
#include <PxSimulationEventCallback.h>
#include <extensions/PxDefaultSimulationFilterShader.h>

#include "BodyInstance.h"
#include "ClothCore.h"
#include "PhysicsCore.h"
#include "PhysicsSettings.h"
#include "PhysXConversion.h"
#include "Components/ClothComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// 기본 충돌 필터에 접촉 지점과 충격량 알림 플래그를 함께 활성화
	physx::PxFilterFlags KurisuContactFilterShader(
		physx::PxFilterObjectAttributes Attributes0,
		physx::PxFilterData FilterData0,
		physx::PxFilterObjectAttributes Attributes1,
		physx::PxFilterData FilterData1,
		physx::PxPairFlags& PairFlags,
		const void* ConstantBlock,
		physx::PxU32 ConstantBlockSize)
	{
		const physx::PxFilterFlags Flags = physx::PxDefaultSimulationFilterShader(
			Attributes0, FilterData0, Attributes1, FilterData1, PairFlags, ConstantBlock, ConstantBlockSize);
		if (PairFlags.isSet(physx::PxPairFlag::eSOLVE_CONTACT))
		{
			PairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND
				| physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS
				| physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;
		}
		return Flags;
	}
}

class FPhysContactEventCallback : public physx::PxSimulationEventCallback
{
public:
	explicit FPhysContactEventCallback(FPhysScene* InScene)
		: OwnerScene(InScene)
	{
	}

	void onConstraintBreak(physx::PxConstraintInfo* Constraints, physx::PxU32 Count) override
	{
		(void)Constraints;
		(void)Count;
	}

	void onWake(physx::PxActor** Actors, physx::PxU32 Count) override
	{
		(void)Actors;
		(void)Count;
	}

	void onSleep(physx::PxActor** Actors, physx::PxU32 Count) override
	{
		(void)Actors;
		(void)Count;
	}

	void onTrigger(physx::PxTriggerPair* Pairs, physx::PxU32 Count) override
	{
		(void)Pairs;
		(void)Count;
	}

	void onAdvance(const physx::PxRigidBody* const* BodyBuffer, const physx::PxTransform* PoseBuffer, const physx::PxU32 Count) override
	{
		(void)BodyBuffer;
		(void)PoseBuffer;
		(void)Count;
	}

	void onContact(const physx::PxContactPairHeader& PairHeader, const physx::PxContactPair* Pairs, physx::PxU32 NbPairs) override
	{
		if (!OwnerScene ||
			PairHeader.flags.isSet(physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0) ||
			PairHeader.flags.isSet(physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
		{
			return;
		}

		physx::PxActor* Actor0 = PairHeader.actors[0];
		physx::PxActor* Actor1 = PairHeader.actors[1];
		if (!Actor0 || !Actor1)
		{
			return;
		}

		const FBodyInstance* Body0 = static_cast<const FBodyInstance*>(Actor0->userData);
		const FBodyInstance* Body1 = static_cast<const FBodyInstance*>(Actor1->userData);
		const bool bSkel0 = Body0 && Body0->bIsSkeletalBody && Body0->OwnerComponent;
		const bool bSkel1 = Body1 && Body1->bIsSkeletalBody && Body1->OwnerComponent;
		if (!bSkel0 && !bSkel1)
		{
			return;
		}

		// 동일 스켈레탈 컴포넌트 내부 본끼리의 자가 접촉은 제외
		if (bSkel0 && bSkel1 && Body0->OwnerComponent == Body1->OwnerComponent)
		{
			return;
		}

		const physx::PxRigidDynamic* Dyn0 = Actor0->is<physx::PxRigidDynamic>();
		const physx::PxRigidDynamic* Dyn1 = Actor1->is<physx::PxRigidDynamic>();
		const FVector Vel0 = Dyn0 ? P2KVector(Dyn0->getLinearVelocity()) : FVector::ZeroVector;
		const FVector Vel1 = Dyn1 ? P2KVector(Dyn1->getLinearVelocity()) : FVector::ZeroVector;

		physx::PxContactPairPoint ContactPoints[16];
		for (physx::PxU32 p = 0; p < NbPairs; ++p)
		{
			const physx::PxContactPair& Pair = Pairs[p];
			if (Pair.flags.isSet(physx::PxContactPairFlag::eREMOVED_SHAPE_0) ||
				Pair.flags.isSet(physx::PxContactPairFlag::eREMOVED_SHAPE_1))
			{
				continue;
			}

			const physx::PxU32 PointCount = Pair.extractContacts(ContactPoints, 16);
			if (PointCount == 0)
			{
				continue;
			}

			physx::PxVec3 TotalImpulse(0.0f, 0.0f, 0.0f);
			physx::PxVec3 BestPos = ContactPoints[0].position;
			physx::PxVec3 BestNormal = ContactPoints[0].normal;
			float BestPointImpulseSq = -1.0f;

			for (physx::PxU32 c = 0; c < PointCount; ++c)
			{
				TotalImpulse += ContactPoints[c].impulse;
				const float MagSq = ContactPoints[c].impulse.magnitudeSquared();
				if (MagSq > BestPointImpulseSq)
				{
					BestPointImpulseSq = MagSq;
					BestPos = ContactPoints[c].position;
					BestNormal = ContactPoints[c].normal;
				}
			}

			const float ImpulseMag = TotalImpulse.magnitude();
			if (ImpulseMag <= 1e-4f)
			{
				continue;
			}

			const FVector WorldPos = P2KVector(BestPos);
			const FVector WorldNormal = P2KVector(BestNormal);
			const FVector WorldImpulse = P2KVector(TotalImpulse);

			if (bSkel0)
			{
				if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Body0->OwnerComponent))
				{
					const FVector RelVel = Vel0 - Vel1;
					const FVector SlideVel = RelVel - WorldNormal * RelVel.Dot(WorldNormal);
					FPhysicsContactImpact Impact{};
					Impact.TargetComponent = SkelComp;
					Impact.HitBody = Body0;
					Impact.WorldLocation = WorldPos;
					Impact.WorldNormal = WorldNormal;
					Impact.WorldImpulse = WorldImpulse;
					Impact.WorldSlideVelocity = SlideVel;
					Impact.ImpulseMagnitude = ImpulseMag;
					OwnerScene->QueueContactImpact(Impact);
				}
			}

			if (bSkel1)
			{
				if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Body1->OwnerComponent))
				{
					const FVector RelVel = Vel1 - Vel0;
					const FVector InvNormal = WorldNormal * -1.0f;
					const FVector SlideVel = RelVel - InvNormal * RelVel.Dot(InvNormal);
					FPhysicsContactImpact Impact{};
					Impact.TargetComponent = SkelComp;
					Impact.HitBody = Body1;
					Impact.WorldLocation = WorldPos;
					Impact.WorldNormal = InvNormal;
					Impact.WorldImpulse = WorldImpulse * -1.0f;
					Impact.WorldSlideVelocity = SlideVel;
					Impact.ImpulseMagnitude = ImpulseMag;
					OwnerScene->QueueContactImpact(Impact);
				}
			}
		}
	}

private:
	FPhysScene* OwnerScene = nullptr;
};

FPhysScene::FPhysScene(UWorld* InOwner)
	: Owner(InOwner)
{
	if (FClothCore::GetFactory())
	{
		ClothSolver = ClothBridge_CreateSolver(FClothCore::GetFactory());
	}

	physx::PxPhysics* Physics = FPhysicsCore::GetPhysics();
	if (!Physics)
	{
		return;
	}

	ContactCallback = MakeUnique<FPhysContactEventCallback>(this);

	physx::PxSceneDesc SceneDesc{ Physics->getTolerancesScale() };
	SceneDesc.gravity = physx::PxVec3{ 0.0f, 0.0f, -9.81f };
	SceneDesc.cpuDispatcher = FPhysicsCore::GetCpuDispatcher();
	SceneDesc.filterShader = KurisuContactFilterShader;
	SceneDesc.simulationEventCallback = ContactCallback.get();
	SceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	SceneDesc.userData = this;

	if (!SceneDesc.isValid())
	{
		KRS_LOG(Error, "[PhysX] Invalid PxSceneDesc");
		return;
	}

	Scene = Physics->createScene(SceneDesc);
	if (!Scene)
	{
		KRS_LOG(Error, "[PhysX] Failed to create PxScene");
	}
}

FPhysScene::~FPhysScene()
{
	if (ClothSolver)
	{
		for (UClothComponent* ClothComp : ClothComponents)
		{
			if (ClothComp)
			{
				ClothBridge_RemoveClothFromSolver(ClothSolver, ClothComp->GetClothHandle());
			}
		}
		ClothComponents.Reset();
		ClothBridge_DestroySolver(ClothSolver);
		ClothSolver = nullptr;
	}

	if (Scene)
	{
		if (bPhysXSceneExecuting)
		{
			Scene->fetchResults(true);
		}
		Scene->release();
		Scene = nullptr;
	}
	PendingContactImpacts.Reset();
	ContactCallback.reset();
}

void FPhysScene::SetUpForFrame(float DeltaSeconds, const FPhysicsSettings& Settings)
{
	assert(!bPhysXSceneExecuting);

	FrameDeltaTime = 0.0f;
	SubstepDeltaTime = 0.0f;
	NumSubsteps = 0;

	if (!Scene || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const physx::PxVec3 NewGravity(0.0f, 0.0f, Settings.DefaultGravityZ);
	if (Scene->getGravity() != NewGravity)
	{
		Scene->setGravity(NewGravity);
	}

	const bool bUseSubstep = Settings.bSubstepping
		&& Settings.MaxSubstepDeltaTime > 0.0f
		&& Settings.MaxSubsteps > 0;

	if (bUseSubstep)
	{
		FrameDeltaTime = std::min(DeltaSeconds, Settings.MaxSubstepDeltaTime * static_cast<float>(Settings.MaxSubsteps));

		const int32 Needed = static_cast<int32>(std::ceil(FrameDeltaTime / Settings.MaxSubstepDeltaTime));
		NumSubsteps = std::clamp(Needed, 1, Settings.MaxSubsteps);
		SubstepDeltaTime = FrameDeltaTime / static_cast<float>(NumSubsteps);
	}
	else
	{
		FrameDeltaTime = std::min(DeltaSeconds, Settings.MaxPhysicsDeltaTime);
		NumSubsteps = 1;
		SubstepDeltaTime = FrameDeltaTime;
	}
}

void FPhysScene::StartFrame()
{
	if (ClothSolver && FrameDeltaTime > 0.0f)
	{
		for (UClothComponent* ClothComp : ClothComponents)
		{
			if (ClothComp && ClothComp->IsRegistered())
			{
				ClothComp->PreClothSimulate();
			}
		}
	}

	if (!Scene || NumSubsteps == 0)
	{
		return;
	}

	Scene->simulate(SubstepDeltaTime);
	bPhysXSceneExecuting = true;
}

void FPhysScene::EndFrame()
{
	if (!bPhysXSceneExecuting)
	{
		return;
	}

	Scene->fetchResults(true);
	for (int32 i = 1; i < NumSubsteps; ++i)
	{
		Scene->simulate(SubstepDeltaTime);
		Scene->fetchResults(true);
	}
	bPhysXSceneExecuting = false;
	SyncComponentsToBodies();
	for (USkeletalMeshComponent* Component : SimulatingSkeletalMeshes)
		if (Component && Component->IsRegistered())
			Component->SyncPhysicsToBones();
	DispatchContactImpacts();

	if (ClothSolver && FrameDeltaTime > 0.0f && !ClothComponents.IsEmpty())
	{
		ClothBridge_StepSolver(ClothSolver, FrameDeltaTime);
		for (UClothComponent* ClothComp : ClothComponents)
		{
			if (ClothComp && ClothComp->IsRegistered())
			{
				ClothComp->PostClothSimulate();
			}
		}
	}
}

void FPhysScene::RegisterSimulatingSkeletalMesh(USkeletalMeshComponent* Component)
{
	if (Component && !SimulatingSkeletalMeshes.Contains(Component))
		SimulatingSkeletalMeshes.Add(Component);
}

void FPhysScene::UnregisterSimulatingSkeletalMesh(USkeletalMeshComponent* Component)
{
	SimulatingSkeletalMeshes.Remove(Component);
}

void FPhysScene::RegisterClothComponent(UClothComponent* Component)
{
	if (!Component || !ClothSolver || !Component->GetClothHandle())
	{
		return;
	}
	if (!ClothComponents.Contains(Component))
	{
		ClothComponents.Add(Component);
		ClothBridge_AddClothToSolver(ClothSolver, Component->GetClothHandle());
	}
}

void FPhysScene::UnregisterClothComponent(UClothComponent* Component)
{
	if (!Component)
	{
		return;
	}
	if (ClothComponents.Contains(Component))
	{
		if (ClothSolver && Component->GetClothHandle())
		{
			ClothBridge_RemoveClothFromSolver(ClothSolver, Component->GetClothHandle());
		}
		ClothComponents.Remove(Component);
	}
}

void FPhysScene::QueueContactImpact(const FPhysicsContactImpact& Impact)
{
	PendingContactImpacts.Add(Impact);
}

void FPhysScene::DispatchContactImpacts()
{
	if (PendingContactImpacts.IsEmpty())
	{
		return;
	}

	TArray<FPhysicsContactImpact> Impacts = std::move(PendingContactImpacts);
	PendingContactImpacts.Reset();

	for (int32 i = 0; i < Impacts.Num(); ++i)
	{
		USkeletalMeshComponent* Target = Impacts[i].TargetComponent;
		if (!Target || !Target->IsRegistered() || !SimulatingSkeletalMeshes.Contains(Target))
		{
			continue;
		}
		Target->NotifyPhysicsContactImpact(Impacts[i]);
	}
}

void FPhysScene::SyncComponentsToBodies()
{
	physx::PxU32 Num = 0;
	physx::PxActor** Actors = Scene->getActiveActors(Num);
	for (physx::PxU32 i = 0; i < Num; ++i)
	{
		auto* Body = static_cast<FBodyInstance*>(Actors[i]->userData);
		if (Body && Body->OwnerComponent && !Body->bIsSkeletalBody)
		{
			Body->OwnerComponent->SyncComponentToRBPhysics();
		}
	}
}
