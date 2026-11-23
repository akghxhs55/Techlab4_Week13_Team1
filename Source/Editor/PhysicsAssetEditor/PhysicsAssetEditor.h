#pragma once

#include "Editor/SkeletalPreviewEditor/SkeletalPreviewEditor.h"
#include "Physics/AggregateGeom.h"
#include "Editor/Gizmo/Gizmo.h"

// 런타임 피직스 에셋에 연결하기 전의 에디터용 바디 설정입니다.
struct FPhysicsAssetPreviewBody
{
	int32 BoneIndex = -1;
	FAggregateGeom Geometry;
};

// 피직스 에셋 편집에 사용할 스켈레탈 메시 미리보기 에디터입니다.
class FPhysicsAssetEditor : public FSkeletalPreviewEditor
{
public:
	enum class EPreviewShapeType { None, Sphere, Box, Capsule };

	FPhysicsAssetEditor(
		UEditorEngine& InEditorEngine,
		FEditorManager& InEditorManager,
		const FString& InAssetPath,
		USkeletalMesh& InPreviewMesh
	);

	void OnRenderLine(FLineBatcher& LineBatcher) override;
	const FGizmo* GetPreviewGizmo() const override { return ShapeGizmo.GetTarget() ? &ShapeGizmo : nullptr; }
	void SetGizmoMode(EGizmoMode Mode);
	EGizmoMode GetGizmoMode() const { return ShapeGizmo.GetMode(); }
	bool IsGizmoDragging() const { return ShapeGizmo.IsUsing(); }
	int32 AddBody(int32 BoneIndex, EPreviewShapeType ShapeType = EPreviewShapeType::Capsule);
	int32 FindBodyByBone(int32 BoneIndex) const;
	void SelectBody(int32 BodyIndex);
	int32 GetSelectedBodyIndex() const { return SelectedBodyIndex; }
	const FPhysicsAssetPreviewBody* GetSelectedBody() const;
	const TArray<FPhysicsAssetPreviewBody>& GetPreviewBodies() const { return PreviewBodies; }

	const char* GetEditorTypeName() const override { return "PhysicsAsset"; }
	static FString MakePreviewDocumentKey(const FString& MeshPath) { return MeshPath + ".physicsasset-preview"; }
	const FString& GetDocumentKey() const override { return PreviewDocumentKey; }
	bool UsesPreviewSkybox() const override { return true; }

protected:
	TUniquePtr<IEditorTab> CreateTab() override;
	bool InitializeResources() override;
	void ShutdownResources() override;
	void TickAfterWorld(float DeltaTime) override;
	bool IsViewportInteractionBusy() const override { return ShapeGizmo.IsUsing(); }
	void OnBoneSelected() override;
	void CancelViewportInteraction() override;

private:
	FString PreviewDocumentKey;
	FShapeElem* GetSelectedShape();
	const FShapeElem* GetSelectedShape() const;
	FVector GetSelectedShapeSize() const;
	void SetSelectedShapeSize(const FVector& Size);
	bool UpdateShapePicking();
	bool UpdateShapeGizmo();
	void SyncShapeGizmo();
	void ApplyShapeGizmoTransform();
	FQuat GetPreviewBoneWorldRotation() const;

	// 각 바디는 생성된 본을 유지하며, 본 선택과 바디 선택은 별도로 관리합니다.
	TArray<FPhysicsAssetPreviewBody> PreviewBodies;
	int32 SelectedBodyIndex = -1;
	EPreviewShapeType SelectedShapeType = EPreviewShapeType::None;
	int32 SelectedShapeIndex = -1;
	USceneComponent* ShapeGizmoTarget = nullptr;
	FGizmo ShapeGizmo;
	FShapeElem DragStartShape;
	FVector DragStartSize = FVector::ZeroVector;
	FMatrix DragBoneInverse;
	FQuat DragBoneRotation = FQuat::Identity();
	int32 DragScaleAxis = -1;
};
