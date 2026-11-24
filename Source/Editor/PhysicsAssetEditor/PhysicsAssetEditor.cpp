#include "EnginePCH.h"
#include "PhysicsAssetEditor.h"
#include "PhysicsAssetEditorTab.h"
#include "Components/SkeletalMeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "Render/LineBatcher.h"

#include <cmath>
#include <cfloat>

namespace
{
	int32 GetShapeCount(const FAggregateGeom& Geometry, FPhysicsAssetEditor::EPreviewShapeType Type)
	{
		switch (Type)
		{
		case FPhysicsAssetEditor::EPreviewShapeType::Sphere: return Geometry.SphereElems.Num();
		case FPhysicsAssetEditor::EPreviewShapeType::Box: return Geometry.BoxElems.Num();
		case FPhysicsAssetEditor::EPreviewShapeType::Capsule: return Geometry.CapsuleElems.Num();
		default: return 0;
		}
	}

	// 로컬 레이의 방향을 정규화하지 않아 교차 매개변수 t를 월드 레이와 공유합니다.
	bool RayHitsSphere(const FRay& Ray, const FVector& Center, float Radius, float& OutT)
	{
		const FVector Offset = Ray.Origin - Center;
		const float C = Offset.Dot(Offset) - Radius * Radius;
		if (C <= 0.0f) { OutT = 0.0f; return true; }
		const float A = Ray.Direction.Dot(Ray.Direction);
		const float B = Offset.Dot(Ray.Direction);
		if (A <= 0.0f) return false;
		const float Discriminant = B * B - A * C;
		if (Discriminant < 0.0f) return false;
		OutT = (-B - std::sqrt(Discriminant)) / A;
		return OutT >= 0.0f;
	}

