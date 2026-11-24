#pragma once

#include "ConstraintInstance.h"
#include "PhysicsAsset.generated.h"

class USkeletalMesh;
class UBodySetup;

USTRUCT()
struct FPhysicsAssetConstraint
{
	GENERATED_BODY()

	UPROPERTY()
	FString ChildBone;

	UPROPERTY()
	FString ParentBone;

	UPROPERTY()
	EConstraintMotion Swing1Motion = EConstraintMotion::Limited;
	
	UPROPERTY()
	EConstraintMotion Swing2Motion = EConstraintMotion::Limited;

	UPROPERTY()
	EConstraintMotion TwistMotion = EConstraintMotion::Limited;

	UPROPERTY()
	float Swing1LimitDeg = 45.0f;

	UPROPERTY()
	float Swing2LimitDeg = 45.0f;

	UPROPERTY()
	float TwistMinDeg = -45.0f;

	UPROPERTY()
	float TwistMaxDeg = 45.0f;

	UPROPERTY()
	FRotator FrameOffset = FRotator::Identitiy;
};

UCLASS()
class UPhysicsAsset : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UPhysicsAsset() override;

	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	bool Validate(const USkeletalMesh& Mesh, const FString& DebugName) const;

	void ClearBodies();

	TArray<UBodySetup*> BodySetups;

	UPROPERTY()
	TArray<FPhysicsAssetConstraint> Constraints;
};
