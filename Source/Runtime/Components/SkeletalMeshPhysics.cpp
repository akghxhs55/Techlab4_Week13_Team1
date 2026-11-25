#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"

#include "Collision/HitResult.h"
#include "Collision/Ray.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Physics/ConstraintInstance.h"
#include "Physics/PhysicsCore.h"
#include "Physics/PhysScene.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysXConversion.h"
#include "Render/SkeletalMesh.h"
#include <PxRigidDynamic.h>
#include <PxAggregate.h>
#include <PxPhysics.h>
#include <PxScene.h>
#include <PxShape.h>
#include <geometry/PxGeometryQuery.h>

namespace
{
	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		FVector Scale(
			FVector(Matrix[0][0], Matrix[0][1], Matrix[0][2]).Size(),
			FVector(Matrix[1][0], Matrix[1][1], Matrix[1][2]).Size(),
			FVector(Matrix[2][0], Matrix[2][1], Matrix[2][2]).Size());
		FMatrix Rotation = Matrix;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float InvScale = Scale[Axis] > 1.e-6f ? 1.0f / Scale[Axis] : 1.0f;
			for (int32 Column = 0; Column < 3; ++Column)
				Rotation[Axis][Column] *= InvScale;
		}
		const FVector AxisX(Rotation[0][0], Rotation[0][1], Rotation[0][2]);
		const FVector AxisY(Rotation[1][0], Rotation[1][1], Rotation[1][2]);
		const FVector AxisZ(Rotation[2][0], Rotation[2][1], Rotation[2][2]);
		if (FVector::Dot(FVector::Cross(AxisX, AxisY), AxisZ) < 0.0f)
			for (int32 Column = 0; Column < 3; ++Column)
				Rotation[2][Column] = -Rotation[2][Column];
		Rotation[3][0] = Rotation[3][1] = Rotation[3][2] = 0.0f;
		return FTransform(FQuat(MatrixToRotator(Rotation)),
			FVector(Matrix[3][0], Matrix[3][1], Matrix[3][2]), Scale);
	}
}

UPhysicsAsset* USkeletalMeshComponent::GetPhysicsAsset() const
{
	return PhysicsAssetOverride ? PhysicsAssetOverride
		: GetSkeletalMesh() ? GetSkeletalMesh()->GetPhysicsAsset() : nullptr;
}

void USkeletalMeshComponent::SetPhysicsAsset(UPhysicsAsset* InAsset)
{
	if (PhysicsAssetOverride == InAsset)
		return;
	TermArticulated();
	PhysicsAssetOverride = InAsset;
	if (IsRegistered())
		InitArticulated();
}

bool USkeletalMeshComponent::ShouldCreatePhysicsState() const
{
	return GetWorld() && GetWorld()->IsGameWorld() && GetWorld()->GetPhysicsScene() &&
		GetSkeletalMesh() && GetPhysicsAsset();
}

void USkeletalMeshComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();
	InitArticulated();
}

void USkeletalMeshComponent::OnDestroyPhysicsState()
{
	TermArticulated();
	Super::OnDestroyPhysicsState();
}

void USkeletalMeshComponent::OnUnregister()
{
	TermArticulated();
	Super::OnUnregister();
}