	bool RayHitsBox(const FRay& Ray, const FVector& HalfExtent, float& OutT)
	{
		float Enter = 0.0f;
		float Exit = FLT_MAX;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Origin = Ray.Origin[Axis];
			const float Direction = Ray.Direction[Axis];
			const float Extent = HalfExtent[Axis];
			if (Direction == 0.0f)
			{
				if (Origin < -Extent || Origin > Extent) return false;
				continue;
			}
			const float T0 = (-Extent - Origin) / Direction;
			const float T1 = (Extent - Origin) / Direction;
			Enter = FMath::Max(Enter, FMath::Min(T0, T1));
			Exit = FMath::Min(Exit, FMath::Max(T0, T1));
			if (Enter > Exit) return false;
		}
		OutT = Enter;
		return true;
	}

	bool RayHitsCapsule(const FRay& Ray, float Radius, float Length, float& OutT)
	{
		const float HalfLength = Length * 0.5f;
		const float ClosestZ = FMath::Clamp(Ray.Origin.Z, -HalfLength, HalfLength);
		const FVector Offset = Ray.Origin - FVector(0.0f, 0.0f, ClosestZ);
		if (Offset.Dot(Offset) <= Radius * Radius) { OutT = 0.0f; return true; }

		float Nearest = FLT_MAX;
		float T;
		for (float CenterZ : { -HalfLength, HalfLength })
		{
			if (RayHitsSphere(Ray, FVector(0.0f, 0.0f, CenterZ), Radius, T))
				Nearest = FMath::Min(Nearest, T);
		}
		const float A = Ray.Direction.X * Ray.Direction.X + Ray.Direction.Y * Ray.Direction.Y;
		const float B = Ray.Origin.X * Ray.Direction.X + Ray.Origin.Y * Ray.Direction.Y;
		const float C = Ray.Origin.X * Ray.Origin.X + Ray.Origin.Y * Ray.Origin.Y - Radius * Radius;
		const float Discriminant = B * B - A * C;
		if (A > 0.0f && Discriminant >= 0.0f)
		{
			const float Root = std::sqrt(Discriminant);
			for (float Candidate : { (-B - Root) / A, (-B + Root) / A })
			{
				const float Z = Ray.Origin.Z + Candidate * Ray.Direction.Z;
				if (Candidate >= 0.0f && Z >= -HalfLength && Z <= HalfLength)
					Nearest = FMath::Min(Nearest, Candidate);
			}
		}
		OutT = Nearest;
		return Nearest < FLT_MAX;
	}

	void DrawWireArc(FLineBatcher& Lines, const FMatrix& ShapeToWorld,
		const FVector& Center, const FVector& AxisA, const FVector& AxisB,
		float Radius, float StartAngle, float EndAngle, const FVector4& Color)
	{
		constexpr int32 Segments = 24;
		const auto PointAt = [&](float Angle)
		{
			return ShapeToWorld.TransformPosition(Center +
				AxisA * (std::cos(Angle) * Radius) + AxisB * (std::sin(Angle) * Radius));
		};

		FVector Previous = PointAt(StartAngle);
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = StartAngle + (EndAngle - StartAngle) * static_cast<float>(Index) / Segments;
			const FVector Current = PointAt(Angle);
			Lines.AddLine(Previous, Current, Color);
			Previous = Current;
		}
	}

	void DrawAggregateGeometry(FLineBatcher& Lines, const FAggregateGeom& Geometry,
		const FMatrix& BoneToWorld, const FVector4& Color, const FShapeElem* SelectedShape, const FVector4& SelectionColor)
	{
		const FVector X(1.0f, 0.0f, 0.0f);
		const FVector Y(0.0f, 1.0f, 0.0f);
		const FVector Z(0.0f, 0.0f, 1.0f);

		for (const FSphereElem& Sphere : Geometry.SphereElems)
		{
			const FVector4 ShapeColor = &Sphere == SelectedShape ? SelectionColor : Color;
			if (!std::isfinite(Sphere.Radius) || Sphere.Radius <= 0.0f) continue;
			const FMatrix ShapeToWorld = FTransform(Sphere.Rotation, Sphere.Center).GetLocalMatrix() * BoneToWorld;
			DrawWireArc(Lines, ShapeToWorld, FVector::ZeroVector, X, Y, Sphere.Radius, 0.0f, 2.0f * PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, FVector::ZeroVector, Y, Z, Sphere.Radius, 0.0f, 2.0f * PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, FVector::ZeroVector, Z, X, Sphere.Radius, 0.0f, 2.0f * PI, ShapeColor);
		}

		for (const FBoxElem& Box : Geometry.BoxElems)
		{
			if (!std::isfinite(Box.X) || !std::isfinite(Box.Y) || !std::isfinite(Box.Z) ||
				Box.X <= 0.0f || Box.Y <= 0.0f || Box.Z <= 0.0f) continue;
			// FBoxElem의 X/Y/Z는 전체 길이입니다.
			const FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);
			const FMatrix ShapeToWorld = FTransform(Box.Rotation, Box.Center).GetLocalMatrix() * BoneToWorld;
			Lines.AddOrientedBox(FBox{ HalfExtent * -1.0f, HalfExtent }, ShapeToWorld,
				&Box == SelectedShape ? SelectionColor : Color);
		}

		for (const FCapsuleElem& Capsule : Geometry.CapsuleElems)
		{
			const FVector4 ShapeColor = &Capsule == SelectedShape ? SelectionColor : Color;
			if (!std::isfinite(Capsule.Radius) || !std::isfinite(Capsule.Length) ||
				Capsule.Radius <= 0.0f || Capsule.Length < 0.0f) continue;
			const FMatrix ShapeToWorld = FTransform(Capsule.Rotation, Capsule.Center).GetLocalMatrix() * BoneToWorld;
			// 캡슐은 로컬 Z축 방향이며 Length는 양쪽 반구 중심 사이의 거리입니다.
			const FVector Top = Z * (Capsule.Length * 0.5f);
			const FVector Bottom = Top * -1.0f;
			DrawWireArc(Lines, ShapeToWorld, Top, X, Y, Capsule.Radius, 0.0f, 2.0f * PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, Bottom, X, Y, Capsule.Radius, 0.0f, 2.0f * PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, Top, X, Z, Capsule.Radius, 0.0f, PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, Top, Y, Z, Capsule.Radius, 0.0f, PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, Bottom, X, Z, Capsule.Radius, 0.0f, -PI, ShapeColor);
			DrawWireArc(Lines, ShapeToWorld, Bottom, Y, Z, Capsule.Radius, 0.0f, -PI, ShapeColor);
			for (const FVector& Side : { X, X * -1.0f, Y, Y * -1.0f })
			{
				Lines.AddLine(ShapeToWorld.TransformPosition(Top + Side * Capsule.Radius),
					ShapeToWorld.TransformPosition(Bottom + Side * Capsule.Radius), ShapeColor);
			}
		}
	}
}

FPhysicsAssetEditor::FPhysicsAssetEditor(
	UEditorEngine& InEditorEngine,
	FEditorManager& InEditorManager,
	const FString& InAssetPath,
	USkeletalMesh& InPreviewMesh
)
	: FSkeletalPreviewEditor(InEditorEngine, InEditorManager, InAssetPath)
	, PreviewDocumentKey(MakePreviewDocumentKey(InAssetPath))
{
	Mesh = &InPreviewMesh;
}

FPhysicsAssetEditor::~FPhysicsAssetEditor() = default;

void FPhysicsAssetEditor::OnRenderLine(FLineBatcher& LineBatcher)
{
	FSkeletalPreviewEditor::OnRenderLine(LineBatcher);
}

