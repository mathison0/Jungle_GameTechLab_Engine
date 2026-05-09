#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"

#include "Core/CoreGlobals.h"
#include "Component/SkeletalMeshComponent.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <commdlg.h>
#include <filesystem>
#include <string>

void FEditorSkeletalMeshViewerPanel::Release()
{
    Owner = nullptr;
    PreviewMeshComponent = nullptr;
}

void FEditorSkeletalMeshViewerPanel::Render(float DeltaTime)
{
    if (!Owner) return;

    bool bOpen = Owner->IsOpen();
    const std::string WindowName = "SkeletalMesh Viewer##" + std::to_string(Owner->GetEditorId());
    if (!ImGui::Begin(WindowName.c_str(), &bOpen))
    {
        Owner->SetOpen(bOpen);
        ImGui::End();
        return;
    }
    Owner->SetOpen(bOpen);

    RenderToolbar();

    ImGui::Separator();

    const float RightPanelWidth = 320.0f;
    ImVec2 Available = ImGui::GetContentRegionAvail();

    ImGui::BeginChild("PreviewViewport", ImVec2(Available.x - RightPanelWidth, Available.y), true);
    RenderPreviewViewport(DeltaTime);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanel", ImVec2(RightPanelWidth, Available.y), true);
    RenderBoneHierarchyTree();
    ImGui::Separator();
    RenderSelectedBoneTransformInspector();
    ImGui::EndChild();

    ImGui::End();
}


void FEditorSkeletalMeshViewerPanel::Update()
{
	
}

void FEditorSkeletalMeshViewerPanel::RenderToolbar()
{
	FSkeletalMeshViewerState& State = Owner->GetState();

	ImGui::Checkbox("Show Mesh", &State.bShowMesh);
    ImGui::Checkbox("Show Bone Names", &State.bShowBoneNames);
    if (ImGui::Checkbox("Show Skeleton", &State.bShowSkeleton))
    {
        Owner->ApplyPreviewFlags();
    }
}

void FEditorSkeletalMeshViewerPanel::RenderPreviewViewport(float DeltaTime)
{
    if (!Owner)
    {
        return;
    }

    auto& VC = Owner->GetViewportClient();

    ImVec2 Pos = ImGui::GetCursorScreenPos();
    ImVec2 Size = ImGui::GetContentRegionAvail();

    VC.SetViewportRect(Pos.x, Pos.y, Size.x, Size.y);
    VC.Draw(VC.GetViewport(), DeltaTime);
    VC.RenderViewportImage();

    const bool bHovered = ImGui::IsItemHovered();
    const bool bActive = ImGui::IsItemActive();

	ImGuiIO& IO = ImGui::GetIO();
    FPreviewInput Input;
    Input.bHovered = bHovered;
    Input.bActive = bActive;
    Input.bLeftDown = IO.MouseDown[0];
    Input.bRightDown = IO.MouseDown[1];
    Input.bMiddleDown = IO.MouseDown[2];
    Input.bAltDown = IO.KeyAlt;
    Input.bCtrlDown = IO.KeyCtrl;
    Input.bShiftDown = IO.KeyShift;
    Input.MousePos = FVector2(IO.MousePos.x - Pos.x, IO.MousePos.y - Pos.y);
    Input.MouseDelta = FVector2(IO.MouseDelta.x, IO.MouseDelta.y);
    Input.MouseWheel = bHovered ? IO.MouseWheel : 0.0f;

    VC.SetInputState(Input);
}

// TODO: 이름 수정이 가능하게 변경
//일단 구현됨
void FEditorSkeletalMeshViewerPanel::RenderBoneHierarchyTree()
{
    ImGui::Text("Bone Hierarchy");

    //if (BoneInfos.empty() || !PreviewMeshComponent)
    //{
    //    ImGui::TextDisabled("No skeleton.");
    //    ImGui::End();
    //    return;
    //}

    //ImGui::Text("Bones: %d", static_cast<int>(BoneInfos.size()));
    //ImGui::Separator();

    //RenderBoneNode(RootBoneIndex);
}

// 일단 구현됨
void FEditorSkeletalMeshViewerPanel::RenderBoneNode(uint32 BoneIndex)
{
    //if (!BoneInfos.empty())
    //    return;

    //const FBoneInfo& Bone = BoneInfos[BoneIndex];
    //const bool bSelected = (SelectedBoneIndex == BoneIndex);
    //const bool bLeaf = BonesHierarchy[BoneIndex].empty();

    //ImGuiTreeNodeFlags Flags =
    //    ImGuiTreeNodeFlags_OpenOnArrow |
    //    ImGuiTreeNodeFlags_OpenOnDoubleClick |
    //    ImGuiTreeNodeFlags_SpanAvailWidth;

    //if (bSelected)
    //    Flags |= ImGuiTreeNodeFlags_Selected;

    //if (bLeaf)
    //    Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    //ImGui::PushID(BoneIndex);

    //const bool bOpen = ImGui::TreeNodeEx(
    //    Bone.Name.ToString().c_str(),
    //    Flags);

    //if (ImGui::IsItemClicked())
    //{
    //    SelectedBoneIndex = BoneIndex;
    //}

    //if (!bLeaf && bOpen)
    //{
    //    for (int32 ChildIndex : BonesHierarchy[BoneIndex])
    //    {
    //        RenderBoneNode(ChildIndex);
    //    }

    //    ImGui::TreePop();
    //}

    //ImGui::PopID();
}

void FEditorSkeletalMeshViewerPanel::RenderSelectedBoneTransformInspector()
{
    //PreviewSkeletalMesh.Transform
    //SkeletalMesh.Transform -> ReadOnly
}

void FEditorSkeletalMeshViewerPanel::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMesh = InSkeletalMesh;

    if (PreviewMeshComponent)
    {
        PreviewMeshComponent->SetSkeletalMesh(InSkeletalMesh);
    }

    SelectedBoneIndex = 0;

	BuildBoneHierarchy();
}

void FEditorSkeletalMeshViewerPanel::BuildBoneHierarchy()
{
 //   BonesHierarchy.clear();
 //   BoneInfos.clear();

	//BoneInfos = SkeletalMesh->GetAllBoneInfos();
 //   const int32 BoneCount = static_cast<int32>(BoneInfos.size());
 //   BonesHierarchy.resize(BoneCount);

 //   for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
 //   {
 //       const int32 ParentIndex = BoneInfos[BoneIndex].ParentIndex;

 //       if (ParentIndex < 0)
 //       {
 //           BonesHierarchy[RootBoneIndex].push_back(BoneIndex);
 //           continue;
 //       }

 //       if (ParentIndex >= BoneCount)
 //       {
 //           // 잘못된 skeleton 데이터
 //           BonesHierarchy[RootBoneIndex].push_back(BoneIndex);
 //           continue;
 //       }

 //       BonesHierarchy[ParentIndex].push_back(BoneIndex);
 //   }
}