bool USkeletalMeshComponent::InitArticulated()
{
	if (!Bodies.IsEmpty())
		return true;
	UWorld* World = GetWorld();
	UPhysicsAsset* Asset = GetPhysicsAsset();
	if (!World || !World->IsGameWorld() || !World->GetPhysicsScene() || !GetSkeletalMesh() || !Asset)
		return false;

	FString Error;
	if (!Asset->Validate(*GetSkeletalMesh(), Error))
	{
		KRS_LOG(Warning, "Physics Asset {}: {}", Asset->GetAssetPath(), Error);
		return false;
	}
	RefreshBoneTransformsNow();
	for (USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
	{
		const int32 BoneIndex = FindBoneIndex(Setup->BoneName);
		auto* Body = new FBodyInstance();
		Body->bIsSkeletalBody = true;
		Body->bSimulatePhysics = false;
		Body->bEnableGravity = Setup->DefaultInstance.bEnableGravity;
		Body->MassInKgOverride = Setup->DefaultInstance.MassInKgOverride;
		Body->LinearDamping = Setup->DefaultInstance.LinearDamping;
		Body->AngularDamping = Setup->DefaultInstance.AngularDamping;
		Body->CollisionEnabled = Setup->DefaultInstance.CollisionEnabled;
		Body->InitBody(Setup, TransformFromMatrix(GetBoneWorldMatrix(BoneIndex)), this, World->GetPhysicsScene());
		if (!Body->IsValidBodyInstance())
		{
			delete Body;
			TermArticulated();
			return false;
		}
		Bodies.Add(Body);
		BodyBoneIndices.Add(BoneIndex);
	}

	physx::PxU32 ShapeCount = 0;
	for (FBodyInstance* Body : Bodies)
		ShapeCount += Body->GetPxRigidDynamic()->getNbShapes();
	BodyAggregate = FPhysicsCore::GetPhysics()->createAggregate(
		static_cast<physx::PxU32>(Bodies.Num()), ShapeCount,
		physx::PxGetAggregateFilterHint(physx::PxAggregateType::eGENERIC, false));
	if (!BodyAggregate)
	{
		TermArticulated();
		return false;
	}
	physx::PxScene* PxScene = World->GetPhysicsScene()->GetPxScene();
	for (FBodyInstance* Body : Bodies)
	{
		physx::PxRigidDynamic* Actor = Body->GetPxRigidDynamic();
		PxScene->removeActor(*Actor);
		if (!BodyAggregate->addActor(*Actor))
		{
			TermArticulated();
			return false;
		}
	}
	if (!PxScene->addAggregate(*BodyAggregate))
	{
		TermArticulated();
		return false;
	}

	for (UPhysicsConstraintTemplate* Template : Asset->ConstraintSetup)
	{
		auto* Instance = new FConstraintInstance(Template->DefaultInstance);
		FBodyInstance* Child = FindArticulatedBody(Instance->ConstraintBone1);
		FBodyInstance* Parent = FindArticulatedBody(Instance->ConstraintBone2);
		if (!Child || !Parent || !Instance->InitConstraint(*Child, *Parent))
		{
			delete Instance;
			TermArticulated();
			return false;
		}
		Constraints.Add(Instance);
	}
	return true;
}

void USkeletalMeshComponent::TermArticulated()
{
	if (bRagdollActive)
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
			Character->RestoreRagdollMovementAndCollision();
	if (UWorld* World = GetWorld())
		if (FPhysScene* Scene = World->GetPhysicsScene())
			Scene->UnregisterSimulatingSkeletalMesh(this);
	bRagdollActive = false;
	bLogNextRagdollStep = false;
	for (FConstraintInstance* Constraint : Constraints)
	{
		Constraint->TermConstraint();
		delete Constraint;
	}
	Constraints.Reset();
	for (FBodyInstance* Body : Bodies)
	{
		Body->TermBody();
		delete Body;
	}
	Bodies.Reset();
	if (BodyAggregate)
	{
		BodyAggregate->release();
		BodyAggregate = nullptr;
	}
	BodyBoneIndices.Reset();
}

FBodyInstance* USkeletalMeshComponent::FindArticulatedBody(const FString& BoneName) const
{
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		if (GetBones()[BodyBoneIndices[Index]].Name == BoneName)
			return Bodies[Index];
	return nullptr;
}

FBodyInstance* USkeletalMeshComponent::GetBodyInstance(const FString& BoneName) const
{
	return FindArticulatedBody(BoneName);
}

void USkeletalMeshComponent::AddImpulseToBone(const FString& BoneName, const FVector& Impulse, bool bVelocityChange)
{
	FBodyInstance* Body = FindArticulatedBody(BoneName);
	if (!Body)
	{
		return;
	}

	Body->AddImpulse(Impulse, bVelocityChange);

	// 래그돌이나 물리 바디 시뮬레이션 중 외부 충격량이 들어오면 리스너로 전파
	if ((bRagdollActive || IsAnyBodySimulatingPhysics()) && !PhysicsImpactListeners.IsEmpty())
	{
		const physx::PxRigidDynamic* Dyn = Body->GetPxRigidDynamic();
		const float MassScale = (bVelocityChange && Dyn) ? Dyn->getMass() : 1.0f;
		const FVector EffectiveImpulse = Impulse * MassScale;
		const float ImpulseMag = EffectiveImpulse.Size();
		if (ImpulseMag > 1e-4f)
		{
			FSkeletalPhysicsImpact Impact{};
			Impact.BoneIndex = FindBoneIndexForBody(Body);
			Impact.WorldLocation = Body->GetUnrealWorldTransform().Location;
			Impact.WorldNormal = EffectiveImpulse / ImpulseMag;
			Impact.WorldImpulse = EffectiveImpulse;
			Impact.WorldSlideVelocity = EffectiveImpulse / std::max(MassScale, 1e-4f);
			Impact.ImpulseMagnitude = ImpulseMag;
			Impact.bFromExternalImpulse = true;

			const TArray<FPhysicsImpactListenerEntry> Snapshot = PhysicsImpactListeners;
			for (int32 i = 0; i < Snapshot.Num(); ++i)
			{
				if (Snapshot[i].Callback)
				{
					Snapshot[i].Callback(Impact);
				}
			}
		}
	}
}

int32 USkeletalMeshComponent::FindBoneIndexForBody(const FBodyInstance* Body) const
{
	if (!Body)
	{
		return -1;
	}
	for (int32 Index = 0; Index < Bodies.Num() && Index < BodyBoneIndices.Num(); ++Index)
	{
		if (Bodies[Index] == Body)
		{
			return BodyBoneIndices[Index];
		}
	}
	return -1;
}

void USkeletalMeshComponent::AddPhysicsImpactListener(const void* ListenerKey, FSkeletalPhysicsImpactCallback Callback)
{
	if (!ListenerKey || !Callback)
	{
		return;
	}
	for (int32 i = 0; i < PhysicsImpactListeners.Num(); ++i)
	{
		if (PhysicsImpactListeners[i].Key == ListenerKey)
		{
			PhysicsImpactListeners[i].Callback = std::move(Callback);
			return;
		}
	}
	PhysicsImpactListeners.Add({ ListenerKey, std::move(Callback) });
}

void USkeletalMeshComponent::RemovePhysicsImpactListener(const void* ListenerKey)
{
	if (!ListenerKey)
	{
		return;
	}
	for (int32 i = PhysicsImpactListeners.Num() - 1; i >= 0; --i)
	{
		if (PhysicsImpactListeners[i].Key == ListenerKey)
		{
			PhysicsImpactListeners.RemoveAt(i, 1);
		}
	}
}

void USkeletalMeshComponent::NotifyPhysicsContactImpact(const FPhysicsContactImpact& ContactImpact)
{
	if (PhysicsImpactListeners.IsEmpty() || (!bRagdollActive && !IsAnyBodySimulatingPhysics()))
	{
		return;
	}

	FSkeletalPhysicsImpact Impact{};
	Impact.BoneIndex = FindBoneIndexForBody(ContactImpact.HitBody);
	Impact.WorldLocation = ContactImpact.WorldLocation;
	Impact.WorldNormal = ContactImpact.WorldNormal;
	Impact.WorldImpulse = ContactImpact.WorldImpulse;
	Impact.WorldSlideVelocity = ContactImpact.WorldSlideVelocity;
	Impact.ImpulseMagnitude = ContactImpact.ImpulseMagnitude;
	Impact.bFromExternalImpulse = false;

	const TArray<FPhysicsImpactListenerEntry> Snapshot = PhysicsImpactListeners;
	for (int32 i = 0; i < Snapshot.Num(); ++i)
	{
		if (Snapshot[i].Callback)
		{
			Snapshot[i].Callback(Impact);
		}
	}
}

void USkeletalMeshComponent::SetRagdollLinearVelocity(const FVector& Velocity)
{
	if (!bRagdollActive) return;
	for (FBodyInstance* Body : Bodies)
		Body->SetLinearVelocity(Velocity);
}

bool USkeletalMeshComponent::IsAnyBodySimulatingPhysics() const
{
	for (const FBodyInstance* Body : Bodies)
		if (Body->IsDynamic()) return true;
	return false;
}

bool USkeletalMeshComponent::SetBodySimulatePhysics(const FString& BoneName, bool bSimulate)
{
	if (bRagdollActive)
		return false;
	if (Bodies.IsEmpty() && !InitArticulated())
		return false;
	FBodyInstance* Body = FindArticulatedBody(BoneName);
	if (!Body)
		return false;
	if (bSimulate && !Body->IsDynamic())
	{
		RefreshBoneTransformsNow();
		Body->SetBodyTransform(TransformFromMatrix(GetBoneWorldMatrix(FindBoneIndex(BoneName))), true);
		for (FConstraintInstance* Constraint : Constraints)
			if (Constraint->ConstraintBone1 == BoneName || Constraint->ConstraintBone2 == BoneName)
			{
				FBodyInstance* Child = FindArticulatedBody(Constraint->ConstraintBone1);
				FBodyInstance* Parent = FindArticulatedBody(Constraint->ConstraintBone2);
				if (Child && Parent) Constraint->UpdateConstraintFrames(*Child, *Parent);
			}
	}
	Body->SetCollisionEnabled(bSimulate ? ECollisionEnabled::QueryAndPhysics
		: Body->BodySetup ? static_cast<USkeletalBodySetup*>(Body->BodySetup)->DefaultInstance.CollisionEnabled
		: ECollisionEnabled::QueryOnly);
	Body->SetInstanceSimulatePhysics(bSimulate);
	if (FPhysScene* Scene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr)
	{
		if (IsAnyBodySimulatingPhysics()) Scene->RegisterSimulatingSkeletalMesh(this);
		else Scene->UnregisterSimulatingSkeletalMesh(this);
	}
	return true;
}

bool USkeletalMeshComponent::SetAllBodiesBelowSimulatePhysics(const FString& BoneName, bool bSimulate)
{
	const int32 RootIndex = FindBoneIndex(BoneName);
	if (RootIndex < 0 || bRagdollActive)
		return false;
	if (Bodies.IsEmpty() && !InitArticulated())
		return false;
	bool bChanged = false;
	for (int32 BodyIndex : BodyBoneIndices)
	{
		int32 Index = BodyIndex;
		while (Index >= 0 && Index != RootIndex)
			Index = GetBones()[Index].ParentIndex;
		if (Index == RootIndex)
			bChanged |= SetBodySimulatePhysics(GetBones()[BodyIndex].Name, bSimulate);
	}
	return bChanged;
}

void USkeletalMeshComponent::UpdateKinematicBonesToAnim()
{
	if (Bodies.IsEmpty())
		return;
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		if (!Bodies[Index]->IsDynamic())
			Bodies[Index]->SetBodyTransform(TransformFromMatrix(GetBoneWorldMatrix(BodyBoneIndices[Index])), false);
}

bool USkeletalMeshComponent::StartRagdoll()
{
	if (bRagdollActive)
		return true;
	if (Bodies.IsEmpty() && !InitArticulated())
		return false;
	RefreshBoneTransformsNow();
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		Bodies[Index]->SetBodyTransform(TransformFromMatrix(GetBoneWorldMatrix(BodyBoneIndices[Index])), true);
	for (FConstraintInstance* Constraint : Constraints)
	{
		FBodyInstance* Child = FindArticulatedBody(Constraint->ConstraintBone1);
		FBodyInstance* Parent = FindArticulatedBody(Constraint->ConstraintBone2);
		if (Child && Parent) Constraint->UpdateConstraintFrames(*Child, *Parent);
	}
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		Bodies[Index]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Bodies[Index]->SetInstanceSimulatePhysics(true);
		physx::PxRigidDynamic* Actor = Bodies[Index]->GetPxRigidDynamic();
		Actor->setLinearVelocity(physx::PxVec3(0.0f));
		Actor->setAngularVelocity(physx::PxVec3(0.0f));
	}
	bRagdollActive = true;
	bLogNextRagdollStep = true;
	RagdollStartBodyPosition = Bodies[0]->GetUnrealWorldTransform().Location;
	GetWorld()->GetPhysicsScene()->RegisterSimulatingSkeletalMesh(this);
	KRS_LOG(Info, "[Ragdoll] simulation started: component={}, asset={}, bodies={}", GetName(),
		GetPhysicsAsset()->GetAssetPath(), Bodies.Num());
	return true;
}

