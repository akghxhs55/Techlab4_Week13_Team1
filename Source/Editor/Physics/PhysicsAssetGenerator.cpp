#include "EnginePCH.h"
#include "Editor/Physics/PhysicsAssetGenerator.h"

#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsBoneUtils.h"
#include "Render/SkeletalMesh.h"

namespace
{
	float GetBoneLength(const TArray<FBoneInfo>& Bones, int32 Bone)
	{
		float Length = 0.0f;
		for (int32 C = Bone + 1; C < Bones.Num(); ++C)
		{
			if (Bones[C].ParentIndex != Bone) continue;
			const FVector& L = Bones[C].LocalBindPose.Location;
			if (std::abs(L.X) + std::abs(L.Y) + std::abs(L.Z) < 1e-5f) continue;
			Length = FMath::Max(Length, L.Length());
		}
		return Length;
	}

	int32 FindDominantBone(const FSkinWeight& Weight, float MinWeight)
	{
		float MaxWeight = 0.0f;
		int32 Dominant = -1;
		for (int32 I = 0; I < MaxBoneInfluences; ++I)
		{
			const float W = Weight.Weight[I];
			if (W > 0.0f && W >= MinWeight && W > MaxWeight)
			{
				MaxWeight = W;
				Dominant = Weight.BoneIndex[I];
			}
		}
		return Dominant;
	}

	int32 FindNearestBodyAncestor(const TArray<FBoneInfo>& Bones, const TArray<uint8>& HasBody, int32 Bone)
	{
		for (int32 P = Bones[Bone].ParentIndex; P >= 0; P = Bones[P].ParentIndex)
		{
			if (HasBody[P]) return P;
		}
		return -1;
	}

	TArray<uint8> GetBodyBonesFromAsset(const UPhysicsAsset& Asset, const TArray<FBoneInfo>& Bones)
	{
		TArray<uint8> HasBody;
		HasBody.Init(0, Bones.Num());
		for (const UBodySetup* Body : Asset.BodySetups)
		{
			if (!Body) continue;
			for (int32 B = 0; B < Bones.Num(); ++B)
			{
				if (Bones[B].Name == Body->BoneName) { HasBody[B] = 1; break; }
			}
		}
		return HasBody;
	}

	TArray<TArray<FVector>> GatherBodyPoints(const FPhysicsAssetGenInput& In, const TArray<uint8>& HasBody, float MinWeight)
	{
		const TArray<FBoneInfo>& Bones = *In.Bones;
		TArray<TArray<FVector>> Points;
		Points.SetNum(Bones.Num());
		for (int32 V = 0; V < In.BindVertices->Num(); ++V)
		{
			const int32 Dominant = FindDominantBone((*In.SkinWeights)[V], MinWeight);
			if (Dominant < 0) continue;

			const int32 Target = HasBody[Dominant] ? Dominant : FindNearestBodyAncestor(Bones, HasBody, Dominant);
			if (Target < 0) continue;

			Points[Target].Add(Bones[Target].InverseBindPose.TransformPosition((*In.BindVertices)[V].Position));
		}
		return Points;
	}

	float Percentile(TArray<float> Values, float Fraction)
	{
		const int32 Num = Values.Num();
		const int32 K = FMath::Clamp(static_cast<int32>(Fraction * static_cast<float>(Num - 1)), 0, Num - 1);
		float* Data = Values.GetData();
		std::nth_element(Data, Data + K, Data + Num);
		return Data[K];
	}

