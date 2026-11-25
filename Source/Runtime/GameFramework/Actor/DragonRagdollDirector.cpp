#include "EnginePCH.h"
#include "GameFramework/Actor/DragonRagdollDirector.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Asset/AssetManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Input/InputSystem.h"
#include "Collision/Ray.h"
#include "Collision/HitResult.h"
#include <cmath>

namespace
{
	const FVector WorldUp(0.0f, 0.0f, 1.0f);

	FVector Horizontal(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.0f);
	}

	// 프레임레이트와 무관한 지수 감쇠 보간 계수
	float LagAlpha(float Speed, float DeltaTime)
	{
		return Speed <= 0.0f ? 1.0f : 1.0f - std::exp(-Speed * DeltaTime);
	}

	// 카메라가 산비탈 속으로 들어가지 않도록 바로 아래 지면보다 MinClearance만큼 위에 둔다
	FVector KeepAboveGround(UWorld* World, const FVector& Location, float MinClearance)
	{
		FRay Ray;
		Ray.Origin = FVector(Location.X, Location.Y, Location.Z + 500.0f);
		Ray.Direction = FVector(0.0f, 0.0f, -1.0f);
		FHitResult Hit;
		if (World && World->LineTraceSingle(Ray, Hit) && Location.Z < Hit.ImpactPoint.Z + MinClearance)
		{
			return FVector(Location.X, Location.Y, Hit.ImpactPoint.Z + MinClearance);
		}
		return Location;
	}

	FVector BoneLocation(const USkeletalMeshComponent* Mesh, int32 BoneIndex)
	{
		return Mesh->GetBoneWorldMatrix(BoneIndex).TransformPosition(FVector::ZeroVector);
	}
}

ADragonRagdollDirector::ADragonRagdollDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADragonRagdollDirector::BeginPlay()
{
	Super::BeginPlay();

	IdleAnim = UAssetManager::LoadAnimSequence(IdleAnimationPath);
	AttackAnim = UAssetManager::LoadAnimSequence(PowerAttackAnimationPath);
	if (!IdleAnim) KRS_LOG(Warning, "[DragonRagdollDirector] idle animation not found: {}", IdleAnimationPath);
	if (!AttackAnim) KRS_LOG(Warning, "[DragonRagdollDirector] power attack animation not found: {}", PowerAttackAnimationPath);

	if (!ResolveActors())
	{
		KRS_LOG(Warning, "[DragonRagdollDirector] actors not found (dragon '{}', victim '{}')", DragonActorName, VictimActorName);
		return;
	}

	VictimStartLocation = Victim->GetActorLocation();
	VictimStartRotation = Victim->GetActorRotation();
	StartSequence();
}

bool ADragonRagdollDirector::ResolveActors()
{
	Dragon = nullptr;
	Victim = nullptr;
	UWorld* World = GetWorld();
	if (!World || !World->GetCurrentLevel())
	{
		return false;
	}
	for (AActor* Actor : World->GetCurrentLevel()->GetActors())
	{
		ASkeletalMeshActor* MeshActor = Cast<ASkeletalMeshActor>(Actor);
		if (!MeshActor || !MeshActor->GetSkeletalMeshComponent()) continue;
		if (MeshActor->GetName() == DragonActorName) Dragon = MeshActor;
		else if (MeshActor->GetName() == VictimActorName) Victim = MeshActor;
	}
	return Dragon && Victim;
}

void ADragonRagdollDirector::StartSequence()
{
	Phase = EPhase::Idle;
	PhaseTime = 0.0f;
	bDragonBackToIdle = false;
	bCameraInitialized = false;

	LaunchDirection = Horizontal(GetVictimLocation() - Dragon->GetActorLocation()).GetSafeNormal(SMALL_NUMBER, FVector(1.0f, 0.0f, 0.0f));

	if (IdleAnim)
	{
		Dragon->GetSkeletalMeshComponent()->PlayAnimation(IdleAnim, true);
	}
}

void ADragonRagdollDirector::StartAttack()
{
	Phase = EPhase::Attack;
	PhaseTime = 0.0f;
	USkeletalMeshComponent* DragonMesh = Dragon->GetSkeletalMeshComponent();
	if (AttackAnim)
	{
		DragonMesh->PlayAnimation(AttackAnim, false);
		if (UAnimSingleNodeInstance* Instance = DragonMesh->GetSingleNodeInstance())
		{
			Instance->SetPlayRate(PowerAttackPlayRate);
		}
	}
}