void USkeletalMeshComponent::StopRagdoll()
{
	if (!bRagdollActive)
		return;
	GetWorld()->GetPhysicsScene()->UnregisterSimulatingSkeletalMesh(this);
	for (FBodyInstance* Body : Bodies)
	{
		Body->SetInstanceSimulatePhysics(false);
		if (Body->BodySetup)
			Body->SetCollisionEnabled(static_cast<USkeletalBodySetup*>(Body->BodySetup)->DefaultInstance.CollisionEnabled);
	}
	bRagdollActive = false;
	if (AnimInstance && BoneSpaceTransforms.Num() == GetBones().Num())
		RefreshAnimationPose();
	else if (BoneSpaceTransforms.Num() == GetBones().Num())
	{
		for (int32 Index = 0; Index < GetBones().Num(); ++Index)
			BoneSpaceTransforms[Index] = GetBones()[Index].LocalBindPose;
		MarkBoneTransformsDirty();
		RefreshBoneTransformsNow();
	}
	UpdateKinematicBonesToAnim();
}

void USkeletalMeshComponent::SyncPhysicsToBones()
{
	if (Bodies.IsEmpty() || !IsAnyBodySimulatingPhysics())
		return;
	if (bLogNextRagdollStep)
	{
		bLogNextRagdollStep = false;
		float MaxSpeed = 0.0f;
		int32 FastestBody = 0;
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			const float Speed = Bodies[Index]->GetLinearVelocity().Size();
			if (Speed > MaxSpeed) { MaxSpeed = Speed; FastestBody = Index; }
		}
		const FVector Delta = Bodies[0]->GetUnrealWorldTransform().Location - RagdollStartBodyPosition;
		KRS_LOG(Info, "[Ragdoll] first step: rootDelta=({},{},{}), fastestBone={}, speed={}",
			Delta.X, Delta.Y, Delta.Z, GetBones()[BodyBoneIndices[FastestBody]].Name, MaxSpeed);
	}
	const TArray<FBoneInfo>& Skeleton = GetBones();
	TArray<FMatrix> NewComponentSpace;
	NewComponentSpace.SetNum(Skeleton.Num());
	const FMatrix WorldToComponent = GetWorldMatrix().Inverse();
	for (int32 BoneIndex = 0; BoneIndex < Skeleton.Num(); ++BoneIndex)
	{
		const int32 Parent = Skeleton[BoneIndex].ParentIndex;
		FBodyInstance* Body = nullptr;
		for (int32 BodyIndex = 0; BodyIndex < BodyBoneIndices.Num(); ++BodyIndex)
			if (BodyBoneIndices[BodyIndex] == BoneIndex)
			{
				Body = Bodies[BodyIndex];
				break;
			}
		if (Body && Body->IsDynamic())
		{
			NewComponentSpace[BoneIndex] = Body->GetUnrealWorldTransform().GetLocalMatrix() * WorldToComponent;
			const FMatrix Local = Parent < 0 ? NewComponentSpace[BoneIndex]
				: NewComponentSpace[BoneIndex] * NewComponentSpace[Parent].Inverse();
			BoneSpaceTransforms[BoneIndex] = TransformFromMatrix(Local);
		}
		else
		{
			const FMatrix Local = BoneSpaceTransforms[BoneIndex].GetLocalMatrix();
			NewComponentSpace[BoneIndex] = Parent < 0 ? Local : Local * NewComponentSpace[Parent];
		}
	}
	MarkBoneTransformsDirty();
	UpdateBoneTransformsNow();
}

