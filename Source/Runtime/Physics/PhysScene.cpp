#include "EnginePCH.h"
#include "PhysScene.h"

#include <PxPhysics.h>
#include <PxScene.h>
#include <PxSceneDesc.h>
#include <extensions/PxDefaultSimulationFilterShader.h>

#include "BodyInstance.h"
#include "ClothCore.h"
#include "PhysicsCore.h"
#include "PhysicsSettings.h"
#include "Components/ClothComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

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

	physx::PxSceneDesc SceneDesc{ Physics->getTolerancesScale() };
	SceneDesc.gravity = physx::PxVec3{ 0.0f, 0.0f, -9.81f };
	SceneDesc.cpuDispatcher = FPhysicsCore::GetCpuDispatcher();
	SceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
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
