#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/Transform.h"

class USkeletalMeshComponent;
class USceneComponent;
class UPhysicsAsset;
class FPhysScene;
class FLineBatcher;
namespace physx { class PxRigidStatic; }

// 프리뷰 씬에서만 물리를 진행합니다. 에셋 설정과 런타임 래그돌 구현은 변경하지 않습니다.
class FPhysicsAssetSimulation
{
public:
	FPhysicsAssetSimulation() = default;
	FPhysicsAssetSimulation(const FPhysicsAssetSimulation&) = delete;
	FPhysicsAssetSimulation& operator=(const FPhysicsAssetSimulation&) = delete;
	~FPhysicsAssetSimulation();
	bool Start(USkeletalMeshComponent& Component, UPhysicsAsset& Asset, FString& OutError);
	void Stop();
	void Tick(float DeltaTime);
	void TogglePause() { if (bActive) { bPaused = !bPaused; bStepRequested = false; } }
	void Step() { if (bActive && bPaused) bStepRequested = true; }
	bool IsActive() const { return bActive; }
	bool IsPaused() const { return bActive && bPaused; }
	float GetElapsedTime() const { return ElapsedTime; }
	void DrawFloor(FLineBatcher& Lines) const;

private:
	USkeletalMeshComponent* MeshComponent = nullptr;
	USceneComponent* TransformTarget = nullptr;
	FPhysScene* PhysicsScene = nullptr;
	physx::PxRigidStatic* Floor = nullptr;
	TArray<FTransform> SavedPose;
	FTransform SavedTargetTransform;
	FTransform SavedMeshTransform;
	FVector FloorCenter = FVector::ZeroVector;
	float FloorHalfExtent = 1.0f;
	float ElapsedTime = 0.0f;
	bool bActive = false;
	bool bPaused = false;
	bool bStepRequested = false;
};