	// 캡슐 축은 로컬 Z(BodyInstance의 SphylBasis), Length는 양 끝 반구를 뺀 원통 길이
	FCapsuleElem FitCapsule(const TArray<FVector>& LocalPoints, const TArray<FBoneInfo>& Bones, int32 Bone, const FPhysicsAssetGenSettings& Settings)
	{
		constexpr int32 MinFitPoints = 4;
		constexpr float MinRadius = 2.0f;
		constexpr float AxialTrim = 0.02f;

		const FVector Axis = FPhysicsBoneUtils::FindBoneAxis(Bones, Bone);
		FCapsuleElem Capsule{};

		const FVector Z(0.0f, 0.0f, 1.0f);
		const float Dot = FMath::Clamp(Z.Dot(Axis), -1.0f, 1.0f);
		if (Dot > 0.9999f) Capsule.Rotation = FRotator();
		else if (Dot < -0.9999f) Capsule.Rotation = FQuat::MakeFromAxisAngle(FVector(1.0f, 0.0f, 0.0f), PI).ToFRotator();
		else Capsule.Rotation = FQuat::MakeFromAxisAngle(Z.Cross(Axis), std::acos(Dot)).ToFRotator();

		const int32 Num = LocalPoints.Num();
		if (Num < MinFitPoints)
		{
			const float BoneLength = GetBoneLength(Bones, Bone);
			Capsule.Radius = FMath::Max(MinRadius, 0.15f * BoneLength);
			Capsule.Length = FMath::Max(0.0f, BoneLength - 2.0f * Capsule.Radius);
			Capsule.Center = Axis * (BoneLength * 0.5f);
			return Capsule;
		}

		TArray<float> AxialPositions;
		AxialPositions.SetNum(Num);
		FVector PerpSum(0.0f, 0.0f, 0.0f);
		for (int32 I = 0; I < Num; ++I)
		{
			AxialPositions[I] = LocalPoints[I].Dot(Axis);
			PerpSum += LocalPoints[I] - Axis * AxialPositions[I];
		}
		const FVector PerpCenter = PerpSum / static_cast<float>(Num);

		TArray<float> RadialDistances;
		RadialDistances.SetNum(Num);
		for (int32 I = 0; I < Num; ++I)
		{
			RadialDistances[I] = (LocalPoints[I] - Axis * AxialPositions[I] - PerpCenter).Length();
		}

		const float TMin = Percentile(AxialPositions, AxialTrim);
		const float TMax = Percentile(AxialPositions, 1.0f - AxialTrim);
		const float Extent = FMath::Max(0.0f, TMax - TMin);

		float Radius = Settings.RadiusScale * Percentile(RadialDistances, Settings.RadiusPercentile);
		Radius = FMath::Max(MinRadius, FMath::Min(Radius, 0.5f * Extent));

		Capsule.Radius = Radius;
		Capsule.Length = FMath::Max(0.0f, Extent - 2.0f * Radius);
		Capsule.Center = Axis * (0.5f * (TMin + TMax)) + PerpCenter;
		return Capsule;
	}
}

bool FPhysicsAssetGenInput::IsValid(FString& OutError) const
{
	if (!Bones || !BindVertices || !SkinWeights) { OutError = "PhysicsAssetGen: input arrays are missing"; return false; }
	if (Bones->IsEmpty()) { OutError = "PhysicsAssetGen: mesh has no bones"; return false; }
	if (BindVertices->Num() != SkinWeights->Num()) { OutError = "PhysicsAssetGen: vertex and skin weight counts differ"; return false; }
	for (const FSkinWeight& Weight : *SkinWeights)
	{
		for (int32 I = 0; I < MaxBoneInfluences; ++I)
		{
			if (Weight.Weight[I] > 0.0f && Weight.BoneIndex[I] >= Bones->Num()) { OutError = "PhysicsAssetGen: skin weight refers to a missing bone"; return false; }
		}
	}
	return true;
}

FPhysicsAssetGenInput FPhysicsAssetGenInput::FromImportData(const FSkeletalMeshData& Data)
{
	return { &Data.Bones, &Data.BindVertices, &Data.SkinWeight };
}

FPhysicsAssetGenInput FPhysicsAssetGenInput::FromMesh(const USkeletalMesh& Mesh)
{
	FPhysicsAssetGenInput Input;
	Input.Bones = &Mesh.GetRefSkeleton();
	const auto& LODs = Mesh.GetRenderData().LODRenderData;
	if (!LODs.IsEmpty())
	{
		Input.BindVertices = &LODs[0].BindVertices;
		Input.SkinWeights = &LODs[0].SkinWeights;
	}
	return Input;
}

