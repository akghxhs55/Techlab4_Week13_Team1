#pragma once

#include "Containers/Array.h"
#include "Core/Memory.h"
#include "Math/Vector.h"
#include "ClothBridge.h"

struct FPhysicsSettings;
struct FBodyInstance;

namespace physx {
	class PxScene;
}

class UWorld;
class USkeletalMeshComponent;
class UClothComponent;
class FPhysContactEventCallback;

struct FPhysicsContactImpact
{
	USkeletalMeshComponent* TargetComponent = nullptr;
	const FBodyInstance* HitBody = nullptr;
	FVector WorldLocation = FVector::ZeroVector;
	FVector WorldNormal = FVector(0.0f, 0.0f, 1.0f);
	FVector WorldImpulse = FVector::ZeroVector;
	FVector WorldSlideVelocity = FVector::ZeroVector;
	float ImpulseMagnitude = 0.0f;
};

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
	void RegisterClothComponent(UClothComponent* Component);
	void UnregisterClothComponent(UClothComponent* Component);
	void QueueContactImpact(const FPhysicsContactImpact& Impact);

private:
	void SyncComponentsToBodies();
	void DispatchContactImpacts();

	UWorld* Owner = nullptr;
	TUniquePtr<FPhysContactEventCallback> ContactCallback;
	physx::PxScene* Scene = nullptr;
	FClothSolverHandle ClothSolver = nullptr;
	float FrameDeltaTime = 0.0f;
	float SubstepDeltaTime = 0.0f;
	int32 NumSubsteps = 0;
	bool bPhysXSceneExecuting = false;
	TArray<USkeletalMeshComponent*> SimulatingSkeletalMeshes;
	TArray<UClothComponent*> ClothComponents;
	TArray<FPhysicsContactImpact> PendingContactImpacts;
};