bool USkeletalMeshComponent::RaycastPhysicsBodies(const FRay& WorldRay, FHitResult& OutHit) const
{
	if (Bodies.IsEmpty())
		return false;
	const FVector Direction = WorldRay.Direction.GetSafeNormal();
	if (Direction.SizeSquared() <= SMALL_NUMBER)
		return false;
	float Nearest = FLT_MAX;
	bool bHit = false;
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		if (Bodies[Index]->CollisionEnabled == ECollisionEnabled::NoCollision ||
			Bodies[Index]->CollisionEnabled == ECollisionEnabled::PhysicsOnly) continue;
		auto* Actor = Bodies[Index]->GetPxRigidDynamic();
		if (!Actor) continue;
		const physx::PxTransform BodyPose = Bodies[Index]->IsDynamic() ? Actor->getGlobalPose()
			: K2PTransform(TransformFromMatrix(GetBoneWorldMatrix(BodyBoneIndices[Index])));
		const physx::PxU32 ShapeCount = Actor->getNbShapes();
		TArray<physx::PxShape*> Shapes;
		Shapes.SetNum(static_cast<int32>(ShapeCount));
		Actor->getShapes(Shapes.GetData(), ShapeCount);
		for (physx::PxShape* Shape : Shapes)
		{
			physx::PxGeomRaycastHit Hit;
			if (!physx::PxGeometryQuery::raycast(K2PVector(WorldRay.Origin), K2PVector(Direction),
				Shape->getGeometry(), BodyPose * Shape->getLocalPose(), Nearest,
				physx::PxHitFlag::eDEFAULT, 1, &Hit))
				continue;
			Nearest = Hit.distance;
			OutHit.HitComponent = const_cast<USkeletalMeshComponent*>(this);
			OutHit.BoneName = GetBones()[BodyBoneIndices[Index]].Name;
			OutHit.Distance = Hit.distance;
			OutHit.ImpactPoint = P2KVector(Hit.position);
			OutHit.Location = OutHit.ImpactPoint;
			OutHit.Normal = OutHit.ImpactNormal = P2KVector(Hit.normal);
			OutHit.bBlockingHit = true;
			bHit = true;
		}
	}
	return bHit;
}

