#include "EnginePCH.h"
#include "PhysicsAsset.h"
#include "Render/SkeletalMesh.h"

const USkeletalBodySetup* UPhysicsAsset::FindBodySetup(const FString& BoneName) const
{
	for (const USkeletalBodySetup* Setup : SkeletalBodySetups)
		if (Setup && Setup->BoneName == BoneName)
			return Setup;
	return nullptr;
}

bool UPhysicsAsset::Validate(const USkeletalMesh& Mesh, FString& OutError) const
{
	OutError.clear();
	if (SkeletalBodySetups.IsEmpty())
	{
		OutError = "PhysicsAsset has no bodies";
		return false;
	}
	for (int32 Index = 0; Index < SkeletalBodySetups.Num(); ++Index)
	{
		const USkeletalBodySetup* Setup = SkeletalBodySetups[Index];
		if (!Setup || Mesh.FindBoneIndex(Setup->BoneName) < 0 || Setup->AggGeom.GetElementCount() == 0)
		{
			OutError = "PhysicsAsset has an invalid body at index " + std::to_string(Index);
			return false;
		}
		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
			if (SkeletalBodySetups[Earlier] && SkeletalBodySetups[Earlier]->BoneName == Setup->BoneName)
			{
				OutError = "PhysicsAsset has duplicate body: " + Setup->BoneName;
				return false;
			}
	}
	for (const UPhysicsConstraintTemplate* Template : ConstraintSetup)
	{
		if (!Template ||
			!FindBodySetup(Template->DefaultInstance.ConstraintBone1) ||
			!FindBodySetup(Template->DefaultInstance.ConstraintBone2) ||
			Template->DefaultInstance.ConstraintBone1 == Template->DefaultInstance.ConstraintBone2)
		{
			OutError = "PhysicsAsset has an invalid constraint";
			return false;
		}
	}
	return true;
}
