#include "EnginePCH.h"
#include "PhysicsAssetDetailsPanel.h"
#include "Editor/PhysicsAssetEditor/PhysicsAssetEditor.h"
#include "Editor/Details/PropertyEditor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Render/SkeletalMesh.h"

FPhysicsAssetDetailsPanel::FPhysicsAssetDetailsPanel(FPhysicsAssetEditor& InEditor)
	: FPhysicsAssetEditorPanel(InEditor, "Details")
{
}

void FPhysicsAssetDetailsPanel::DrawContent()
{
	const USkeletalMesh* Mesh = Editor.GetMesh();
	if (!Mesh)
	{
		ImGui::TextWrapped("No preview skeletal mesh is available.");
		return;
	}

	if (ImGui::CollapsingHeader("Preview Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Mesh Path");
		ImGui::TextWrapped("%s", Mesh->GetPath().c_str());
		ImGui::Text("Bones: %d", Mesh->GetNumBones());
	}

	const int32 BoneIndex = Editor.GetSelectedBoneIndex();
	const TArray<FBoneInfo>& Bones = Mesh->GetRefSkeleton();
	const bool bValidBone = BoneIndex >= 0 && BoneIndex < Bones.Num();
	if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (bValidBone)
		{
			const FBoneInfo& Bone = Bones[BoneIndex];
			ImGui::TextDisabled("Bone Name");
			ImGui::TextWrapped("%s", Bone.Name.c_str());
			ImGui::Text("Bone Index: %d", BoneIndex);
			ImGui::TextDisabled("Parent Bone");
			const bool bValidParent = Bone.ParentIndex >= 0 && Bone.ParentIndex < Bones.Num();
			ImGui::TextWrapped("%s", bValidParent ? Bones[Bone.ParentIndex].Name.c_str() : "None (Root)");
		}
		else
		{
			ImGui::TextWrapped("Select a bone in the viewport or skeleton tree to inspect it.");
		}
	}

	if (bValidBone && ImGui::CollapsingHeader("Bone Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (const USkeletalMeshComponent* Component = Editor.GetMeshComponent())
		{
			// 본 포즈와 충돌 형상 변환을 혼동하지 않도록 본의 변환은 읽기 전용으로 표시합니다.
			FTransform Transform = Component->GetBoneLocalTransform(BoneIndex);
			FRotator Rotation = Transform.Rotation.ToFRotator();
			ImGui::TextDisabled("Parent Space (Read Only)");
			ImGui::PushID(BoneIndex);
			ImGui::BeginDisabled();
			FPropertyEditor::DrawVector3("Location", Transform.Location.V, 0.0f);
			FPropertyEditor::DrawRotator("Rotation", Rotation);
			FPropertyEditor::DrawVector3("Scale", Transform.Scale.V, 1.0f);
			ImGui::EndDisabled();
			ImGui::PopID();
		}
		else
		{
			ImGui::TextWrapped("The preview mesh component is unavailable.");
		}
	}

	const FPhysicsAssetPreviewBody* Body = Editor.GetSelectedBody();
	if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (Body)
		{
			ImGui::Text("Body Index: %d", Editor.GetSelectedBodyIndex());
			ImGui::Text("Shapes: %d", Body->Geometry.GetElementCount());
			ImGui::TextWrapped("Changes are kept until this editor is closed.");
		}
		else
		{
			ImGui::TextWrapped("Select a body node or a collision shape. Right-click a bone in the skeleton tree to add a body.");
		}
	}
	if (ImGui::CollapsingHeader("Collision"))
	{
		if (Body)
		{
			ImGui::Text("Spheres: %d", Body->Geometry.SphereElems.Num());
			ImGui::Text("Boxes: %d", Body->Geometry.BoxElems.Num());
			ImGui::Text("Capsules: %d", Body->Geometry.CapsuleElems.Num());
		}
		else
		{
			ImGui::TextWrapped("Select a body to inspect its collision geometry.");
		}
	}
	if (ImGui::CollapsingHeader("Constraint"))
	{
		ImGui::TextWrapped("Constraint data is not connected. Select a constraint to edit its frames and linear/angular limits once constraint selection is available.");
	}
}
