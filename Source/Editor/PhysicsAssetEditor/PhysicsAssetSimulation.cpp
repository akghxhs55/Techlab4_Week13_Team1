#include "EnginePCH.h"
#include "PhysicsAssetSimulation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsCore.h"
#include "Physics/PhysScene.h"
#include "Physics/PhysicsSettings.h"
#include "Render/SkeletalMesh.h"
#include "Render/LineBatcher.h"

#include <PxPhysics.h>
#include <PxScene.h>
#include <PxRigidStatic.h>
#include <extensions/PxSimpleFactory.h>
#include <foundation/PxPlane.h>
#include <cmath>

namespace
{
	bool IsFiniteVector(const FVector& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	bool IsValidShapePose(const FShapeElem& Shape)
	{
		return IsFiniteVector(Shape.Center) && std::isfinite(Shape.Rotation.Pitch) &&
			std::isfinite(Shape.Rotation.Yaw) && std::isfinite(Shape.Rotation.Roll);
	}

	bool ValidateSimulationAsset(const UPhysicsAsset& Asset, const USkeletalMesh& Mesh, FString& OutError)
	{
		if (Asset.BodySetups.IsEmpty())
		{
			OutError = "Add physics bodies before starting simulation.";
			return false;
		}
		// 런타임 검증에 넘기기 전에 null과 PhysX에 전달할 형상 수치를 검사합니다.
		for (const UBodySetup* Body : Asset.BodySetups)
		{
			if (!Body) { OutError = "The physics asset contains a null body."; return false; }
			bool bValid = true;
			for (const FSphereElem& Sphere : Body->AggGeom.SphereElems)
				bValid &= IsValidShapePose(Sphere) && std::isfinite(Sphere.Radius) && Sphere.Radius > 0.0f;
			for (const FBoxElem& Box : Body->AggGeom.BoxElems)
				bValid &= IsValidShapePose(Box) && std::isfinite(Box.X) && std::isfinite(Box.Y) && std::isfinite(Box.Z) &&
					Box.X > 0.0f && Box.Y > 0.0f && Box.Z > 0.0f;
			for (const FCapsuleElem& Capsule : Body->AggGeom.CapsuleElems)
				bValid &= IsValidShapePose(Capsule) && std::isfinite(Capsule.Radius) && std::isfinite(Capsule.Length) &&
					Capsule.Radius > 0.0f && Capsule.Length >= 0.0f;
			if (!bValid)
			{
				OutError = FString("Invalid collision shape on body: ") + Body->BoneName;
				return false;
			}
		}
		for (const FPhysicsAssetConstraint& Constraint : Asset.Constraints)
		{
			const auto ValidMotion = [](EConstraintMotion Motion)
			{
				return Motion == EConstraintMotion::Locked || Motion == EConstraintMotion::Limited || Motion == EConstraintMotion::Free;
			};
			if (!ValidMotion(Constraint.Swing1Motion) || !ValidMotion(Constraint.Swing2Motion) || !ValidMotion(Constraint.TwistMotion) ||
				!std::isfinite(Constraint.Swing1LimitDeg) || !std::isfinite(Constraint.Swing2LimitDeg) ||
				!std::isfinite(Constraint.TwistMinDeg) || !std::isfinite(Constraint.TwistMaxDeg) ||
				!std::isfinite(Constraint.FrameOffset.Pitch) || !std::isfinite(Constraint.FrameOffset.Yaw) ||
				!std::isfinite(Constraint.FrameOffset.Roll))
			{
				OutError = "Constraint motion and angle settings are invalid.";
				return false;
			}
		}
		if (!Asset.Validate(Mesh, "Physics Asset Editor Preview"))
		{
			OutError = "Invalid physics asset. Check body shapes and connect each child body to its nearest ancestor body. See Output Log.";
			return false;
		}
		return true;
	}
}

FPhysicsAssetSimulation::~FPhysicsAssetSimulation()
{
	Stop();
}

bool FPhysicsAssetSimulation::Start(USkeletalMeshComponent& Component, UPhysicsAsset& Asset, FString& OutError)
{
	if (bActive) return true;
	OutError.clear();
	UWorld* World = Component.GetWorld();
	USkeletalMesh* Mesh = Component.GetSkeletalMesh();
	FPhysScene* Scene = World ? World->GetPhysicsScene() : nullptr;
	physx::PxPhysics* Physics = FPhysicsCore::GetPhysics();
	physx::PxMaterial* Material = FPhysicsCore::GetDefaultMaterial();
	if (!World || World->GetWorldType() != EWorldType::EditorPreview || !Mesh || Mesh->GetPhysicsAsset() != &Asset ||
		!Scene || !Scene->GetPxScene() || !Physics || !Material)
	{
		OutError = "The preview physics scene or physics asset is unavailable.";
		return false;
	}
	if (!ValidateSimulationAsset(Asset, *Mesh, OutError)) return false;
	const FVector Scale = Component.GetWorldScale3D();
	if (!IsFiniteVector(Scale) || Scale.X <= 0.0f || Scale.Y <= 0.0f || Scale.Z <= 0.0f)
	{
		OutError = "Simulation requires a finite, positive mesh scale.";
		return false;
	}
	Component.RefreshBoneTransformsNow();
	if (Component.GetBoneSpaceTransforms().Num() != Mesh->GetNumBones())
	{
		OutError = "The preview bone pose is not ready.";
		return false;
	}
	MeshComponent = &Component;
	PhysicsScene = Scene;
	AActor* Owner = Component.GetOwner();
	TransformTarget = Owner && Owner->GetRootComponent() ? Owner->GetRootComponent() : &Component;
	SavedTargetTransform = TransformTarget->GetTransform();
	SavedMeshTransform = Component.GetTransform();
	SavedPose = Component.GetBoneSpaceTransforms();
	const FBox Bounds = Component.CalcBounds();
	if (!IsFiniteVector(Bounds.Min) || !IsFiniteVector(Bounds.Max) ||
		Bounds.Min.X > Bounds.Max.X || Bounds.Min.Y > Bounds.Max.Y || Bounds.Min.Z > Bounds.Max.Z)
	{
		OutError = "The preview mesh bounds are invalid.";
		Stop();
		return false;
	}
	FloorCenter = FVector((Bounds.Min.X + Bounds.Max.X) * 0.5f, (Bounds.Min.Y + Bounds.Max.Y) * 0.5f, Bounds.Min.Z);
	FloorHalfExtent = FMath::Max(1.0f, FMath::Max(Bounds.Max.Z - Bounds.Min.Z,
		FMath::Max(Bounds.Max.X - Bounds.Min.X, Bounds.Max.Y - Bounds.Min.Y))) * 2.0f;
	bool bCreated = false;
	{
		FPhysSceneWriteLock Lock(Scene);
		Component.SetSimulatePhysics(false);
		Component.TermArticulated();
		physx::PxScene* PxScene = Scene->GetPxScene();
		const physx::PxU32 PreviousBodies = PxScene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC);
		const physx::PxU32 PreviousConstraints = PxScene->getNbConstraints();
		Component.InitArticulated(Scene);
		bCreated = PxScene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC) == PreviousBodies + static_cast<physx::PxU32>(Asset.BodySetups.Num()) &&
			PxScene->getNbConstraints() == PreviousConstraints + static_cast<physx::PxU32>(Asset.Constraints.Num());
		if (bCreated)
		{
			Floor = physx::PxCreatePlane(*Physics, physx::PxPlane(0.0f, 0.0f, 1.0f, -FloorCenter.Z), *Material);
			if (Floor)
			{
				PxScene->addActor(*Floor);
				Component.SetSimulatePhysics(true);
			}
			else bCreated = false;
		}
	}
	if (!bCreated)
	{
		OutError = "Could not create all preview bodies, joints or the collision floor. See Output Log.";
		Stop();
		return false;
	}
	ElapsedTime = 0.0f;
	bPaused = false;
	bStepRequested = false;
	bActive = true;
	return true;
}

