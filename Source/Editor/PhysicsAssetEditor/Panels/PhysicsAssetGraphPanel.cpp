#include "EnginePCH.h"
#include "PhysicsAssetGraphPanel.h"
#include "Editor/PhysicsAssetEditor/PhysicsAssetEditor.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/BodySetup.h"
#include "Render/SkeletalMesh.h"

#include <cmath>

FPhysicsAssetGraphPanel::FPhysicsAssetGraphPanel(FPhysicsAssetEditor& InEditor)
	: FPhysicsAssetEditorPanel(InEditor, "Constraint Graph")
{
}

void FPhysicsAssetGraphPanel::DrawContent()
{
	const int32 ChildBody = Editor.GetSelectedBodyIndex();
	const int32 ParentBody = Editor.GetNearestParentBody(ChildBody);
	if (ImGui::Button("Fit Graph")) bFitRequested = true;
	ImGui::SameLine();
	ImGui::BeginDisabled(!Editor.CanAddConstraint(ParentBody, ChildBody));
	if (ImGui::Button("Connect to Parent")) Editor.AddConstraint(ParentBody, ChildBody);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!Editor.GetSelectedConstraint() || Editor.IsSimulationActive());
	if (ImGui::Button("Delete Constraint")) Editor.RemoveConstraint(Editor.GetSelectedConstraintIndex());
	ImGui::EndDisabled();
	ImGui::TextWrapped("Select a child body and connect it to its nearest parent body. Middle/right drag: pan. Wheel: zoom.");

	const TArray<FPhysicsAssetPreviewBody>& Bodies = Editor.GetPreviewBodies();
	const UPhysicsAsset* Asset = Editor.GetPhysicsAsset();
	const USkeletalMesh* Mesh = Editor.GetMesh();
	if (!Asset || !Mesh || Bodies.IsEmpty())
	{
		ImGui::TextDisabled("Add physics bodies in the skeleton tree to build a constraint graph.");
		return;
	}
	ImGui::TextDisabled("Bodies: %d  Constraints: %d", Bodies.Num(), Asset->Constraints.Num());
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (Size.x < 1.0f || Size.y < 1.0f) return;

	// 노드 배치에는 본 계층을 사용하고 연결선은 에셋에 저장된 제약에서만 만듭니다.
	const float Scale = EditorStyle::Scale(1.0f);
	const ImVec2 NodeSize(200.0f * Scale, 52.0f * Scale);
	TArray<ImVec2> Positions;
	Positions.SetNum(Bodies.Num());
	TArray<int32> Rows;
	Rows.Init(0, Bodies.Num() + 1);
	ImVec2 Bounds(NodeSize.x, NodeSize.y);
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		int32 Depth = 0;
		for (int32 Parent = Editor.GetNearestParentBody(Index); Parent >= 0 && Depth < Bodies.Num();
			Parent = Editor.GetNearestParentBody(Parent)) ++Depth;
		Positions[Index] = ImVec2(Depth * 280.0f * Scale, Rows[Depth]++ * 86.0f * Scale);
		Bounds.x = FMath::Max(Bounds.x, Positions[Index].x + NodeSize.x);
		Bounds.y = FMath::Max(Bounds.y, Positions[Index].y + NodeSize.y);
	}
	if (LastTopologyRevision != Editor.GetTopologyRevision())
	{
		LastTopologyRevision = Editor.GetTopologyRevision();
		bFitRequested = true;
	}
	if (bFitRequested)
	{
		Zoom = FMath::Clamp(FMath::Min(Size.x * 0.9f / Bounds.x, Size.y * 0.9f / Bounds.y), 0.2f, 1.5f);
		Pan = ImVec2((Size.x - Bounds.x * Zoom) * 0.5f, (Size.y - Bounds.y * Zoom) * 0.5f);
		bFitRequested = false;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 End(Origin.x + Size.x, Origin.y + Size.y);
	ImGui::InvisibleButton("##ConstraintGraphCanvas", Size, ImGuiButtonFlags_MouseButtonLeft |
		ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
	const bool bHovered = ImGui::IsItemHovered();
	const ImGuiIO& IO = ImGui::GetIO();
	if (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) ImGui::SetWindowFocus();
	if (!IO.AppFocusLost && ImGui::IsItemActive() &&
		(ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right)))
	{
		Pan.x += IO.MouseDelta.x;
		Pan.y += IO.MouseDelta.y;
	}
	if (bHovered && IO.MouseWheel != 0.0f && !IO.AppFocusLost)
	{
		const float NewZoom = FMath::Clamp(Zoom * std::pow(1.15f, IO.MouseWheel), 0.2f, 2.5f);
		const float Ratio = NewZoom / Zoom;
		Pan.x = IO.MousePos.x - Origin.x - (IO.MousePos.x - Origin.x - Pan.x) * Ratio;
		Pan.y = IO.MousePos.y - Origin.y - (IO.MousePos.y - Origin.y - Pan.y) * Ratio;
		Zoom = NewZoom;
	}
	const auto ScreenPosition = [&](int32 Index)
	{
		return ImVec2(Origin.x + Pan.x + Positions[Index].x * Zoom, Origin.y + Pan.y + Positions[Index].y * Zoom);
	};

	ImDrawList* Draw = ImGui::GetWindowDrawList();
	Draw->PushClipRect(Origin, End, true);
	Draw->AddRectFilled(Origin, End, EditorColor::BgDeepest);
	int32 HoveredConstraint = -1;
	float NearestDistance = 8.0f * Scale;
	for (int32 Index = 0; Index < Asset->Constraints.Num(); ++Index)
	{
		const FPhysicsAssetConstraint& Constraint = Asset->Constraints[Index];
		const int32 Parent = Editor.FindBodyByBone(Mesh->FindBoneIndex(Constraint.ParentBone));
		const int32 Child = Editor.FindBodyByBone(Mesh->FindBoneIndex(Constraint.ChildBone));
		if (Parent < 0 || Child < 0) continue;
		const ImVec2 A = ScreenPosition(Parent);
		const ImVec2 B = ScreenPosition(Child);
		const ImVec2 Start(A.x + NodeSize.x * Zoom, A.y + NodeSize.y * Zoom * 0.5f);
		const ImVec2 Finish(B.x, B.y + NodeSize.y * Zoom * 0.5f);
		const bool bSelected = Index == Editor.GetSelectedConstraintIndex();
		Draw->AddLine(Start, Finish, bSelected ? EditorColor::AccentBright : EditorColor::CategoryBlue,
			(bSelected ? 3.0f : 2.0f) * Scale);
		Draw->AddCircleFilled(Finish, 4.0f * Scale, EditorColor::CategoryBlue);
		if (bHovered)
		{
			const ImVec2 Point = ImLineClosestPoint(Start, Finish, IO.MousePos);
			const float DX = Point.x - IO.MousePos.x;
			const float DY = Point.y - IO.MousePos.y;
			const float Distance = std::sqrt(DX * DX + DY * DY);
			if (Distance < NearestDistance) { NearestDistance = Distance; HoveredConstraint = Index; }
		}
	}

	int32 HoveredBody = -1;
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		const ImVec2 Min = ScreenPosition(Index);
		const ImVec2 Max(Min.x + NodeSize.x * Zoom, Min.y + NodeSize.y * Zoom);
		const bool bSelected = Index == Editor.GetSelectedBodyIndex();
		Draw->AddRectFilled(Min, Max, bSelected ? EditorColor::AccentDim : EditorColor::BgElevated, 5.0f * Scale);
		Draw->AddRect(Min, Max, bSelected ? EditorColor::AccentBright : EditorColor::BorderStrong, 5.0f * Scale);
		const ImVec4 Clip(Min.x + 4.0f, Min.y, Max.x - 4.0f, Max.y);
		const ImVec2 TextPosition(Min.x + 10.0f * Scale * Zoom, Min.y + 14.0f * Scale * Zoom);
		Draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * Zoom, TextPosition, EditorColor::TextPrimary,
			Bodies[Index].BodySetup->BoneName.c_str(), nullptr, 0.0f, &Clip);
		if (bHovered && ImRect(Min, Max).Contains(IO.MousePos)) HoveredBody = Index;
	}
	Draw->PopClipRect();
	if (bHovered && !IO.AppFocusLost && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (HoveredBody >= 0) Editor.SelectBody(HoveredBody);
		else if (HoveredConstraint >= 0) Editor.SelectConstraint(HoveredConstraint);
		else Editor.SelectBone(-1);
	}
	if (bHovered && HoveredBody < 0 && HoveredConstraint >= 0)
	{
		const FPhysicsAssetConstraint& Constraint = Asset->Constraints[HoveredConstraint];
		ImGui::SetTooltip("%s -> %s\nClick to edit this constraint.", Constraint.ParentBone.c_str(), Constraint.ChildBone.c_str());
	}
}