void FPhysicsAssetEditor::OnRenderDebugOverlay(const FSceneView* View)
{
	if (!View || !Mesh || !MeshComponent || !PhysicsBodyLines) return;
	PhysicsBodyLines->BeginFrame();
	// 물리 형상 전용 배치에서 깊이를 검사하고, 장면 깊이에는 쓰지 않습니다.
	PhysicsBodyLines->SetDepthTest(bBodyDepthTest, false);
	const FShapeElem* SelectedShape = GetSelectedShape();
	for (int32 Index = 0; Index < PreviewBodies.Num(); ++Index)
	{
		if (!IsBodyVisible(Index)) continue;
		const FPhysicsAssetPreviewBody& Body = PreviewBodies[Index];
		if (Body.BoneIndex < 0 || Body.BoneIndex >= Mesh->GetNumBones()) continue;
		const FVector4& Color = Index == SelectedBodyIndex ? SelectedBodyColor : BodyColor;
		DrawAggregateGeometry(*PhysicsBodyLines, Body.Geometry, MeshComponent->GetBoneWorldMatrix(Body.BoneIndex),
			Color, SelectedShape, SelectedShapeColor);
	}
	PhysicsBodyLines->OnRender(View->ViewProjectionMatrix);
}

bool FPhysicsAssetEditor::IsBodyVisible(int32 BodyIndex) const
{
	return BodyIndex >= 0 && BodyIndex < PreviewBodies.Num() && BodyDrawMode != EBodyDrawMode::None &&
		(BodyDrawMode != EBodyDrawMode::SelectedOnly || BodyIndex == SelectedBodyIndex);
}

void FPhysicsAssetEditor::SetBodyDrawMode(EBodyDrawMode Mode)
{
	if (Mode == BodyDrawMode) return;
	CancelViewportInteraction();
	BodyDrawMode = Mode;
	SyncShapeGizmo();
}

TUniquePtr<IEditorTab> FPhysicsAssetEditor::CreateTab()
{
	return MakeUnique<FPhysicsAssetEditorTab>(*this);
}

bool FPhysicsAssetEditor::InitializeResources()
{
	if (!InitializePreview())
	{
		return false;
	}

	PreviewBodies.Empty();
	SelectedBodyIndex = -1;
	PhysicsBodyLines = MakeUnique<FLineBatcher>();
	if (!PhysicsBodyLines->Init(nullptr, PreviewScene.GetWorld())) return false;
	ShapeGizmoTarget = Cast<USceneComponent>(PreviewScene.AddComponent(USceneComponent::StaticClass()));
	if (!ShapeGizmoTarget) return false;
	ShapeGizmoTarget->DetachFromParent();
	return true;
}

void FPhysicsAssetEditor::ShutdownResources()
{
	CancelViewportInteraction();
	ShapeGizmo.SetTarget(nullptr);
	ShapeGizmoTarget = nullptr;
	SelectedShapeType = EPreviewShapeType::None;
	SelectedShapeIndex = -1;
	SelectedBodyIndex = -1;
	PreviewBodies.Empty();
	PhysicsBodyLines.reset();
	FSkeletalPreviewEditor::ShutdownResources();
}

int32 FPhysicsAssetEditor::FindBodyByBone(int32 BoneIndex) const
{
	for (int32 Index = 0; Index < PreviewBodies.Num(); ++Index)
		if (PreviewBodies[Index].BoneIndex == BoneIndex) return Index;
	return -1;
}

const FPhysicsAssetPreviewBody* FPhysicsAssetEditor::GetSelectedBody() const
{
	return SelectedBodyIndex >= 0 && SelectedBodyIndex < PreviewBodies.Num()
		? &PreviewBodies[SelectedBodyIndex] : nullptr;
}

int32 FPhysicsAssetEditor::AddBody(int32 BoneIndex, EPreviewShapeType ShapeType)
{
	if (!Mesh || !MeshComponent || BoneIndex < 0 || BoneIndex >= Mesh->GetNumBones() ||
		(ShapeType != EPreviewShapeType::Sphere && ShapeType != EPreviewShapeType::Box && ShapeType != EPreviewShapeType::Capsule))
		return -1;
	const int32 ExistingIndex = FindBodyByBone(BoneIndex);
	if (ExistingIndex >= 0)
	{
		SelectBody(ExistingIndex);
		return ExistingIndex;
	}

	// 재할당 전에 진행 중인 조작을 정리합니다. 선택은 포인터 대신 인덱스로 보관합니다.
	CancelViewportInteraction();
	const int32 BodyIndex = PreviewBodies.Num();
	FPhysicsAssetPreviewBody& Body = PreviewBodies.Emplace_GetRef();
	Body.BoneIndex = BoneIndex;
	++TopologyRevision;
	AddShape(BodyIndex, ShapeType);
	return BodyIndex;
}

