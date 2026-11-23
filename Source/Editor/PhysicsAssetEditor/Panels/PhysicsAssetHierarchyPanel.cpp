#include "EnginePCH.h"
#include "PhysicsAssetHierarchyPanel.h"
#include "Editor/PhysicsAssetEditor/PhysicsAssetEditor.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Render/SkeletalMesh.h"

FPhysicsAssetHierarchyPanel::FPhysicsAssetHierarchyPanel(FPhysicsAssetEditor& InEditor)
	: FPhysicsAssetEditorPanel(InEditor, "Skeleton Tree")
{
}

void FPhysicsAssetHierarchyPanel::DrawContent()
{
	if (!Editor.GetMesh())
	{
		LastSelectedBone = -1;
		LastSelectedBody = -1;
		ImGui::TextDisabled("No preview skeletal mesh.");
		return;
	}

	EditorStyle::SearchBox("##PhysicsAssetBoneSearch", SearchBuffer, sizeof(SearchBuffer));
	ImGui::TextDisabled("Bodies: %d", Editor.GetPreviewBodies().Num());
	ImGui::TextWrapped("Right-click a bone to add a body.");
	const TArray<FEditorBoneItem>& Bones = Editor.GetBoneItems();
	if (Bones.IsEmpty())
	{
		ImGui::TextDisabled("No bones.");
		return;
	}

	TArray<int32> BodyByBone;
	BodyByBone.Init(-1, Bones.Num());
	const TArray<FPhysicsAssetPreviewBody>& Bodies = Editor.GetPreviewBodies();
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		const int32 BoneIndex = Bodies[Index].BoneIndex;
		if (BoneIndex >= 0 && BoneIndex < Bones.Num()) BodyByBone[BoneIndex] = Index;
	}

	TArray<uint8> VisibleBones;
	VisibleBones.Init(0, Bones.Num());
	const TArray<FBoneInfo>& RefBones = Editor.GetMesh()->GetRefSkeleton();
	for (int32 Index = 0; Index < Bones.Num(); ++Index)
	{
		const FString BodyLabel = FString("Body: ") + Bones[Index].Name;
		VisibleBones[Index] = SearchBuffer[0] == '\0' || EditorStyle::ContainsIgnoreCase(Bones[Index].Name.c_str(), SearchBuffer) ||
			(BodyByBone[Index] >= 0 && EditorStyle::ContainsIgnoreCase(BodyLabel.c_str(), SearchBuffer));
	}
	// 본 배열은 부모가 먼저 저장되므로 뒤에서부터 전파하면 검색 결과의 조상이 유지됩니다.
	for (int32 Index = Bones.Num() - 1; Index >= 0; --Index)
	{
		const int32 Parent = RefBones[Index].ParentIndex;
		if (VisibleBones[Index] && Parent >= 0 && Parent < Bones.Num()) VisibleBones[Parent] = 1;
	}

	const int32 SelectedBone = Editor.GetSelectedBoneIndex();
	const int32 SelectedBody = Editor.GetSelectedBodyIndex();
	const bool bRevealSelection = LastSelectedBone != SelectedBone || LastSelectedBody != SelectedBody;
	LastSelectedBone = SelectedBone;
	LastSelectedBody = SelectedBody;
	bool bScrollToSelection = bRevealSelection && SelectedBone >= 0;
	TArray<uint8> SelectionAncestors;
	SelectionAncestors.Init(0, Bones.Num());
	for (int32 Index = SelectedBone; Index >= 0 && Index < Bones.Num() && !SelectionAncestors[Index]; Index = RefBones[Index].ParentIndex)
		SelectionAncestors[Index] = 1;

	if (ImGui::BeginTable("##PhysicsAssetHierarchy", 1,
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp))
	{
		bool bAnyVisible = false;
		for (int32 RootIndex : Editor.GetRootBoneIndices())
		{
			if (RootIndex >= 0 && RootIndex < Bones.Num() && VisibleBones[RootIndex])
			{
				bAnyVisible = true;
				DrawBoneNode(RootIndex, BodyByBone, VisibleBones, SelectionAncestors, bRevealSelection, bScrollToSelection);
			}
		}
		if (!bAnyVisible)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("No matching bones or bodies.");
		}
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
			Editor.SelectBone(-1);
		ImGui::EndTable();
	}
}