bool FPhysicsAssetGenerator::Generate(UPhysicsAsset& Out, const FPhysicsAssetGenInput& In, const FPhysicsAssetGenSettings& Settings, TArray<FString>& OutWarnings)
{
	FString Error;
	if (!In.IsValid(Error))
	{
		OutWarnings.Add(Error);
		return false;
	}
	const TArray<FBoneInfo>& Bones = *In.Bones;
	const int32 NumBones = Bones.Num();

	Out.ClearBodies();
	Out.Constraints.Empty();

	TArray<int32> DominantCounts;
	DominantCounts.Init(0, NumBones);
	for (const FSkinWeight& Weight : *In.SkinWeights)
	{
		const int32 Dominant = FindDominantBone(Weight, Settings.MinWeight);
		if (Dominant >= 0) ++DominantCounts[Dominant];
	}

	TArray<uint8> HasBody;
	HasBody.Init(0, NumBones);
	for (int32 B = 0; B < NumBones; ++B)
	{
		HasBody[B] = GetBoneLength(Bones, B) >= Settings.MinBoneLength && DominantCounts[B] >= Settings.MinVertexCount;
	}

	int32 NumRoots = 0;
	int32 CommonAncestor = -1;
	for (int32 B = 0; B < NumBones && (NumRoots == 0 || CommonAncestor >= 0); ++B)
	{
		if (!HasBody[B] || FindNearestBodyAncestor(Bones, HasBody, B) >= 0) continue;
		if (NumRoots++ == 0) { CommonAncestor = B; continue; }

		int32 A = CommonAncestor;
		int32 C = B;
		while (A != C && A >= 0 && C >= 0)
		{
			if (A > C) A = Bones[A].ParentIndex;
			else C = Bones[C].ParentIndex;
		}
		CommonAncestor = A == C ? A : -1;
	}
	if (NumRoots > 1)
	{
		if (CommonAncestor >= 0)
		{
			HasBody[CommonAncestor] = 1;
			OutWarnings.Add("PhysicsAssetGen: multiple root bodies; added a body to common ancestor '" + Bones[CommonAncestor].Name + "'");
		}
		else
		{
			OutWarnings.Add("PhysicsAssetGen: root bodies have no common ancestor bone; the skeleton has several root bones");
		}
	}

	const TArray<TArray<FVector>> Points = GatherBodyPoints(In, HasBody, Settings.MinWeight);
	for (int32 B = 0; B < NumBones; ++B)
	{
		if (!HasBody[B]) continue;
		UBodySetup* Body = NewObject<UBodySetup>(&Out);
		Body->BoneName = Bones[B].Name;
		Body->AggGeom.CapsuleElems.Add(FitCapsule(Points[B], Bones, B, Settings));
		Out.BodySetups.Add(Body);
	}

	// 빈 에셋이 저장되면 다음 Import부터 자동 생성을 건너뛴다.
	if (Out.BodySetups.IsEmpty())
	{
		OutWarnings.Add("PhysicsAssetGen: no bone qualified for a body");
		return false;
	}

	if (Settings.bCreateConstraints)
	{
		RebuildConstraints(Out, Bones);
		if (!Out.Validate(Bones, "PhysicsAssetGen"))
		{
			OutWarnings.Add("PhysicsAssetGen: generated asset failed validation (see [PhysicsAsset] log)");
			return false;
		}
	}
	return true;
}

UBodySetup* FPhysicsAssetGenerator::GenerateBodyForBone(UPhysicsAsset& Asset, const FPhysicsAssetGenInput& In, int32 BoneIndex, const FPhysicsAssetGenSettings& Settings)
{
	FString Error;
	if (!In.IsValid(Error) || BoneIndex < 0 || BoneIndex >= In.Bones->Num()) return nullptr;
	const TArray<FBoneInfo>& Bones = *In.Bones;

	TArray<uint8> HasBody = GetBodyBonesFromAsset(Asset, Bones);
	HasBody[BoneIndex] = 1;
	const TArray<TArray<FVector>> Points = GatherBodyPoints(In, HasBody, Settings.MinWeight);
	const FCapsuleElem Capsule = FitCapsule(Points[BoneIndex], Bones, BoneIndex, Settings);

	UBodySetup* Body = nullptr;
	for (UBodySetup* Existing : Asset.BodySetups)
	{
		if (Existing && Existing->BoneName == Bones[BoneIndex].Name) { Body = Existing; break; }
	}
	if (!Body)
	{
		Body = NewObject<UBodySetup>(&Asset);
		Body->BoneName = Bones[BoneIndex].Name;
		Asset.BodySetups.Add(Body);
	}
	Body->AggGeom.EmptyElements();
	Body->AggGeom.CapsuleElems.Add(Capsule);
	return Body;
}

void FPhysicsAssetGenerator::RebuildConstraints(UPhysicsAsset& Asset, const TArray<FBoneInfo>& Bones)
{
	const TArray<uint8> HasBody = GetBodyBonesFromAsset(Asset, Bones);
	const TArray<FPhysicsAssetConstraint> Previous = Asset.Constraints;
	Asset.Constraints.Empty();

	for (int32 B = 0; B < Bones.Num(); ++B)
	{
		if (!HasBody[B]) continue;
		const int32 Parent = FindNearestBodyAncestor(Bones, HasBody, B);
		if (Parent < 0) continue;

		FPhysicsAssetConstraint Constraint;
		for (const FPhysicsAssetConstraint& Old : Previous)
		{
			if (Old.ChildBone == Bones[B].Name) { Constraint = Old; break; }
		}
		Constraint.ChildBone = Bones[B].Name;
		Constraint.ParentBone = Bones[Parent].Name;
		Asset.Constraints.Add(Constraint);
	}
}
