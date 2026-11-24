#include "EnginePCH.h"
#include "PhysicsAssetHierarchyPanel.h"
#include "Editor/PhysicsAssetEditor/PhysicsAssetEditor.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Render/SkeletalMesh.h"

#include <cstdio>
#include <utility>

namespace
{
	bool ShapeMatchesFilter(const char* Name, int32 Index, const char* Filter)
	{
		char Label[64];
		std::snprintf(Label, sizeof(Label), "%s %d", Name, Index + 1);
		return !Filter[0] || EditorStyle::ContainsIgnoreCase(Label, Filter);
	}

	bool BodyMatchesFilter(const FPhysicsAssetPreviewBody& Body, const FString& BoneName, const char* Filter)
	{
		if (!Filter[0] || EditorStyle::ContainsIgnoreCase((FString("Body: ") + BoneName).c_str(), Filter)) return true;
		for (int32 Index = 0; Index < Body.Geometry.CapsuleElems.Num(); ++Index)
			if (ShapeMatchesFilter("Capsule", Index, Filter)) return true;
		for (int32 Index = 0; Index < Body.Geometry.SphereElems.Num(); ++Index)
			if (ShapeMatchesFilter("Sphere", Index, Filter)) return true;
		for (int32 Index = 0; Index < Body.Geometry.BoxElems.Num(); ++Index)
			if (ShapeMatchesFilter("Box", Index, Filter)) return true;
		return false;
	}
}

FPhysicsAssetHierarchyPanel::FPhysicsAssetHierarchyPanel(FPhysicsAssetEditor& InEditor)
	: FPhysicsAssetEditorPanel(InEditor, "Skeleton Tree")
{
}

void FPhysicsAssetHierarchyPanel::DrawContent()
{
	PendingEdit = {};
	if (!Editor.GetMesh())
	{
		LastSelectedBone = -1;
		LastSelectedBody = -1;
		ImGui::TextDisabled("No preview skeletal mesh.");
		return;
	}

	EditorStyle::SearchBox("##PhysicsAssetBoneSearch", SearchBuffer, sizeof(SearchBuffer));
	bool bViewChanged = ImGui::Checkbox("Bodies Only", &bBodiesOnly);
	bViewChanged |= ImGui::Checkbox("Show Shapes", &bShowShapes);
	ImGui::TextDisabled("Bodies: %d", Editor.GetPreviewBodies().Num());
	ImGui::TextWrapped(bBodiesOnly ? "Right-click bodies or shapes to edit. Turn off Bodies Only to add a body to another bone." :
		"Right-click bones, bodies or shapes to add or delete.");
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
		VisibleBones[Index] = (!bBodiesOnly && (SearchBuffer[0] == '\0' || EditorStyle::ContainsIgnoreCase(Bones[Index].Name.c_str(), SearchBuffer))) ||
			(BodyByBone[Index] >= 0 && BodyMatchesFilter(Bodies[BodyByBone[Index]], Bones[Index].Name, SearchBuffer));
	}
	// 본 배열은 부모가 먼저 저장되므로 뒤에서부터 전파하면 검색 결과의 조상이 유지됩니다.
	for (int32 Index = Bones.Num() - 1; Index >= 0; --Index)
	{
		const int32 Parent = RefBones[Index].ParentIndex;
		if (VisibleBones[Index] && Parent >= 0 && Parent < Bones.Num()) VisibleBones[Parent] = 1;
	}

	const int32 SelectedBone = Editor.GetSelectedBoneIndex();
	const int32 SelectedBody = Editor.GetSelectedBodyIndex();
	const int32 SelectedShapeType = static_cast<int32>(Editor.GetSelectedShapeType());
	const int32 SelectedShapeIndex = Editor.GetSelectedShapeIndex();
	const bool bRevealSelection = bViewChanged || LastSelectedBone != SelectedBone || LastSelectedBody != SelectedBody ||
		LastSelectedShapeType != SelectedShapeType || LastSelectedShapeIndex != SelectedShapeIndex ||
		LastTopologyRevision != Editor.GetTopologyRevision();
	LastSelectedBone = SelectedBone;
	LastSelectedBody = SelectedBody;
	LastSelectedShapeType = SelectedShapeType;
	LastSelectedShapeIndex = SelectedShapeIndex;
	LastTopologyRevision = Editor.GetTopologyRevision();
	bool bScrollToSelection = bRevealSelection && SelectedBone >= 0;
	TArray<uint8> SelectionAncestors;
	SelectionAncestors.Init(0, Bones.Num());
	for (int32 Index = SelectedBone; Index >= 0 && Index < Bones.Num() && !SelectionAncestors[Index]; Index = RefBones[Index].ParentIndex)
		SelectionAncestors[Index] = 1;

	if (ImGui::BeginTable(bBodiesOnly ? "##PhysicsAssetBodyHierarchy" : "##PhysicsAssetHierarchy", 1,
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp))
	{
		bool bAnyVisible = false;
		if (bBodiesOnly)
		{
			TArray<TArray<int32>> BodyChildren;
			BodyChildren.SetNum(Bodies.Num());
			TArray<int32> RootBodies;
			TArray<int32> NearestBody;
			NearestBody.Init(-1, Bones.Num());
			// 바디가 없는 중간 본을 건너뛰고 가장 가까운 조상 바디에 연결합니다.
			for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
			{
				const int32 ParentBone = RefBones[BoneIndex].ParentIndex;
				const int32 ParentBody = ParentBone >= 0 && ParentBone < BoneIndex ? NearestBody[ParentBone] : -1;
				const int32 BodyIndex = BodyByBone[BoneIndex];
				NearestBody[BoneIndex] = BodyIndex >= 0 ? BodyIndex : ParentBody;
				if (BodyIndex < 0) continue;
				if (ParentBody >= 0) BodyChildren[ParentBody].Add(BodyIndex);
				else RootBodies.Add(BodyIndex);
			}
			const FBodyTreeContext BodyTree{ BodyChildren, VisibleBones, SelectionAncestors };
			for (int32 BodyIndex : RootBodies)
			{
				if (!VisibleBones[Bodies[BodyIndex].BoneIndex]) continue;
				bAnyVisible = true;
				DrawBodyNode(BodyIndex, bRevealSelection, bScrollToSelection, &BodyTree);
			}
		}
		else
		{
			for (int32 RootIndex : Editor.GetRootBoneIndices())
			{
				if (RootIndex >= 0 && RootIndex < Bones.Num() && VisibleBones[RootIndex])
				{
					bAnyVisible = true;
					DrawBoneNode(RootIndex, BodyByBone, VisibleBones, SelectionAncestors, bRevealSelection, bScrollToSelection);
				}
			}
		}
		if (!bAnyVisible)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (bBodiesOnly && Bodies.IsEmpty())
				ImGui::TextDisabled("No bodies. Turn off Bodies Only to add a body.");
			else
				ImGui::TextDisabled(bBodiesOnly ? "No matching bodies." : "No matching bones or bodies.");
		}
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
			Editor.SelectBone(-1);
		ImGui::EndTable();
	}
	// 순회가 끝난 뒤 한 번만 적용하여 본과 바디의 인덱스 대응을 도중에 바꾸지 않습니다.
	std::function<void()> Edit = std::move(PendingEdit);
	PendingEdit = {};
	if (Edit) Edit();
}

