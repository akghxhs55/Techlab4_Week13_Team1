#pragma once

#include <utility>
#include "UObject/Object.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "PhysicsAsset.generated.h"

class USkeletalMesh;

UCLASS()
class UPhysicsAsset : public UObject
{
	GENERATED_BODY()
public:
	TArray<USkeletalBodySetup*> SkeletalBodySetups;
	TArray<UPhysicsConstraintTemplate*> ConstraintSetup;

	const USkeletalBodySetup* FindBodySetup(const FString& BoneName) const;
	bool Validate(const USkeletalMesh& Mesh, FString& OutError) const;
	const FString& GetAssetPath() const { return AssetPath; }
	void SetAssetPath(FString InPath) { AssetPath = std::move(InPath); }

private:
	FString AssetPath;
};
