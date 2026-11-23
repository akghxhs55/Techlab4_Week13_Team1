#include "EnginePCH.h"
#include "PhysicsAssetEditorTab.h"
#include "PhysicsAssetEditor.h"
#include "Panels/PhysicsAssetViewportPanel.h"
#include "Panels/PhysicsAssetHierarchyPanel.h"
#include "Panels/PhysicsAssetDetailsPanel.h"
#include "Panels/PhysicsAssetSettingsPanel.h"

FPhysicsAssetEditorTab::FPhysicsAssetEditorTab(FPhysicsAssetEditor& InEditor)
	: FAssetEditorTab(InEditor)
	, PhysicsEditor(InEditor)
{
	const ImGuiID AssetId = ImHashStr(InEditor.GetAssetPath().c_str());
	DockSpaceId = ImHashStr("PhysicsAssetDockSpace", 0, AssetId);
	PanelClassId = ImHashStr("PhysicsAssetPanelClass", 0, AssetId);

	Panels.Add(MakeUnique<FPhysicsAssetViewportPanel>(InEditor));
	Panels.Add(MakeUnique<FPhysicsAssetHierarchyPanel>(InEditor));
	Panels.Add(MakeUnique<FPhysicsAssetDetailsPanel>(InEditor));
	Panels.Add(MakeUnique<FPhysicsAssetSettingsPanel>(InEditor));
	for (auto& Panel : Panels)
	{
		Panel->SetDockClass(PanelClassId);
		Panel->Init();
	}
}

void FPhysicsAssetEditorTab::DrawMenuBar()
{
	FAssetEditorTab::DrawMenuBar();
	EditorDock::DrawWindowMenu(Panels);
}

void FPhysicsAssetEditorTab::BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size)
{
	ImGui::DockBuilderAddNode(InDockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(InDockSpaceId, Size);

	ImGuiID ViewportId = InDockSpaceId;
	const ImGuiID HierarchyId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Left, 0.22f, nullptr, &ViewportId);
	ImGuiID DetailsId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Right, 0.28f, nullptr, &ViewportId);
	const ImGuiID SettingsId = ImGui::DockBuilderSplitNode(DetailsId, ImGuiDir_Down, 0.40f, nullptr, &DetailsId);
	ImGui::DockBuilderDockWindow(Panels[0]->GetWindowName(), ViewportId);
	ImGui::DockBuilderDockWindow(Panels[1]->GetWindowName(), HierarchyId);
	ImGui::DockBuilderDockWindow(Panels[2]->GetWindowName(), DetailsId);
	ImGui::DockBuilderDockWindow(Panels[3]->GetWindowName(), SettingsId);
	ImGui::DockBuilderFinish(InDockSpaceId);
}

void FPhysicsAssetEditorTab::OnRender(bool bActive)
{
	// 뷰포트 패널이 렌더링될 때 해당 패널이 크기와 입력을 다시 설정합니다.
	PhysicsEditor.SetViewportState(0, 0, {});
	if (PhysicsEditor.IsCloseRequested())
	{
		return;
	}

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		const ImVec2 Size = ImGui::GetContentRegionAvail();
		if (bActive && Size.x > 0.0f && Size.y > 0.0f &&
			(!bLayoutInitialized || !ImGui::DockBuilderGetNode(DockSpaceId)))
		{
			ImGui::DockBuilderRemoveNode(DockSpaceId);
			BuildDefaultLayout(DockSpaceId, Size);
			bLayoutInitialized = true;
		}

		const ImGuiWindowClass WindowClass = EditorDock::MakeDockSpaceClass(PanelClassId);
		const ImGuiDockNodeFlags Flags = bActive ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly;
		if (bActive || ImGui::DockBuilderGetNode(DockSpaceId))
		{
			ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f), Flags, &WindowClass);
		}
	}

	for (auto& Panel : Panels)
	{
		// 분리된 패널은 부모 탭이 가려져도 표시합니다.
		if (Panel->IsOpen() && (bActive || Panel->IsDetached(DockSpaceId)))
		{
			Panel->OnRender();
		}
	}
}