bool FPhysicsAssetEditor::RemoveBody(int32 BodyIndex)
{
	if (BodyIndex < 0 || BodyIndex >= PreviewBodies.Num()) return false;
	CancelViewportInteraction();
	ShapeGizmo.SetTarget(nullptr);
	if (SelectedBodyIndex == BodyIndex)
	{
		SelectedBodyIndex = -1;
		SelectedShapeType = EPreviewShapeType::None;
		SelectedShapeIndex = -1;
	}
	else if (SelectedBodyIndex > BodyIndex)
	{
		--SelectedBodyIndex;
	}
	PreviewBodies.RemoveAt(static_cast<uint32>(BodyIndex));
	++TopologyRevision;
	SyncShapeGizmo();
	return true;
}

int32 FPhysicsAssetEditor::AddShape(int32 BodyIndex, EPreviewShapeType ShapeType)
{
	if (BodyIndex < 0 || BodyIndex >= PreviewBodies.Num() ||
		(ShapeType != EPreviewShapeType::Sphere && ShapeType != EPreviewShapeType::Box && ShapeType != EPreviewShapeType::Capsule))
		return -1;
	CancelViewportInteraction();
	ShapeGizmo.SetTarget(nullptr);
	FAggregateGeom& Geometry = PreviewBodies[BodyIndex].Geometry;
	const int32 ShapeIndex = GetShapeCount(Geometry, ShapeType);
	const float Radius = FMath::Max(BoneSphereSize * 2.0f, 0.01f);
	FShapeElem* Shape = nullptr;
	if (ShapeType == EPreviewShapeType::Sphere)
	{
		FSphereElem& Sphere = Geometry.SphereElems.Emplace_GetRef();
		Sphere.Radius = Radius;
		Shape = &Sphere;
	}
	else if (ShapeType == EPreviewShapeType::Box)
	{
		FBoxElem& Box = Geometry.BoxElems.Emplace_GetRef();
		Box.X = Box.Y = Box.Z = Radius * 2.0f;
		Shape = &Box;
	}
	else
	{
		FCapsuleElem& Capsule = Geometry.CapsuleElems.Emplace_GetRef();
		Capsule.Radius = Radius;
		Capsule.Length = Radius * 4.0f;
		Shape = &Capsule;
	}
	Shape->Center = FVector::ZeroVector;
	Shape->Rotation = FRotator(0.0f, 0.0f, 0.0f);
	++TopologyRevision;
	SelectShape(BodyIndex, ShapeType, ShapeIndex);
	return ShapeIndex;
}

bool FPhysicsAssetEditor::RemoveShape(int32 BodyIndex, EPreviewShapeType ShapeType, int32 ShapeIndex)
{
	if (BodyIndex < 0 || BodyIndex >= PreviewBodies.Num() || ShapeIndex < 0 ||
		ShapeIndex >= GetShapeCount(PreviewBodies[BodyIndex].Geometry, ShapeType)) return false;
	CancelViewportInteraction();
	ShapeGizmo.SetTarget(nullptr);
	FAggregateGeom& Geometry = PreviewBodies[BodyIndex].Geometry;
	switch (ShapeType)
	{
	case EPreviewShapeType::Sphere: Geometry.SphereElems.RemoveAt(static_cast<uint32>(ShapeIndex)); break;
	case EPreviewShapeType::Box: Geometry.BoxElems.RemoveAt(static_cast<uint32>(ShapeIndex)); break;
	case EPreviewShapeType::Capsule: Geometry.CapsuleElems.RemoveAt(static_cast<uint32>(ShapeIndex)); break;
	default: return false;
	}
	if (SelectedBodyIndex == BodyIndex && SelectedShapeType == ShapeType)
	{
		if (SelectedShapeIndex == ShapeIndex)
		{
			SelectedShapeType = EPreviewShapeType::None;
			SelectedShapeIndex = -1;
		}
		else if (SelectedShapeIndex > ShapeIndex)
		{
			--SelectedShapeIndex;
		}
	}
	++TopologyRevision;
	SyncShapeGizmo();
	return true;
}

bool FPhysicsAssetEditor::SelectShape(int32 BodyIndex, EPreviewShapeType ShapeType, int32 ShapeIndex)
{
	if (BodyIndex < 0 || BodyIndex >= PreviewBodies.Num() || ShapeIndex < 0 ||
		ShapeIndex >= GetShapeCount(PreviewBodies[BodyIndex].Geometry, ShapeType)) return false;
	SelectBody(BodyIndex);
	SelectedShapeType = ShapeType;
	SelectedShapeIndex = ShapeIndex;
	SyncShapeGizmo();
	return true;
}

