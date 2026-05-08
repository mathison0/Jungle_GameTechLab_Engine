#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"

#include "Core/CoreGlobals.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <commdlg.h>
#include <filesystem>

void FEditorSkeletalMeshViewerPanel::Release()
{
    Owner = nullptr;
    PreviewMeshComponent = nullptr;
}

void FEditorSkeletalMeshViewerPanel::Render(float DeltaTime)
{
    if (!Owner) return;

    if (!ImGui::Begin("SkeletalMesh Viewer", &FEditorSettings::Get().UI.bSkeletalMeshViewer))
    {
        ImGui::End();
        return;
    }

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
    ImGui::Checkbox("Show Skeleton", &State.bShowSkeleton);
    ImGui::Checkbox("Show Bone Names", &State.bShowBoneNames);
    if (ImGui::Checkbox("Show Skeleton", &State.bShowSkeleton))
    {
        Owner->ApplyPreviewFlags();
    }
}

void FEditorSkeletalMeshViewerPanel::RenderPreviewViewport(float DeltaTime)
{
    //if (!Owner)
    //{
    //    return;
    //}

    //auto& VC = Owner->GetViewportClient();

    //ImVec2 Pos = ImGui::GetCursorScreenPos();
    //ImVec2 Size = ImGui::GetContentRegionAvail();

    //VC.SetViewportRect(Pos.x, Pos.y, Size.x, Size.y);
    //VC.Draw(VC.GetViewport(), DeltaTime);
    //VC.RenderViewportImage();
}

// TODO: 이름 수정이 가능하게 변경
//일단 구현됨
void FEditorSkeletalMeshViewerPanel::RenderBoneHierarchyTree()
{
        ImGui::Text("Bone Hierarchy");
    
        if (!PreviewMeshComponent)
        {
            ImGui::TextDisabled("No Skeletal Mesh");
            return;
        }
    
        FBorn* RootBone = nullptr;// = PreviewMeshComponent->GetRootBone();
    
        ImGui::Separator();
    
        if (!RootBone)
        {
            ImGui::TextDisabled("No Root Bone");
            return;
        }
    
        RenderBoneNode(RootBone);
}

// 일단 구현됨
void FEditorSkeletalMeshViewerPanel::RenderBoneNode(FBorn* RootBone)
{
    //    if (!Bone)
    //        return;
    //
    //    const TArray<FBorn*>& Children = Bone->GetChildren();
    //
    //    ImGuiTreeNodeFlags Flags =
    //        ImGuiTreeNodeFlags_OpenOnArrow |
    //        ImGuiTreeNodeFlags_SpanAvailWidth;
    //
    //    const bool bLeaf = Children.empty();
    //
    //    if (bLeaf)
    //    {
    //        Flags |= ImGuiTreeNodeFlags_Leaf;
    //        Flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;
    //    }
    //
    //    if (SelectedBone == Bone)
    //    {
    //        Flags |= ImGuiTreeNodeFlags_Selected;
    //    }
    //
    //    const char* BoneName = Bone->GetName();
    //
    //    bool bOpen = ImGui::TreeNodeEx(
    //        Bone,
    //        Flags,
    //        "%s",
    //        BoneName ? BoneName : "Unnamed Bone");
    //
    //    if (ImGui::IsItemClicked())
    //    {
    //        SelectedBone = Bone;
    //    }
    //
    //    if (ImGui::BeginPopupContextItem())
    //    {
    //        if (ImGui::MenuItem("Copy Name"))
    //        {
    //            ImGui::SetClipboardText(BoneName ? BoneName : "");
    //        }
    //
    //        ImGui::EndPopup();
    //    }
    //
    //    if (!bLeaf && bOpen)
    //    {
    //        for (FBorn* Child : Children)
    //        {
    //            RenderBoneNode(Child);
    //        }
    //
    //        ImGui::TreePop();
    //    }
}

void FEditorSkeletalMeshViewerPanel::RenderSelectedBoneTransformInspector()
{
    //PreviewSkeletalMesh.Transform
    //SkeletalMesh.Transform -> ReadOnly
}

void FEditorSkeletalMeshViewerPanel::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    //SkeletalMesh = InSkeletalMesh;

    //if (PreviewMeshComponent)
    //{
    //    PreviewMeshComponent->SetSkeletalMesh(InSkeletalMesh);
    //}

    //SelectedBoneIndex = INDEX_NONE;
}