void ADragonRagdollDirector::Impact()
{
	Phase = EPhase::Launched;
	PhaseTime = 0.0f;

	USkeletalMeshComponent* Mesh = Victim->GetSkeletalMeshComponent();
	Mesh->SetSimulatePhysics(true);

	const FVector Launch = LaunchDirection * LaunchBackSpeed + WorldUp * LaunchUpSpeed;
	Mesh->AddImpulseToAllBodiesBelow(Launch, NAME_None, true);

	// 상체는 뒤로·위로, 다리는 앞으로 밀어 뒤로 젖혀지며 회전하게 한다
	const FVector UpperKick = LaunchDirection * LaunchSpinSpeed + WorldUp * (LaunchSpinSpeed * 0.5f);
	const FVector LowerKick = LaunchDirection * (-LaunchSpinSpeed);
	for (const char* Bone : { "Chest", "Head" }) Mesh->AddImpulse(UpperKick, Bone, true);
	for (const char* Bone : { "Leg.L", "Leg.R" }) Mesh->AddImpulse(LowerKick, Bone, true);
	// 팔다리가 제각각 휘날리도록 좌우로 살짝 다르게 준다
	const FVector Side = WorldUp.Cross(LaunchDirection);
	Mesh->AddImpulse(Side * 3.0f + WorldUp * 2.0f, "ForeArm.L", true);
	Mesh->AddImpulse(Side * -3.0f + WorldUp * 1.0f, "ForeArm.R", true);

	PlayCameraShake();
	if (HitStopDuration > 0.0f)
	{
		GetWorld()->StartHitStop(HitStopDuration, 0.05f);
	}
}

void ADragonRagdollDirector::PlayCameraShake()
{
	APlayerController* Controller = GetController();
	APlayerCameraManager* CameraManager = Controller ? Controller->GetCameraManager() : nullptr;
	if (!CameraManager || ImpactShakeScale <= 0.0f)
	{
		return;
	}

	// 짧고 강한 충격 + 길게 남는 낮은 진동 두 겹
	if (UCameraShakeBase* Shake = CameraManager->StartCameraShake(UCameraShakeBase::StaticClass(), ImpactShakeScale))
	{
		Shake->Duration = 0.45f;
		Shake->BlendInTime = 0.0f;
		Shake->BlendOutTime = 0.3f;
		Shake->LocationZ = { 0.35f, 19.0f, EInitialWaveOscillatorOffsetType::Zero };
		Shake->LocationY = { 0.2f, 23.0f, EInitialWaveOscillatorOffsetType::Random };
		Shake->RotationPitch = { 3.0f, 15.0f, EInitialWaveOscillatorOffsetType::Zero };
		Shake->RotationYaw = { 1.5f, 12.0f, EInitialWaveOscillatorOffsetType::Random };
		Shake->RotationRoll = { 2.0f, 9.0f, EInitialWaveOscillatorOffsetType::Random };
		Shake->FOV = { 4.0f, 6.0f, EInitialWaveOscillatorOffsetType::Zero };
		Shake->StartShake(ImpactShakeScale);
	}
	if (UCameraShakeBase* Rumble = CameraManager->StartCameraShake(UCameraShakeBase::StaticClass(), ImpactShakeScale))
	{
		Rumble->Duration = 1.6f;
		Rumble->BlendInTime = 0.05f;
		Rumble->BlendOutTime = 1.0f;
		Rumble->LocationZ = { 0.08f, 7.0f, EInitialWaveOscillatorOffsetType::Random };
		Rumble->RotationPitch = { 0.6f, 5.0f, EInitialWaveOscillatorOffsetType::Random };
		Rumble->RotationRoll = { 0.5f, 4.0f, EInitialWaveOscillatorOffsetType::Random };
		Rumble->StartShake(ImpactShakeScale);
	}
}

FVector ADragonRagdollDirector::GetVictimLocation() const
{
	return Victim ? Victim->GetActorLocation() : FVector::ZeroVector;
}

FVector ADragonRagdollDirector::GetFocusLocation() const
{
	if (!Victim)
	{
		return GetActorLocation();
	}
	const USkeletalMeshComponent* Mesh = Victim->GetSkeletalMeshComponent();
	const int32 Bone = Mesh->FindBoneIndex(FocusBoneName);
	return Bone >= 0 ? BoneLocation(Mesh, Bone) : Victim->GetActorLocation();
}

void ADragonRagdollDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Dragon || !Victim)
	{
		return;
	}

	if (FInputSystem::IsKeyPressed(EKeyCode::R))
	{
		USkeletalMeshComponent* Mesh = Victim->GetSkeletalMeshComponent();
		Mesh->SetSimulatePhysics(false);
		Victim->GetRootComponent()->SetWorldLocationAndRotation(VictimStartLocation, VictimStartRotation, false, ETeleportType::TeleportPhysics);
		Mesh->ResetPose();
		Mesh->RefreshBoneTransformsNow();
		Mesh->UpdateKinematicBonesToAnim(ETeleportType::TeleportPhysics);
		if (APlayerController* Controller = GetController())
		{
			if (APlayerCameraManager* CameraManager = Controller->GetCameraManager()) CameraManager->StopAllCameraShakes(true);
		}
		StartSequence();
		return;
	}

	PhaseTime += DeltaTime;
	switch (Phase)
	{
	case EPhase::Idle:
		if (PhaseTime >= IdleTimeBeforeAttack) StartAttack();
		break;
	case EPhase::Attack:
		if (PhaseTime * PowerAttackPlayRate >= ImpactTime) Impact();
		break;
	case EPhase::Launched:
		break;
	}

	// 공격 모션이 끝나면 드래곤은 다시 Idle로 돌아간다
	if (Phase == EPhase::Launched && !bDragonBackToIdle && IdleAnim)
	{
		const UAnimSingleNodeInstance* Instance = Dragon->GetSkeletalMeshComponent()->GetSingleNodeInstance();
		const float EndTime = PowerAttackEndTime > 0.0f ? PowerAttackEndTime : (Instance ? Instance->GetLength() : 0.0f);
		if (!Instance || Instance->GetCurrentAsset() != AttackAnim || Instance->GetCurrentTime() >= EndTime - 0.02f)
		{
			Dragon->GetSkeletalMeshComponent()->PlayAnimation(IdleAnim, true);
			bDragonBackToIdle = true;
		}
	}
}

void ADragonRagdollDirector::UpdateThirdPersonCamera(float DeltaTime, bool bSnap)
{
	// 스카이림식 3인칭: 피해자 어깨 높이를 중심으로 등 뒤·오른쪽 어깨 너머에서 드래곤 쪽(발사 반대 방향)을 바라본다.
	// 날아가는 동안에도 바라보는 방향은 그대로 두고 몸을 따라가므로, 날아가는 몸 뒤로 드래곤이 멀어지는 것이 보인다
	const FVector Forward = -LaunchDirection;               // 피해자가 바라보는 방향 (드래곤 쪽)
	const FVector Right = WorldUp.Cross(Forward);
	const FVector Pivot = GetFocusLocation() + WorldUp * FollowUpOffset + Right * FollowSideDistance;
	const FVector DesiredLocation = KeepAboveGround(GetWorld(), Pivot - Forward * FollowBackDistance, 0.5f);
	const FVector DesiredLook = Pivot + Forward * 10.0f + WorldUp * CameraLookUpOffset;

	if (bSnap || !bCameraInitialized)
	{
		CameraLocation = DesiredLocation;
		CameraLookAt = DesiredLook;
		bCameraInitialized = true;
		return;
	}
	CameraLocation = FMath::Lerp(CameraLocation, DesiredLocation, LagAlpha(FollowLocationLag, DeltaTime));
	CameraLookAt = FMath::Lerp(CameraLookAt, DesiredLook, LagAlpha(FollowLookLag, DeltaTime));
}

void ADragonRagdollDirector::CalcCamera(float DeltaTime, FMinimalViewInfo& OutPOV)
{
	if (!Dragon || !Victim)
	{
		Super::CalcCamera(DeltaTime, OutPOV);
		return;
	}

	UpdateThirdPersonCamera(DeltaTime, false);

	FRotator Rotation;
	Rotation.LookDirection((CameraLookAt - CameraLocation).GetSafeNormal(SMALL_NUMBER, LaunchDirection));
	OutPOV.Location = CameraLocation;
	OutPOV.Rotation = Rotation;
	OutPOV.FOV = CameraFOV;
}
