#include "EnginePCH.h"
#include "PhysicsAssetUtils.h"

#include "Physics/PhysicsAsset.h"
#include "Render/SkeletalMesh.h"
#include "UObject/UObjectGlobals.h"
#include <algorithm>
#include <cfloat>

bool FPhysicsAssetUtils::CreateFromSkeletalMesh(const USkeletalMesh& Mesh, UPhysicsAsset& Asset,
	const FPhysAssetCreateParams& Params, FString& OutError)
{
	OutError.clear();
	if (Params.MinBoneSize <= 0.0f || Params.MinVertexCount < 1)
	{
		OutError = "Invalid Physics Asset generation settings";
		return false;
	}
	const TArray<FBoneInfo>& Bones = Mesh.GetRefSkeleton();
	const auto& LODs = Mesh.GetRenderData().LODRenderData;
	if (Bones.IsEmpty() || LODs.IsEmpty() || !Asset.SkeletalBodySetups.IsEmpty() || !Asset.ConstraintSetup.IsEmpty())
	{
		OutError = "Physics Asset generation requires a mesh and an empty asset";
		return false;
	}
	const FSkeletalMeshLODRenderData& LOD = LODs[0];
	if (LOD.BindVertices.Num() != LOD.SkinWeights.Num())
	{
		OutError = "Skeletal mesh vertex and skin weight counts differ";
		return false;
	}
	TArray<FBox> LocalBounds;
	TArray<int32> VertexCounts;
	LocalBounds.SetNum(Bones.Num());
	VertexCounts.SetNum(Bones.Num());
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		LocalBounds[Index] = { FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX) };
		VertexCounts[Index] = 0;
	}
	for (int32 VertexIndex = 0; VertexIndex < LOD.BindVertices.Num(); ++VertexIndex)
	{
		const FSkinWeight& Skin = LOD.SkinWeights[VertexIndex];
		int32 BoneIndex = -1;
		float GreatestWeight = 0.0f;
		for (int32 Influence = 0; Influence < MaxBoneInfluences; ++Influence)
		{
			if (Skin.Weight[Influence] > GreatestWeight && Skin.BoneIndex[Influence] < Bones.Num())
			{
				GreatestWeight = Skin.Weight[Influence];
				BoneIndex = Skin.BoneIndex[Influence];
			}
		}
		if (BoneIndex < 0) continue;
		const FVector Local = Bones[BoneIndex].InverseBindPose.TransformPosition(LOD.BindVertices[VertexIndex].Position);
		FBox& Bounds = LocalBounds[BoneIndex];
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Bounds.Min[Axis] = std::min(Bounds.Min[Axis], Local[Axis]);
			Bounds.Max[Axis] = std::max(Bounds.Max[Axis], Local[Axis]);
		}
		++VertexCounts[BoneIndex];
	}
	TArray<uint8> HasBody;
	HasBody.SetNum(Bones.Num());
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		HasBody[Index] = 0;
		if (VertexCounts[Index] < Params.MinVertexCount) continue;
		const FVector Size = LocalBounds[Index].Max - LocalBounds[Index].Min;
		if (std::max({ Size.X, Size.Y, Size.Z }) < Params.MinBoneSize) continue;
		auto* Setup = NewObject<USkeletalBodySetup>(&Asset);
		Setup->BoneName = Bones[Index].Name;
		FBoxElem Box;
		Box.Center = (LocalBounds[Index].Min + LocalBounds[Index].Max) * 0.5f;
		Box.Rotation = FRotator(0, 0, 0);
		Box.X = std::max(Size.X, 0.04f);
		Box.Y = std::max(Size.Y, 0.04f);
		Box.Z = std::max(Size.Z, 0.04f);
		Setup->AggGeom.BoxElems.Add(Box);
		Setup->DefaultInstance.CollisionEnabled = ECollisionEnabled::QueryOnly;
		Setup->DefaultInstance.MassInKgOverride = std::clamp(Box.X * Box.Y * Box.Z * 1000.0f, 0.5f, 15.0f);
		Asset.SkeletalBodySetups.Add(Setup);
		HasBody[Index] = 1;
	}
	if (Asset.SkeletalBodySetups.IsEmpty())
	{
		OutError = "No bones have enough skinned vertices to create physics bodies";
		return false;
	}
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		if (!HasBody[Index]) continue;
		int32 Parent = Bones[Index].ParentIndex;
		while (Parent >= 0 && !HasBody[Parent]) Parent = Bones[Parent].ParentIndex;
		if (Parent < 0) continue;
		auto* Template = NewObject<UPhysicsConstraintTemplate>(&Asset);
		Template->DefaultInstance.ConstraintBone1 = Bones[Index].Name;
		Template->DefaultInstance.ConstraintBone2 = Bones[Parent].Name;
		Asset.ConstraintSetup.Add(Template);
	}
	return Asset.Validate(Mesh, OutError);
}
