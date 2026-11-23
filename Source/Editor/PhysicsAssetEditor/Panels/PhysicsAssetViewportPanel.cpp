#include "EnginePCH.h"
#include "PhysicsAssetViewportPanel.h"
#include "Editor/PhysicsAssetEditor/PhysicsAssetEditor.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Render/Texture2D.h"
#include "Editor/Viewports/ViewportToolbarWidgets.h"

FPhysicsAssetViewportPanel::FPhysicsAssetViewportPanel(FPhysicsAssetEditor& InEditor)
	: FPhysicsAssetEditorPanel(InEditor, "Viewport")
{
}

void FPhysicsAssetViewportPanel::DrawContent()
{
	DrawPreviewViewToolbar(Editor, ToolbarState);

	FEditorViewportClient* Client = Editor.GetViewportClient();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (!Client || Size.x < 1.0f || Size.y < 1.0f)
	{
		Editor.SetViewportState(0, 0, {});
		return;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();

	// 이미지 영역을 입력 항목으로 등록해 영역 밖에서도 시작한 드래그를 유지합니다.
	ImGui::InvisibleButton("##PhysicsAssetViewport", Size,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	if (FTexture2D* Target = Client->GetColorTarget(); Target && Target->GetSRV())
	{
		ImGui::GetWindowDrawList()->AddImage(Target->GetSRV(), Origin,
			ImVec2(Origin.x + Size.x, Origin.y + Size.y));
	}

	const bool bHovered = ImGui::IsItemHovered();
	if (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
	{
		ImGui::SetWindowFocus();
	}

	// 분리된 플랫폼 창에서도 입력이 동작하도록 ImGui 입력을 사용합니다.
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !IO.AppFocusLost;
	const bool bKeyboardInput = bFocused && !IO.WantTextInput;
	const bool bMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool bCameraDragging = ImGui::IsMouseDown(ImGuiMouseButton_Right);
	if (bKeyboardInput && bHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
	{
		Editor.FocusMesh();
	}
	if (bKeyboardInput && bHovered && !bCameraDragging && !bMouseDown && !Editor.IsGizmoDragging())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) Editor.SetGizmoMode(EGizmoMode::Location);
		else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) Editor.SetGizmoMode(EGizmoMode::Rotation);
		else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) Editor.SetGizmoMode(EGizmoMode::Scale);
		else if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
			Editor.SetGizmoMode(static_cast<EGizmoMode>((static_cast<int>(Editor.GetGizmoMode()) + 1) % 3));
	}

	const FViewportPointerInput PointerInput
	{
		.MouseUV = FVector2((IO.MousePos.x - Origin.x) / Size.x, (IO.MousePos.y - Origin.y) / Size.y),
		.bHovered = bHovered,
		.bFocused = bFocused,
		.bMousePressed = bFocused && bHovered && !bCameraDragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
		.bMouseDown = bFocused && bMouseDown,
		.bCancel = !bFocused || ImGui::IsKeyPressed(ImGuiKey_Escape, false),
	};

	const FViewportCameraInput CameraInput
	{
		.bCaptured = bFocused && ImGui::IsItemActive() && bCameraDragging && !bMouseDown && !Editor.IsGizmoDragging(),
		.bAllowWheel = bHovered && !IO.AppFocusLost && !bMouseDown && !Editor.IsGizmoDragging(),
		.bForward = bKeyboardInput && ImGui::IsKeyDown(ImGuiKey_W),
		.bBackward = bKeyboardInput && ImGui::IsKeyDown(ImGuiKey_S),
		.bRight = bKeyboardInput && ImGui::IsKeyDown(ImGuiKey_D),
		.bLeft = bKeyboardInput && ImGui::IsKeyDown(ImGuiKey_A),
		.bUp = bKeyboardInput && ImGui::IsKeyDown(ImGuiKey_E),
		.bDown = bKeyboardInput && ImGui::IsKeyDown(ImGuiKey_Q),
		.MouseX = IO.MouseDelta.x,
		.MouseY = IO.MouseDelta.y,
		// 공통 카메라 이동 함수가 사용하는 Windows 휠 단위로 변환합니다.
		.Wheel = IO.MouseWheel * 120.0f,
	};

	// 렌더 타깃 크기 변경과 렌더링은 에디터에서 처리합니다.
	Editor.SetViewportState(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y), CameraInput, PointerInput);

	const int ClickedMode = ViewportToolbarWidgets::TransformModeOverlay("##PhysicsAssetGizmoToolbar", Origin,
		static_cast<int>(Editor.GetGizmoMode()));
	if (ClickedMode >= 0) Editor.SetGizmoMode(static_cast<EGizmoMode>(ClickedMode));
}
