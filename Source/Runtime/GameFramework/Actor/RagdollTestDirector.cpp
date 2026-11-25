#include "EnginePCH.h"
#include "GameFramework/Actor/RagdollTestDirector.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Input/InputSystem.h"
#include <cmath>

ARagdollTestDirector::ARagdollTestDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARagdollTestDirector::BeginPlay()
{
	Super::BeginPlay();
	ResolveActors();
	// 다른 액터의 BeginPlay에서 래그돌 바디가 만들어진 뒤 켜도록 첫 Tick까지 미룬다
	bPendingActivate = true;
	HangTime = 0.0f;
}

void ARagdollTestDirector::ResolveActors()
{
	TestActors.Empty();
	UWorld* World = GetWorld();
	if (!World || !World->GetCurrentLevel()) return;

	for (AActor* Actor : World->GetCurrentLevel()->GetActors())
	{
		ASkeletalMeshActor* MeshActor = Cast<ASkeletalMeshActor>(Actor);
		if (!MeshActor || !MeshActor->GetSkeletalMeshComponent()) continue;

		const FString& Name = MeshActor->GetName();
		FTestActor Test;
		if (Name.starts_with(HangPrefix)) Test.Role = ERole::Hang;
		else if (Name.starts_with(HitPrefix)) Test.Role = ERole::Hit;
		else if (Name.starts_with(DropPrefix)) Test.Role = ERole::Drop;
		else continue;

		Test.Actor = MeshActor;
		Test.StartLocation = MeshActor->GetActorLocation();
		Test.StartRotation = MeshActor->GetActorRotation();
		TestActors.Add(Test);
	}
	KRS_LOG(Info, "[RagdollTestDirector] test actors: {}", TestActors.Num());
}

void ARagdollTestDirector::Activate(FTestActor& Test)
{
	USkeletalMeshComponent* Mesh = Test.Actor->GetSkeletalMeshComponent();
	if (Test.Role != ERole::Hang)
	{
		Mesh->SetSimulatePhysics(true);
		return;
	}

	const TArray<FBoneInfo>& Bones = Mesh->GetBones();
	const int32 Root = Mesh->FindBoneIndex(HangRootBone);
	TArray<int32> Children;
	for (int32 B = 0; B < Bones.Num(); ++B)
	{
		if (Root >= 0 && Bones[B].ParentIndex == Root) Children.Add(B);
	}
	if (Children.IsEmpty() || !Mesh->SetSimulatePhysicsBelow(Children))
	{
		KRS_LOG(Warning, "[RagdollTestDirector] '{}' has no simulated bodies below '{}'", Test.Actor->GetName(), HangRootBone);
	}
}

void ARagdollTestDirector::ResetAll()
{
	for (FTestActor& Test : TestActors)
	{
		USkeletalMeshComponent* Mesh = Test.Actor->GetSkeletalMeshComponent();
		Mesh->SetSimulatePhysics(false);
		Test.Actor->GetRootComponent()->SetWorldLocationAndRotation(Test.StartLocation, Test.StartRotation, false, ETeleportType::TeleportPhysics);
		Mesh->ResetPose();
		Mesh->RefreshBoneTransformsNow();
		Mesh->UpdateKinematicBonesToAnim(ETeleportType::TeleportPhysics);
	}
	HangTime = 0.0f;
	// 키네마틱으로 순간이동한 자세가 반영된 다음 프레임에 다시 시뮬레이션한다
	bPendingActivate = true;
}

void ARagdollTestDirector::UpdateHangMotion(float DeltaTime)
{
	if (bHangPaused) return;
	HangTime += DeltaTime;

	const float Period = HangSwingPeriod > 0.0f ? HangSwingPeriod : 1.0f;
	const float Offset = HangSwingAmplitude * std::sin(2.0f * PI * HangTime / Period);
	for (FTestActor& Test : TestActors)
	{
		if (Test.Role != ERole::Hang) continue;
		FRotator Rotation = Test.StartRotation;
		Rotation.Yaw += HangSpinSpeed * HangTime;
		const FVector Location = Test.StartLocation + FVector(0.0f, Offset, 0.0f);
		// 순간이동이 아닌 이동이라 키네마틱 바디가 속도를 갖고 따라가며 시뮬레이션된 술을 끌고 간다
		Test.Actor->GetRootComponent()->SetWorldLocationAndRotation(Location, Rotation, false, ETeleportType::None);
	}
}

void ARagdollTestDirector::HitAll(const FString& BoneName, float Speed)
{
	FVector Horizontal(HitDirection.X, HitDirection.Y, 0.0f);
	Horizontal = Horizontal.GetSafeNormal(SMALL_NUMBER, FVector(1.0f, 0.0f, 0.0f));
	const FVector Impulse = Horizontal * Speed + FVector(0.0f, 0.0f, HitUpSpeed);
	for (FTestActor& Test : TestActors)
	{
		if (Test.Role != ERole::Hit) continue;
		Test.Actor->GetSkeletalMeshComponent()->AddImpulse(Impulse, FName(BoneName), true);
	}
}

void ARagdollTestDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FInputSystem::IsKeyPressed(EKeyCode::R))
	{
		ResetAll();
		return;
	}

	if (bPendingActivate)
	{
		bPendingActivate = false;
		for (FTestActor& Test : TestActors) Activate(Test);
	}

	if (FInputSystem::IsKeyPressed(EKeyCode::F)) HitAll(HitHeadBone, HitHeadSpeed);
	if (FInputSystem::IsKeyPressed(EKeyCode::G)) HitAll(HitTasselBone, HitTasselSpeed);
	if (FInputSystem::IsKeyPressed(EKeyCode::H)) bHangPaused = !bHangPaused;

	UpdateHangMotion(DeltaTime);
}