void FPhysicsAssetSimulation::Stop()
{
	if (MeshComponent)
	{
		{
			FPhysSceneWriteLock Lock(PhysicsScene);
			MeshComponent->SetSimulatePhysics(false);
			MeshComponent->TermArticulated();
			if (Floor) { Floor->release(); Floor = nullptr; }
		}
		if (TransformTarget) TransformTarget->SetTransform(SavedTargetTransform, ETeleportType::TeleportPhysics);
		if (TransformTarget != MeshComponent) MeshComponent->SetTransform(SavedMeshTransform, ETeleportType::TeleportPhysics);
		for (int32 Index = 0; Index < SavedPose.Num(); ++Index) MeshComponent->SetBoneLocalTransform(Index, SavedPose[Index]);
		MeshComponent->RefreshBoneTransformsNow();
	}
	MeshComponent = nullptr;
	TransformTarget = nullptr;
	PhysicsScene = nullptr;
	SavedPose.Empty();
	bActive = false;
	bPaused = false;
	bStepRequested = false;
	ElapsedTime = 0.0f;
}

void FPhysicsAssetSimulation::Tick(float DeltaTime)
{
	if (!bActive || !MeshComponent || !PhysicsScene) return;
	if ((!bPaused || bStepRequested) && std::isfinite(DeltaTime) && DeltaTime >= 0.0f)
	{
		FPhysicsSettings Settings;
		Settings.bSubstepping = true;
		Settings.MaxSubstepDeltaTime = 1.0f / 120.0f;
		Settings.MaxSubsteps = 8;
		const float StepTime = bStepRequested ? 1.0f / 60.0f : FMath::Min(DeltaTime, Settings.MaxSubstepDeltaTime * Settings.MaxSubsteps);
		if (StepTime > 0.0f)
		{
			PhysicsScene->SetUpForFrame(StepTime, Settings);
			PhysicsScene->StartFrame();
			PhysicsScene->EndFrame();
			ElapsedTime += StepTime;
		}
		bStepRequested = false;
	}
	// 프리뷰 월드는 게임의 EndPhysics 틱 그룹을 실행하지 않으므로 여기서 한 번 반영합니다.
	// 일시정지 중에도 바디 포즈를 반영해 일반 메시 틱이 덮어쓰는 것을 방지합니다.
	MeshComponent->EndPhysicsTickComponent(DeltaTime);
}

void FPhysicsAssetSimulation::DrawFloor(FLineBatcher& Lines) const
{
	if (!bActive) return;
	const FVector4 Color(0.35f, 0.4f, 0.45f, 1.0f);
	for (int32 Index = -5; Index <= 5; ++Index)
	{
		const float Offset = FloorHalfExtent * Index / 5.0f;
		Lines.AddLine(FloorCenter + FVector(-FloorHalfExtent, Offset, 0.0f), FloorCenter + FVector(FloorHalfExtent, Offset, 0.0f), Color);
		Lines.AddLine(FloorCenter + FVector(Offset, -FloorHalfExtent, 0.0f), FloorCenter + FVector(Offset, FloorHalfExtent, 0.0f), Color);
	}
}