void FPhysicsAssetEditor::SelectBody(int32 BodyIndex)
{
	CancelViewportInteraction();
	SelectedBodyIndex = -1;
	SelectedShapeType = EPreviewShapeType::None;
	SelectedShapeIndex = -1;
	ShapeGizmo.SetTarget(nullptr);
	if (BodyIndex < 0 || BodyIndex >= PreviewBodies.Num()) return;
	const FPhysicsAssetPreviewBody& Body = PreviewBodies[BodyIndex];
	// SelectBone의 훅에서 이전 선택을 정리한 후 새 바디를 선택합니다.
	SelectBone(Body.BoneIndex);
	SelectedBodyIndex = BodyIndex;
	if (!Body.Geometry.CapsuleElems.IsEmpty()) SelectedShapeType = EPreviewShapeType::Capsule;
	else if (!Body.Geometry.BoxElems.IsEmpty()) SelectedShapeType = EPreviewShapeType::Box;
	else if (!Body.Geometry.SphereElems.IsEmpty()) SelectedShapeType = EPreviewShapeType::Sphere;
	if (SelectedShapeType != EPreviewShapeType::None) SelectedShapeIndex = 0;
	SyncShapeGizmo();
}

FShapeElem* FPhysicsAssetEditor::GetSelectedShape()
{
	if (!GetSelectedBody() || SelectedShapeIndex < 0) return nullptr;
	FAggregateGeom& Geometry = PreviewBodies[SelectedBodyIndex].Geometry;
	switch (SelectedShapeType)
	{
	case EPreviewShapeType::Sphere:
		return SelectedShapeIndex < Geometry.SphereElems.Num() ? &Geometry.SphereElems[SelectedShapeIndex] : nullptr;
	case EPreviewShapeType::Box:
		return SelectedShapeIndex < Geometry.BoxElems.Num() ? &Geometry.BoxElems[SelectedShapeIndex] : nullptr;
	case EPreviewShapeType::Capsule:
		return SelectedShapeIndex < Geometry.CapsuleElems.Num() ? &Geometry.CapsuleElems[SelectedShapeIndex] : nullptr;
	default: return nullptr;
	}
}

const FShapeElem* FPhysicsAssetEditor::GetSelectedShape() const
{
	const FPhysicsAssetPreviewBody* Body = GetSelectedBody();
	if (!Body || SelectedShapeIndex < 0) return nullptr;
	const FAggregateGeom& Geometry = Body->Geometry;
	switch (SelectedShapeType)
	{
	case EPreviewShapeType::Sphere:
		return SelectedShapeIndex < Geometry.SphereElems.Num() ? &Geometry.SphereElems[SelectedShapeIndex] : nullptr;
	case EPreviewShapeType::Box:
		return SelectedShapeIndex < Geometry.BoxElems.Num() ? &Geometry.BoxElems[SelectedShapeIndex] : nullptr;
	case EPreviewShapeType::Capsule:
		return SelectedShapeIndex < Geometry.CapsuleElems.Num() ? &Geometry.CapsuleElems[SelectedShapeIndex] : nullptr;
	default: return nullptr;
	}
}

FVector FPhysicsAssetEditor::GetSelectedShapeSize() const
{
	if (!GetSelectedShape()) return FVector::ZeroVector;
	const FAggregateGeom& Geometry = PreviewBodies[SelectedBodyIndex].Geometry;
	switch (SelectedShapeType)
	{
	case EPreviewShapeType::Sphere:
		return FVector(Geometry.SphereElems[SelectedShapeIndex].Radius, 0.0f, 0.0f);
	case EPreviewShapeType::Box:
	{
		const FBoxElem& Box = Geometry.BoxElems[SelectedShapeIndex];
		return FVector(Box.X, Box.Y, Box.Z);
	}
	case EPreviewShapeType::Capsule:
	{
		const FCapsuleElem& Capsule = Geometry.CapsuleElems[SelectedShapeIndex];
		return FVector(Capsule.Radius, Capsule.Length, 0.0f);
	}
	default: return FVector::ZeroVector;
	}
}

void FPhysicsAssetEditor::SetSelectedShapeSize(const FVector& Size)
{
	if (!GetSelectedShape()) return;
	FAggregateGeom& Geometry = PreviewBodies[SelectedBodyIndex].Geometry;
	switch (SelectedShapeType)
	{
	case EPreviewShapeType::Sphere:
		Geometry.SphereElems[SelectedShapeIndex].Radius = Size.X;
		break;
	case EPreviewShapeType::Box:
	{
		FBoxElem& Box = Geometry.BoxElems[SelectedShapeIndex];
		Box.X = Size.X; Box.Y = Size.Y; Box.Z = Size.Z;
		break;
	}
	case EPreviewShapeType::Capsule:
		Geometry.CapsuleElems[SelectedShapeIndex].Radius = Size.X;
		Geometry.CapsuleElems[SelectedShapeIndex].Length = Size.Y;
		break;
	default: break;
	}
}

