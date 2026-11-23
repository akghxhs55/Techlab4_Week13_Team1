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

class FPhysScene;

// 2. 멀티스레드 씬 락 (Scoped Lock)
class FPhysSceneReadLock
{
public:
	explicit FPhysSceneReadLock(FPhysScene* InScene, const char* InFile = nullptr, uint32 InLine = 0);
	explicit FPhysSceneReadLock(FPhysScene& InScene, const char* InFile = nullptr, uint32 InLine = 0);
	~FPhysSceneReadLock();

	FPhysSceneReadLock(const FPhysSceneReadLock&) = delete;
	FPhysSceneReadLock& operator=(const FPhysSceneReadLock&) = delete;

private:
	FPhysScene* PhysScene = nullptr;
};

class FPhysSceneWriteLock
{
public:
	explicit FPhysSceneWriteLock(FPhysScene* InScene, const char* InFile = nullptr, uint32 InLine = 0);
	explicit FPhysSceneWriteLock(FPhysScene& InScene, const char* InFile = nullptr, uint32 InLine = 0);
	~FPhysSceneWriteLock();

	FPhysSceneWriteLock(const FPhysSceneWriteLock&) = delete;
	FPhysSceneWriteLock& operator=(const FPhysSceneWriteLock&) = delete;

private:
	FPhysScene* PhysScene = nullptr;
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

	// ---- 멀티스레드 락 인터페이스 ----
	void LockRead(const char* File = nullptr, uint32 Line = 0);
	void UnlockRead();
	void LockWrite(const char* File = nullptr, uint32 Line = 0);
	void UnlockWrite();

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
