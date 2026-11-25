#pragma once

#include "GameFramework/Pawn.h"
#include "DragonRagdollDirector.generated.h"

class ASkeletalMeshActor;
class USkeletalMeshComponent;
class UAnimSequence;

// 드래곤 파워 어택에 맞은 캐릭터가 래그돌로 하늘 높이 날아가는 연출을 진행한다.
// 씬의 첫 Pawn으로 빙의되어 카메라 시점이 된다. 드래곤·피해자는 씬의 SkeletalMeshActor를 이름으로 찾는다.
// 흐름: Idle(대기) → PowerAttack → ImpactTime에 피해자 래그돌 전환 + 발사 + 카메라 셰이크 → 카메라가 래그돌 추적.
// R 키로 처음부터 다시 재생한다.
UCLASS()
class ADragonRagdollDirector : public APawn
{
	GENERATED_BODY()

public:
	ADragonRagdollDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutPOV) override;

	UPROPERTY(EditAnywhere)
	FString DragonActorName = "FrostDragon";
	UPROPERTY(EditAnywhere)
	FString VictimActorName = "Dovahkiin";
	UPROPERTY(EditAnywhere)
	FString IdleAnimationPath = "FBX/FrostDragon/FrostDragon_Dragon_Ancient_Idle.anim.bin";
	UPROPERTY(EditAnywhere)
	FString PowerAttackAnimationPath = "FBX/FrostDragon/FrostDragon_Dragon_Ancient_Attack_PowerAttack.anim.bin";

	// 시작 후 공격하기 전까지 Idle 시간 (초)
	UPROPERTY(EditAnywhere)
	float IdleTimeBeforeAttack = 2.5f;
	// PowerAttack 재생 시작부터 타격 순간까지 (초). 머리와 앞발이 땅을 내려찍는 프레임
	UPROPERTY(EditAnywhere)
	float ImpactTime = 1.9f;
	// 클립 뒤쪽은 정지 포즈만 이어지므로 이 시간에 Idle로 돌아간다 (초)
	UPROPERTY(EditAnywhere)
	float PowerAttackEndTime = 3.2f;
	UPROPERTY(EditAnywhere)
	float PowerAttackPlayRate = 1.0f;

	// 발사 속도 변화량 (m/s). 수평은 드래곤 → 피해자 방향
	UPROPERTY(EditAnywhere)
	float LaunchUpSpeed = 24.0f;
	UPROPERTY(EditAnywhere)
	float LaunchBackSpeed = 9.0f;
	// 상체는 뒤로, 다리는 앞으로 더 밀어 뒤로 공중제비 돌게 한다 (m/s)
	UPROPERTY(EditAnywhere)
	float LaunchSpinSpeed = 4.0f;

	// 타격 순간 카메라 셰이크 배율과 히트 스톱
	UPROPERTY(EditAnywhere)
	float ImpactShakeScale = 1.0f;
	UPROPERTY(EditAnywhere)
	float HitStopDuration = 0.08f;

	// 스카이림식 3인칭 카메라. 기준점은 FocusBone에서 FollowUpOffset 위(어깨 높이) + 오른쪽 어깨 오프셋
	UPROPERTY(EditAnywhere)
	FString FocusBoneName = "Hips";
	// 기준점 뒤로 떨어진 거리 (m)
	UPROPERTY(EditAnywhere)
	float FollowBackDistance = 3.0f;
	// 오른쪽 어깨 너머 오프셋 (m). 음수면 왼쪽 어깨
	UPROPERTY(EditAnywhere)
	float FollowSideDistance = 0.55f;
	// Hips에서 어깨 높이까지 (m)
	UPROPERTY(EditAnywhere)
	float FollowUpOffset = 0.75f;
	// 10m 앞 시선 높이 (m). 크면 위를 올려다본다
	UPROPERTY(EditAnywhere)
	float CameraLookUpOffset = 1.2f;
	// 값이 클수록 카메라가 빠르게 따라간다 (1/s)
	UPROPERTY(EditAnywhere)
	float FollowLocationLag = 12.0f;
	UPROPERTY(EditAnywhere)
	float FollowLookLag = 18.0f;
	UPROPERTY(EditAnywhere)
	float CameraFOV = 70.0f;

private:
	enum class EPhase : uint8
	{
		Idle,
		Attack,
		Launched,
	};

	bool ResolveActors();
	void StartSequence();
	void StartAttack();
	void Impact();
	void PlayCameraShake();
	FVector GetFocusLocation() const;
	FVector GetVictimLocation() const;
	FVector GetLaunchDirection() const;
	void UpdateThirdPersonCamera(float DeltaTime, bool bSnap);

	ASkeletalMeshActor* Dragon = nullptr;
	ASkeletalMeshActor* Victim = nullptr;
	UAnimSequence* IdleAnim = nullptr;
	UAnimSequence* AttackAnim = nullptr;

	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.0f;
	bool bDragonBackToIdle = false;

	FVector VictimStartLocation = FVector::ZeroVector;
	FRotator VictimStartRotation;
	FVector LaunchDirection = FVector(1.0f, 0.0f, 0.0f);

	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraLookAt = FVector::ZeroVector;
	bool bCameraInitialized = false;
};
