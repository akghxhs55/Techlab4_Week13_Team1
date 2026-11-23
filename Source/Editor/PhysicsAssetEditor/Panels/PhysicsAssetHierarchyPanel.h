#pragma once

#include "PhysicsAssetEditorPanel.h"
#include "Containers/Array.h"

// 피직스 에셋 에디터의 본 계층과 선택 상태를 표시합니다.
class FPhysicsAssetHierarchyPanel : public FPhysicsAssetEditorPanel
{
public:
	explicit FPhysicsAssetHierarchyPanel(FPhysicsAssetEditor& InEditor);

protected:
	void DrawContent() override;

private:
	void DrawBoneNode(int32 BoneIndex, const TArray<int32>& BodyByBone,
		const TArray<uint8>& VisibleBones, const TArray<uint8>& SelectionAncestors,
		bool bRevealSelection, bool& bScrollToSelection);
	char SearchBuffer[128] = {};
	int32 LastSelectedBone = -1;
	int32 LastSelectedBody = -1;
};
