#pragma once

#include <memory>
#include "Containers/Array.h"

struct FPhysicsSettings;
class UWorld;
class UPrimitiveComponent;

namespace physx {
	class PxScene;
	class PxSimulationEventCallback;
}

// 1. 이벤트 데이터 구조체 (Hit용, Overlap용)
struct FPendingHitEvent
{
	UPrimitiveComponent* CompA = nullptr;
	UPrimitiveComponent* CompB = nullptr;
};

struct FPendingTriggerEvent
{
	UPrimitiveComponent* TriggerComp = nullptr;
	UPrimitiveComponent* OtherComp = nullptr;
	bool bIsBegin = true; // true: BeginOverlap, false: EndOverlap
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

	void AddPendingHit(UPrimitiveComponent* A, UPrimitiveComponent* B);
	void AddPendingTrigger(UPrimitiveComponent* TriggerComp, UPrimitiveComponent* OtherComp, bool bIsBegin);

private:
	void SyncComponentsToBodies();
	void DispatchPendingEvents();

	UWorld* Owner = nullptr;
	physx::PxScene* Scene = nullptr;
	float FrameDeltaTime = 0.0f;
	float SubstepDeltaTime = 0.0f;
	int32 NumSubsteps = 0;
	bool bPhysXSceneExecuting = false;

	// PhysX 콜백 수신기 (PIMPL로 헤더 의존성 최소화)
	std::unique_ptr<physx::PxSimulationEventCallback> SimEventCallback;

	TArray<FPendingHitEvent> PendingHitEvents;
	TArray<FPendingTriggerEvent> PendingTriggerEvents;
};