void FPhysicsAssetHierarchyPanel::DrawBoneNode(int32 BoneIndex, const TArray<int32>& BodyByBone,
	const TArray<uint8>& VisibleBones, const TArray<uint8>& SelectionAncestors,
	bool bRevealSelection, bool& bScrollToSelection)
{
	const TArray<FEditorBoneItem>& Bones = Editor.GetBoneItems();
	const FEditorBoneItem& Bone = Bones[BoneIndex];
	const int32 BodyIndex = BodyByBone[BoneIndex];
	const bool bShowBody = BodyIndex >= 0 &&
		BodyMatchesFilter(Editor.GetPreviewBodies()[BodyIndex], Bone.Name, SearchBuffer);
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
			if (ImGui::MenuItem("Capsule")) PendingEdit = [this, BoneIndex] { Editor.AddBody(BoneIndex, FPhysicsAssetEditor::EPreviewShapeType::Capsule); };
			if (ImGui::MenuItem("Sphere")) PendingEdit = [this, BoneIndex] { Editor.AddBody(BoneIndex, FPhysicsAssetEditor::EPreviewShapeType::Sphere); };
			if (ImGui::MenuItem("Box")) PendingEdit = [this, BoneIndex] { Editor.AddBody(BoneIndex, FPhysicsAssetEditor::EPreviewShapeType::Box); };
			ImGui::EndMenu();
		}
		if (BodyIndex >= 0 && ImGui::MenuItem("Select Body")) Editor.SelectBody(BodyIndex);
		if (BodyIndex >= 0 && ImGui::MenuItem("Delete Body")) PendingEdit = [this, BodyIndex] { Editor.RemoveBody(BodyIndex); };
		ImGui::EndPopup();
	}

	if (bOpen && bHasChildren)
	{
		if (bShowBody)
		{
			DrawBodyNode(BodyIndex, bRevealSelection, bScrollToSelection);
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

void FPhysicsAssetHierarchyPanel::DrawBodyNode(int32 BodyIndex, bool bRevealSelection, bool& bScrollToSelection,
	const FBodyTreeContext* BodyTree)
{
	const FPhysicsAssetPreviewBody& Body = Editor.GetPreviewBodies()[BodyIndex];
	const FString BodyLabel = FString("Body: ") + Editor.GetBoneItems()[Body.BoneIndex].Name;
	const bool bBodySelected = Editor.GetSelectedBodyIndex() == BodyIndex;
	const bool bHasShapes = bShowShapes && Body.Geometry.GetElementCount() > 0;
	bool bHasChildren = bHasShapes;
	if (BodyTree)
	{
		for (int32 Child : BodyTree->Children[BodyIndex])
			if (BodyTree->VisibleBones[Editor.GetPreviewBodies()[Child].BoneIndex]) bHasChildren = true;
	}
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::PushID(Body.BoneIndex);
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
	if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (bBodySelected) Flags |= ImGuiTreeNodeFlags_Selected;
	const bool bSelectedDescendant = BodyTree && BodyTree->SelectionAncestors[Body.BoneIndex];
	if (bHasChildren && (SearchBuffer[0] || (bRevealSelection && (bBodySelected || bSelectedDescendant)))) ImGui::SetNextItemOpen(true);
	const bool bOpen = ImGui::TreeNodeEx("##Body", Flags, "%s", BodyLabel.c_str());
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) Editor.SelectBody(BodyIndex);
	if (bBodySelected && (!bShowShapes || Editor.GetSelectedShapeType() == FPhysicsAssetEditor::EPreviewShapeType::None) && bScrollToSelection)
	{
		ImGui::SetScrollHereY(0.5f);
		bScrollToSelection = false;
	}
	if (ImGui::BeginPopupContextItem("##BodyContext"))
	{
		if (ImGui::BeginMenu("Add Shape"))
		{
			if (ImGui::MenuItem("Capsule")) PendingEdit = [this, BodyIndex] { Editor.AddShape(BodyIndex, FPhysicsAssetEditor::EPreviewShapeType::Capsule); };
			if (ImGui::MenuItem("Sphere")) PendingEdit = [this, BodyIndex] { Editor.AddShape(BodyIndex, FPhysicsAssetEditor::EPreviewShapeType::Sphere); };
			if (ImGui::MenuItem("Box")) PendingEdit = [this, BodyIndex] { Editor.AddShape(BodyIndex, FPhysicsAssetEditor::EPreviewShapeType::Box); };
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Delete Body")) PendingEdit = [this, BodyIndex] { Editor.RemoveBody(BodyIndex); };
		ImGui::EndPopup();
	}
	if (bOpen && bHasChildren)
	{
		const bool bShowAllShapes = !SearchBuffer[0] || EditorStyle::ContainsIgnoreCase(BodyLabel.c_str(), SearchBuffer);
		const auto DrawShape = [&](FPhysicsAssetEditor::EPreviewShapeType Type, int32 Index, const char* Name)
		{
			if (!bShowAllShapes && !ShapeMatchesFilter(Name, Index, SearchBuffer)) return;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(static_cast<int>(Type));
			ImGui::PushID(Index);
			const bool bSelected = Editor.GetSelectedBodyIndex() == BodyIndex && Editor.GetSelectedShapeType() == Type &&
				Editor.GetSelectedShapeIndex() == Index;
			const ImGuiTreeNodeFlags ShapeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth | (bSelected ? ImGuiTreeNodeFlags_Selected : 0);
			ImGui::TreeNodeEx("##Shape", ShapeFlags, "%s %d", Name, Index + 1);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) Editor.SelectShape(BodyIndex, Type, Index);
			if (bSelected && bScrollToSelection)
			{
				ImGui::SetScrollHereY(0.5f);
				bScrollToSelection = false;
			}
			if (ImGui::BeginPopupContextItem("##ShapeContext"))
			{
				if (ImGui::MenuItem("Delete Shape")) PendingEdit = [this, BodyIndex, Type, Index] { Editor.RemoveShape(BodyIndex, Type, Index); };
				ImGui::EndPopup();
			}
			ImGui::PopID();
			ImGui::PopID();
		};
		if (bHasShapes)
		{
			for (int32 Index = 0; Index < Body.Geometry.CapsuleElems.Num(); ++Index)
				DrawShape(FPhysicsAssetEditor::EPreviewShapeType::Capsule, Index, "Capsule");
			for (int32 Index = 0; Index < Body.Geometry.SphereElems.Num(); ++Index)
				DrawShape(FPhysicsAssetEditor::EPreviewShapeType::Sphere, Index, "Sphere");
			for (int32 Index = 0; Index < Body.Geometry.BoxElems.Num(); ++Index)
				DrawShape(FPhysicsAssetEditor::EPreviewShapeType::Box, Index, "Box");
		}
		if (BodyTree)
		{
			for (int32 Child : BodyTree->Children[BodyIndex])
			{
				if (BodyTree->VisibleBones[Editor.GetPreviewBodies()[Child].BoneIndex])
					DrawBodyNode(Child, bRevealSelection, bScrollToSelection, BodyTree);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}
