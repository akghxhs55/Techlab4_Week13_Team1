#pragma once

#include "Containers/Array.h"

struct FPhysicsSettings;

namespace physx {
	class PxScene;
}

class UWorld;
class USkeletalMeshComponent;

class FPhysScene
{
public:
	explicit FPhysScene(UWorld* InOwner);
	~FPhysScene();

	void SetUpForFrame(float DeltaSeconds, const FPhysicsSettings& Settings);
	void StartFrame();
	void EndFrame();

	physx::PxScene* GetPxScene() const { return Scene; }
	UWorld* GetOwningWorld() const { return Owner; }
	void RegisterSimulatingSkeletalMesh(USkeletalMeshComponent* Component);
	void UnregisterSimulatingSkeletalMesh(USkeletalMeshComponent* Component);

private:
	void SyncComponentsToBodies();

	UWorld* Owner = nullptr;
	physx::PxScene* Scene = nullptr;
	float FrameDeltaTime = 0.0f;
	float SubstepDeltaTime = 0.0f;
	int32 NumSubsteps = 0;
	bool bPhysXSceneExecuting = false;
	TArray<USkeletalMeshComponent*> SimulatingSkeletalMeshes;
};