bool USkeletalMeshComponent::SweepPhysicsBodies(const FRay& WorldRay, float MaxDistance, float Radius, FHitResult& OutHit) const
{
	if (Bodies.IsEmpty() || Radius < 0.0f || MaxDistance <= 0.0f)
		return false;
	if (Radius == 0.0f)
	{
		if (!RaycastPhysicsBodies(WorldRay, OutHit) || OutHit.Distance > MaxDistance)
			return false;
		OutHit.Time = OutHit.Distance / MaxDistance;
		return true;
	}
	const FVector Direction = WorldRay.Direction.GetSafeNormal();
	if (Direction.SizeSquared() <= SMALL_NUMBER)
		return false;
	const physx::PxSphereGeometry Sphere(Radius);
	const physx::PxTransform Start(K2PVector(WorldRay.Origin));
	float Nearest = MaxDistance;
	bool bHit = false;
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		if (Bodies[Index]->CollisionEnabled == ECollisionEnabled::NoCollision ||
			Bodies[Index]->CollisionEnabled == ECollisionEnabled::PhysicsOnly) continue;
		auto* Actor = Bodies[Index]->GetPxRigidDynamic();
		if (!Actor) continue;
		const physx::PxTransform BodyPose = Bodies[Index]->IsDynamic() ? Actor->getGlobalPose()
			: K2PTransform(TransformFromMatrix(GetBoneWorldMatrix(BodyBoneIndices[Index])));
		const physx::PxU32 ShapeCount = Actor->getNbShapes();
		TArray<physx::PxShape*> Shapes;
		Shapes.SetNum(static_cast<int32>(ShapeCount));
		Actor->getShapes(Shapes.GetData(), ShapeCount);
		for (physx::PxShape* Shape : Shapes)
		{
			const physx::PxTransform ShapePose = BodyPose * Shape->getLocalPose();
			const bool bOverlapping = physx::PxGeometryQuery::overlap(Sphere, Start, Shape->getGeometry(), ShapePose);
			physx::PxGeomSweepHit Hit;
			if (!bOverlapping && (Nearest <= 0.0f || !physx::PxGeometryQuery::sweep(K2PVector(Direction), Nearest,
				Sphere, Start, Shape->getGeometry(), ShapePose, Hit)))
				continue;
			const float Distance = bOverlapping ? 0.0f : Hit.distance;
			if (bHit && Distance >= Nearest) continue;
			Nearest = Distance;
			OutHit.HitComponent = const_cast<USkeletalMeshComponent*>(this);
			OutHit.BoneName = GetBones()[BodyBoneIndices[Index]].Name;
			OutHit.Distance = Distance;
			OutHit.Time = Distance / MaxDistance;
			OutHit.bStartPenetrating = bOverlapping;
			OutHit.Location = WorldRay.PointAt(Distance);
			OutHit.ImpactPoint = bOverlapping ? WorldRay.Origin : P2KVector(Hit.position);
			OutHit.Normal = OutHit.ImpactNormal = bOverlapping ? -Direction : P2KVector(Hit.normal);
			OutHit.bBlockingHit = true;
			bHit = true;
		}
	}
	return bHit;
}
