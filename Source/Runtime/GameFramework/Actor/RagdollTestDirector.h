#pragma once

#include "GameFramework/Actor.h"
#include "RagdollTestDirector.generated.h"

class ASkeletalMeshActor;

// 래그돌 기능 테스트 씬을 진행한다. 씬의 SkeletalMeshActor를 이름 접두사로 찾아 역할을 나눈다.
// - DropPrefix: 시작하면 전체 래그돌로 떨어진다 (평지, 계단, 경사면, 모서리, 더미 정거장)
// - HangPrefix: HangRootBone은 키네마틱으로 두고 그 자식 본 아래만 시뮬레이션한다. 액터를 좌우로 흔들고 돌려 부분 래그돌을 보여준다
// - HitPrefix: 전체 래그돌로 바닥에 놓인 뒤 키 입력으로 특정 본에 충격을 준다
// 키: R 처음 상태로 리셋, F 머리 타격, G 술 끝 타격, H 매달기 정거장 움직임 일시정지
// Pawn이 아니므로 플레이 중 카메라는 기본 Pawn(WASD/QE 자유 비행)을 쓴다.
UCLASS()
class ARagdollTestDirector : public AActor
{
	GENERATED_BODY()

public:
	ARagdollTestDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	FString DropPrefix = "Ragdoll_";
	UPROPERTY(EditAnywhere)
	FString HangPrefix = "Hang_";
	UPROPERTY(EditAnywhere)
	FString HitPrefix = "Hit_";

	// 매달기 정거장에서 키네마틱으로 남길 본. 이 본의 자식 본들 아래가 시뮬레이션된다
	UPROPERTY(EditAnywhere)
	FString HangRootBone = "Head";
	// 좌우 흔들기 진폭 (m)과 주기 (초)
	UPROPERTY(EditAnywhere)
	float HangSwingAmplitude = 0.8f;
	UPROPERTY(EditAnywhere)
	float HangSwingPeriod = 2.5f;
	// 제자리 회전 속도 (도/초)
	UPROPERTY(EditAnywhere)
	float HangSpinSpeed = 60.0f;

	// 타격 방향 (월드). 수평 성분은 정규화하고 위쪽 성분을 더한다
	UPROPERTY(EditAnywhere)
	FVector HitDirection = FVector(1.0f, 0.0f, 0.0f);
	// 속도 변화량 (m/s)
	UPROPERTY(EditAnywhere)
	float HitHeadSpeed = 6.0f;
	UPROPERTY(EditAnywhere)
	float HitTasselSpeed = 10.0f;
	UPROPERTY(EditAnywhere)
	float HitUpSpeed = 2.5f;
	UPROPERTY(EditAnywhere)
	FString HitHeadBone = "Head";
	UPROPERTY(EditAnywhere)
	FString HitTasselBone = "Tassel_04_3";

private:
	enum class ERole : uint8
	{
		Drop,
		Hang,
		Hit,
	};

	struct FTestActor
	{
		ASkeletalMeshActor* Actor = nullptr;
		ERole Role = ERole::Drop;
		FVector StartLocation = FVector::ZeroVector;
		FRotator StartRotation;
	};

	void ResolveActors();
	void Activate(FTestActor& Test);
	void ResetAll();
	void UpdateHangMotion(float DeltaTime);
	void HitAll(const FString& BoneName, float Speed);

	TArray<FTestActor> TestActors;
	bool bPendingActivate = true;
	bool bHangPaused = false;
	float HangTime = 0.0f;
};