bool FPhysicsAssetEditor::GetSelectedShapeProperties(FShapeElem& OutShape, FVector& OutSize, EPreviewShapeType& OutType) const
{
	const FShapeElem* Shape = GetSelectedShape();
	if (!Shape) return false;
	OutShape = *Shape;
	OutSize = GetSelectedShapeSize();
	OutType = SelectedShapeType;
	return true;
}

bool FPhysicsAssetEditor::SetSelectedShapeProperties(const FShapeElem& Shape, const FVector& Size)
{
	if (!GetSelectedShape() ||
		!std::isfinite(Shape.Center.X) || !std::isfinite(Shape.Center.Y) || !std::isfinite(Shape.Center.Z) ||
		!std::isfinite(Shape.Rotation.Pitch) || !std::isfinite(Shape.Rotation.Yaw) || !std::isfinite(Shape.Rotation.Roll) ||
		!std::isfinite(Size.X) || !std::isfinite(Size.Y) || !std::isfinite(Size.Z))
		return false;

	FVector ValidSize = Size;
	constexpr float MinDimension = 0.001f;
	switch (SelectedShapeType)
	{
	case EPreviewShapeType::Sphere:
		ValidSize = FVector(FMath::Max(Size.X, MinDimension), 0.0f, 0.0f);
		break;
	case EPreviewShapeType::Box:
		ValidSize = FVector(FMath::Max(Size.X, MinDimension), FMath::Max(Size.Y, MinDimension), FMath::Max(Size.Z, MinDimension));
		break;
	case EPreviewShapeType::Capsule:
		ValidSize = FVector(FMath::Max(Size.X, MinDimension), FMath::Max(Size.Y, 0.0f), 0.0f);
		break;
	default: return false;
	}

	// 디테일과 기즈모가 같은 형상을 수정하므로 이전 드래그 상태를 먼저 정리합니다.
	CancelViewportInteraction();
	*GetSelectedShape() = Shape;
	SetSelectedShapeSize(ValidSize);
	SyncShapeGizmo();
	return true;
}

void FPhysicsAssetEditor::SetGizmoMode(EGizmoMode Mode)
{
	if (ShapeGizmo.GetMode() == Mode) return;
	CancelViewportInteraction();
	ShapeGizmo.SetMode(Mode);
	SyncShapeGizmo();
}

void FPhysicsAssetEditor::CancelViewportInteraction()
{
	if (ShapeGizmo.IsUsing())
	{
		if (FShapeElem* Shape = GetSelectedShape())
		{
			*Shape = DragStartShape;
			SetSelectedShapeSize(DragStartSize);
		}
	}
	ShapeGizmo.EndDrag();
}

void FPhysicsAssetEditor::OnBoneSelected()
{
	CancelViewportInteraction();
	SelectedBodyIndex = -1;
	SelectedShapeType = EPreviewShapeType::None;
	SelectedShapeIndex = -1;
	ShapeGizmo.SetTarget(nullptr);
}

void FPhysicsAssetEditor::TickAfterWorld(float DeltaTime)
{
	if (!UpdateShapeGizmo() && !UpdateShapePicking())
		UpdateBonePicking();
	SyncShapeGizmo();
}

