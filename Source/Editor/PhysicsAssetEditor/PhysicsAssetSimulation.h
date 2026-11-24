#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/Transform.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class USceneComponent;
class UPhysicsAsset;
class FPhysScene;

// 프리뷰 씬에서만 물리를 진행합니다. 에셋 설정과 런타임 래그돌 구현은 변경하지 않습니다.
class FPhysicsAssetSimulation
{
public:
	FPhysicsAssetSimulation() = default;
	FPhysicsAssetSimulation(const FPhysicsAssetSimulation&) = delete;
	FPhysicsAssetSimulation& operator=(const FPhysicsAssetSimulation&) = delete;
	~FPhysicsAssetSimulation();
	bool Start(USkeletalMeshComponent& Component, UStaticMeshComponent& PreviewFloor, UPhysicsAsset& Asset, FString& OutError);
	void Stop();
	void Tick(float DeltaTime);
	bool IsActive() const { return bActive; }

private:
	USkeletalMeshComponent* MeshComponent = nullptr;
	UStaticMeshComponent* FloorComponent = nullptr;
	USceneComponent* TransformTarget = nullptr;
	FPhysScene* PhysicsScene = nullptr;
	TArray<FTransform> SavedPose;
	FTransform SavedTargetTransform;
	FTransform SavedMeshTransform;
	bool bActive = false;
};
