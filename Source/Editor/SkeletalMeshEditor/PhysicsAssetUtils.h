#pragma once

#include "Core/EngineString.h"

class USkeletalMesh;
class UPhysicsAsset;

struct FPhysAssetCreateParams
{
	float MinBoneSize = 0.08f;
	int32 MinVertexCount = 8;
};

struct FPhysicsAssetUtils
{
	static bool CreateFromSkeletalMesh(const USkeletalMesh& Mesh, UPhysicsAsset& Asset,
		const FPhysAssetCreateParams& Params, FString& OutError);
};