bool FPhysicsAssetEditor::UpdateShapePicking()
{
	if (!CanProcessViewportInput() || !Mesh || !MeshComponent || !PointerInput.bHovered ||
		!PointerInput.bMousePressed || PointerInput.bCancel || CameraInput.bCaptured) return false;

	const FVector2 MousePosition(PointerInput.MouseUV.X * ViewportWidth, PointerInput.MouseUV.Y * ViewportHeight);
	const FRay WorldRay = ViewportClient->DeprojectScreenToWorld(MousePosition);
	float Nearest = FLT_MAX;
	int32 HitBodyIndex = -1;
	EPreviewShapeType HitShapeType = EPreviewShapeType::None;
	int32 HitShapeIndex = -1;
	for (int32 BodyIndex = 0; BodyIndex < PreviewBodies.Num(); ++BodyIndex)
	{
		if (!IsBodyVisible(BodyIndex)) continue;
		const FPhysicsAssetPreviewBody& Body = PreviewBodies[BodyIndex];
		if (Body.BoneIndex < 0 || Body.BoneIndex >= Mesh->GetNumBones()) continue;
		const FMatrix BoneToWorld = MeshComponent->GetBoneWorldMatrix(Body.BoneIndex);
		const auto TestShape = [&](const FShapeElem& Shape, EPreviewShapeType Type, int32 Index, const auto& Trace)
		{
			const FMatrix ShapeToWorld = FTransform(Shape.Rotation, Shape.Center).GetLocalMatrix() * BoneToWorld;
			const float Determinant = ShapeToWorld.Determinant();
			if (!std::isfinite(Determinant) || std::abs(Determinant) < 1e-12f) return;
			float T;
			if (Trace(ToLocalRay(WorldRay, ShapeToWorld), T) && std::isfinite(T) && T >= 0.0f && T < Nearest)
			{
				Nearest = T;
				HitBodyIndex = BodyIndex;
				HitShapeType = Type;
				HitShapeIndex = Index;
			}
		};
		for (int32 Index = 0; Index < Body.Geometry.SphereElems.Num(); ++Index)
		{
			const FSphereElem& Sphere = Body.Geometry.SphereElems[Index];
			if (!std::isfinite(Sphere.Radius) || Sphere.Radius <= 0.0f) continue;
			TestShape(Sphere, EPreviewShapeType::Sphere, Index, [&](const FRay& Ray, float& T)
			{ return RayHitsSphere(Ray, FVector::ZeroVector, Sphere.Radius, T); });
		}
		for (int32 Index = 0; Index < Body.Geometry.BoxElems.Num(); ++Index)
		{
			const FBoxElem& Box = Body.Geometry.BoxElems[Index];
			if (!std::isfinite(Box.X) || !std::isfinite(Box.Y) || !std::isfinite(Box.Z) ||
				Box.X <= 0.0f || Box.Y <= 0.0f || Box.Z <= 0.0f) continue;
			TestShape(Box, EPreviewShapeType::Box, Index, [&](const FRay& Ray, float& T)
			{ return RayHitsBox(Ray, FVector(Box.X, Box.Y, Box.Z) * 0.5f, T); });
		}
		for (int32 Index = 0; Index < Body.Geometry.CapsuleElems.Num(); ++Index)
		{
			const FCapsuleElem& Capsule = Body.Geometry.CapsuleElems[Index];
			if (!std::isfinite(Capsule.Radius) || !std::isfinite(Capsule.Length) ||
				Capsule.Radius <= 0.0f || Capsule.Length < 0.0f) continue;
			TestShape(Capsule, EPreviewShapeType::Capsule, Index, [&](const FRay& Ray, float& T)
			{ return RayHitsCapsule(Ray, Capsule.Radius, Capsule.Length, T); });
		}
	}
	if (HitBodyIndex >= 0) return SelectShape(HitBodyIndex, HitShapeType, HitShapeIndex);
	SelectBody(-1);
	return false;
}

FQuat FPhysicsAssetEditor::GetPreviewBoneWorldRotation() const
{
	FQuat Rotation = FQuat::Identity();
	const FPhysicsAssetPreviewBody* Body = GetSelectedBody();
	if (!Body || !Mesh || !MeshComponent) return Rotation;
	const TArray<FBoneInfo>& Bones = Mesh->GetRefSkeleton();
	for (int32 Index = Body->BoneIndex; Index >= 0 && Index < Bones.Num(); Index = Bones[Index].ParentIndex)
		Rotation = MeshComponent->GetBoneLocalTransform(Index).Rotation * Rotation;
	return (MeshComponent->GetWorldRotation().Quaternion() * Rotation).Normalized();
}

void FPhysicsAssetEditor::SyncShapeGizmo()
{
	const FShapeElem* Shape = GetSelectedShape();
	const FPhysicsAssetPreviewBody* Body = GetSelectedBody();
	if (!Shape || !Body || !IsBodyVisible(SelectedBodyIndex) || !ShapeGizmoTarget || !Mesh || !MeshComponent ||
		Body->BoneIndex < 0 || Body->BoneIndex >= Mesh->GetNumBones())
	{
		ShapeGizmo.SetTarget(nullptr);
		return;
	}
	if (ShapeGizmo.IsUsing()) return;
	const FVector Location = MeshComponent->GetBoneWorldMatrix(Body->BoneIndex).TransformPosition(Shape->Center);
	const FQuat Rotation = (GetPreviewBoneWorldRotation() * Shape->Rotation.Quaternion()).Normalized();
	// 기즈모 스케일은 드래그 배율이며, 실제 크기는 형상의 Radius/Length/X/Y/Z에 저장합니다.
	ShapeGizmoTarget->SetTransform(FTransform(Rotation, Location));
	ShapeGizmo.SetTarget(ShapeGizmoTarget);
}