void FPhysicsAssetHierarchyPanel::DrawBoneNode(int32 BoneIndex, const TArray<int32>& BodyByBone,
	const TArray<uint8>& VisibleBones, const TArray<uint8>& SelectionAncestors,
	bool bRevealSelection, bool& bScrollToSelection)
{
	const TArray<FEditorBoneItem>& Bones = Editor.GetBoneItems();
	const FEditorBoneItem& Bone = Bones[BoneIndex];
	const int32 BodyIndex = BodyByBone[BoneIndex];
	const FString BodyLabel = FString("Body: ") + Bone.Name;
	const bool bShowBody = BodyIndex >= 0 &&
		(SearchBuffer[0] == '\0' || EditorStyle::ContainsIgnoreCase(BodyLabel.c_str(), SearchBuffer));
	bool bHasChildren = bShowBody;
	for (int32 Child : Bone.ChildBoneIndices)
		if (Child >= 0 && Child < Bones.Num() && VisibleBones[Child]) bHasChildren = true;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::PushID(BoneIndex);
	const bool bBoneSelected = Editor.GetSelectedBodyIndex() < 0 && Editor.GetSelectedBoneIndex() == BoneIndex;
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
	if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (bBoneSelected) Flags |= ImGuiTreeNodeFlags_Selected;
	if (bHasChildren && (SearchBuffer[0] != '\0' || (bRevealSelection && SelectionAncestors[BoneIndex])))
		ImGui::SetNextItemOpen(true);
	const bool bOpen = ImGui::TreeNodeEx("##Bone", Flags, "%s", Bone.Name.c_str());
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) Editor.SelectBone(BoneIndex);
	if (bBoneSelected && bScrollToSelection)
	{
		ImGui::SetScrollHereY(0.5f);
		bScrollToSelection = false;
	}
	if (ImGui::BeginPopupContextItem("##BoneContext"))
	{
		if (ImGui::BeginMenu("Add Body", BodyIndex < 0))
		{
			if (ImGui::MenuItem("Capsule")) Editor.AddBody(BoneIndex, FPhysicsAssetEditor::EPreviewShapeType::Capsule);
			if (ImGui::MenuItem("Sphere")) Editor.AddBody(BoneIndex, FPhysicsAssetEditor::EPreviewShapeType::Sphere);
			if (ImGui::MenuItem("Box")) Editor.AddBody(BoneIndex, FPhysicsAssetEditor::EPreviewShapeType::Box);
			ImGui::EndMenu();
		}
		if (BodyIndex >= 0 && ImGui::MenuItem("Select Body")) Editor.SelectBody(BodyIndex);
		ImGui::EndPopup();
	}

	if (bOpen && bHasChildren)
	{
		if (bShowBody)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			const bool bBodySelected = Editor.GetSelectedBodyIndex() == BodyIndex;
			const ImGuiTreeNodeFlags BodyFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth | (bBodySelected ? ImGuiTreeNodeFlags_Selected : 0);
			ImGui::TreeNodeEx("##Body", BodyFlags, "%s", BodyLabel.c_str());
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) Editor.SelectBody(BodyIndex);
			if (bBodySelected && bScrollToSelection)
			{
				ImGui::SetScrollHereY(0.5f);
				bScrollToSelection = false;
			}
		}
		for (int32 Child : Bone.ChildBoneIndices)
		{
			if (Child >= 0 && Child < Bones.Num() && VisibleBones[Child])
				DrawBoneNode(Child, BodyByBone, VisibleBones, SelectionAncestors, bRevealSelection, bScrollToSelection);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}