bool FPhysicsAssetEditor::UpdateShapeGizmo()
{
	if (!CanProcessViewportInput() || PointerInput.bCancel)
	{
		const bool bWasUsing = ShapeGizmo.IsUsing();
		CancelViewportInteraction();
		return bWasUsing;
	}
	SyncShapeGizmo();
	if (!ShapeGizmo.GetTarget()) return false;
	const bool bWasUsing = ShapeGizmo.IsUsing();
	if ((!PointerInput.bHovered && !bWasUsing) || CameraInput.bCaptured) return false;

	const FVector2 MousePosition(PointerInput.MouseUV.X * ViewportWidth, PointerInput.MouseUV.Y * ViewportHeight);
	ShapeGizmo.Update(ViewportClient->DeprojectScreenToWorld(MousePosition), MousePosition,
		ViewportClient->GetViewProjectionMatrix(), static_cast<int>(ViewportWidth), static_cast<int>(ViewportHeight),
		PointerInput.bMouseDown, ViewportClient->GetViewLocation(), !ViewportClient->IsPerspective(), PointerInput.bMousePressed);
	if (!bWasUsing && ShapeGizmo.IsUsing())
	{
		const FMatrix BoneToWorld = MeshComponent->GetBoneWorldMatrix(GetSelectedBody()->BoneIndex);
		if (!std::isfinite(BoneToWorld.Determinant()) || std::abs(BoneToWorld.Determinant()) < 1e-12f)
		{
			ShapeGizmo.EndDrag();
			return true;
		}
		DragStartShape = *GetSelectedShape();
		DragStartSize = GetSelectedShapeSize();
		DragScaleAxis = ShapeGizmo.GetHoveredAxis();
		DragBoneInverse = BoneToWorld.Inverse();
		DragBoneRotation = GetPreviewBoneWorldRotation();
	}
	// Update()가 버튼을 놓는 프레임에 드래그를 끝내므로 마지막 변환도 반영합니다.
	if (bWasUsing || ShapeGizmo.IsUsing()) ApplyShapeGizmoTransform();
	return bWasUsing || ShapeGizmo.IsUsing() || ShapeGizmo.GetHoveredAxis() >= 0;
}

void FPhysicsAssetEditor::ApplyShapeGizmoTransform()
{
	FShapeElem* Shape = GetSelectedShape();
	if (!Shape || !ShapeGizmoTarget) return;
	switch (ShapeGizmo.GetMode())
	{
	case EGizmoMode::Location:
		Shape->Center = DragBoneInverse.TransformPosition(ShapeGizmoTarget->GetWorldLocation());
		break;
	case EGizmoMode::Rotation:
		Shape->Rotation = (DragBoneRotation.Inverse() * ShapeGizmoTarget->GetWorldRotation().Quaternion()).Normalized().ToFRotator();
		break;
	case EGizmoMode::Scale:
	{
		const FVector RawScale = ShapeGizmoTarget->GetRelativeScale3D();
		if (!std::isfinite(RawScale.X) || !std::isfinite(RawScale.Y) || !std::isfinite(RawScale.Z)) return;
		const FVector Scale(FMath::Max(RawScale.X, 0.01f), FMath::Max(RawScale.Y, 0.01f), FMath::Max(RawScale.Z, 0.01f));
		FVector Size = DragStartSize;
		if (SelectedShapeType == EPreviewShapeType::Sphere)
			Size.X = FMath::Max(0.001f, Size.X * Scale[DragScaleAxis >= 0 && DragScaleAxis < 3 ? DragScaleAxis : 0]);
		else if (SelectedShapeType == EPreviewShapeType::Box)
			Size = FVector(FMath::Max(0.001f, Size.X * Scale.X), FMath::Max(0.001f, Size.Y * Scale.Y), FMath::Max(0.001f, Size.Z * Scale.Z));
		else if (SelectedShapeType == EPreviewShapeType::Capsule)
		{
			// X 또는 Y 핸들은 원형 단면 전체를 조절하고, Z 핸들은 높이만 조절합니다.
			const float RadiusScale = DragScaleAxis == 2 ? 1.0f : (DragScaleAxis == 1 ? Scale.Y : Scale.X);
			Size.X = FMath::Max(0.001f, DragStartSize.X * RadiusScale);
			const float HalfHeight = (DragStartSize.Y * 0.5f + DragStartSize.X) * Scale.Z;
			Size.Y = FMath::Max(0.0f, 2.0f * (HalfHeight - Size.X));
		}
		if (std::isfinite(Size.X) && std::isfinite(Size.Y) && std::isfinite(Size.Z))
			SetSelectedShapeSize(Size);
		break;
	}
	default: break;
	}
}
