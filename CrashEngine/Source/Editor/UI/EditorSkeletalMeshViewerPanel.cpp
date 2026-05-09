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
#include "Render/Scene/Debug/DebugRenderAPI.h"

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

    FSkeletalMeshViewerState& State = Owner->GetState();
    FPreviewViewportOptions Options;
    Options.bShowMesh = State.bShowMesh;
    Options.bShowSkeleton = State.bShowSkeleton;
    Options.bShowBoneNames = State.bShowBoneNames;
    Options.SelectedBoneIndex = State.SelectedBoneIndex;

	if (State.bShowSkeleton)
    {
        RenderBoneDebugLine(RootBoneIndex, false);
	}

    VC.SetPreviewOptions(Options);
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

    if (BoneInfos.empty())
    {
        ImGui::TextDisabled("No skeleton.");
        return;
    }

    ImGui::Text("Bones: %d", static_cast<int>(BoneInfos.size()));
    ImGui::Separator();

    RenderBoneNode(RootBoneIndex);
}

// 일단 구현됨
void FEditorSkeletalMeshViewerPanel::RenderBoneNode(uint32 BoneIndex)
{
    if (BoneInfos.empty() || BoneIndex >= BoneInfos.size())
        return;
    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;

    const FViewerBoneInfo& Bone = BoneInfos[BoneIndex];
    const bool bSelected = (SelectedBoneIndex == BoneIndex);
    const bool bLeaf = BonesHierarchy[BoneIndex].empty();

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (bSelected)
        Flags |= ImGuiTreeNodeFlags_Selected;

    if (bLeaf)
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::PushID(BoneIndex);

    const bool bOpen = ImGui::TreeNodeEx(
        Bone.BoneName.ToString().c_str(),
        Flags);

    if (ImGui::IsItemClicked())
    {
        SelectedBoneIndex = static_cast<int32>(BoneIndex);
        Owner->GetState().SelectedBoneIndex = SelectedBoneIndex;
    }

    if (!bLeaf && bOpen)
    {
        for (int32 ChildIndex : BonesHierarchy[BoneIndex])
        {
            RenderBoneNode(ChildIndex);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void FEditorSkeletalMeshViewerPanel::RenderSelectedBoneTransformInspector()
{
    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;
    if (SelectedBoneIndex < 0 || SelectedBoneIndex >= static_cast<int32>(BoneInfos.size()))
    {
        ImGui::TextDisabled("No selected Bone");
        return;
    }

    const FViewerBoneInfo& Bone = BoneInfos[SelectedBoneIndex];

    ImGui::Text("Index: %d", SelectedBoneIndex);
    ImGui::Text("Name: %s", Bone.BoneName.ToString().c_str());
    ImGui::Text("Parent: %d", Bone.ParentIndex);

    if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(BoneInfos.size()))
    {
        ImGui::Text("Parent Name: %s", BoneInfos[Bone.ParentIndex].BoneName.ToString().c_str());
    }

    ImGui::SeparatorText("Local Transform");

    FTransform& LocalTransform = CurrentLocalPose[SelectedBoneIndex];
    FTransform& GlobalTransform = CurrentGlobalPose[SelectedBoneIndex];

    float Location[3] = {
        LocalTransform.Location.X,
        LocalTransform.Location.Y,
        LocalTransform.Location.Z
    };

    const FRotator LocalRotation = LocalTransform.GetRotator();
    float Rotation[3] = {
        LocalRotation.Pitch,
        LocalRotation.Yaw,
        LocalRotation.Roll
    };

    float Scale[3] = {
        LocalTransform.Scale.X,
        LocalTransform.Scale.Y,
        LocalTransform.Scale.Z
    };

    bool bChanged = false;

    bChanged |= ImGui::DragFloat3("Local Location", Location, 0.1f);
    bChanged |= ImGui::DragFloat3("Local Rotation", Rotation, 0.1f);
    bChanged |= ImGui::DragFloat3("Local Scale", Scale, 0.01f);

    if (bChanged)
    {
        LocalTransform.Location = FVector(Location[0], Location[1], Location[2]);
        LocalTransform.Rotation = FRotator(Rotation[0], Rotation[1], Rotation[2]).ToQuaternion();
        LocalTransform.Scale = FVector(Scale[0], Scale[1], Scale[2]);

        RecalculatePoseFromBone(SelectedBoneIndex);
        Owner->SetBoneLocalTransform(SelectedBoneIndex, LocalTransform);
    }
    ImGui::Spacing();
    ImGui::SeparatorText("Global Transform");

    const FVector& GlobalLocation = GlobalTransform.Location;
    const FRotator GlobalRotation = GlobalTransform.GetRotator();
    const FVector& GlobalScale = GlobalTransform.Scale;

    ImGui::Text("Global Location: %.3f, %.3f, %.3f",
                GlobalLocation.X, GlobalLocation.Y, GlobalLocation.Z);

    ImGui::Text("Global Rotation: %.3f, %.3f, %.3f",
                GlobalRotation.Pitch, GlobalRotation.Yaw, GlobalRotation.Roll);

    ImGui::Text("Global Scale: %.3f, %.3f, %.3f",
                GlobalScale.X, GlobalScale.Y, GlobalScale.Z);
}

//저장은 외부에서 진행하게 변경할것
void FEditorSkeletalMeshViewerPanel::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMesh = InSkeletalMesh;

    if (PreviewMeshComponent)
    {
        PreviewMeshComponent->SetSkeletalMesh(InSkeletalMesh);
    }
    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;

    SelectedBoneIndex = INDEX_NONE;
    Owner->GetState().SelectedBoneIndex = INDEX_NONE;

	BuildBoneHierarchy();
}

void FEditorSkeletalMeshViewerPanel::BuildBoneHierarchy()
{
    BoneInfos.clear();
    BonesHierarchy.clear();
    CurrentLocalPose.clear();
    CurrentGlobalPose.clear();

    auto AddBone = [this](const char* BoneName, int32 ParentIndex, const FVector& LocalLocation)
    {
        FViewerBoneInfo Info;
        Info.BoneName = FName(BoneName);
        Info.ParentIndex = ParentIndex;
        Info.ReferenceLocalTransform = FTransform(LocalLocation, FQuat::Identity, FVector(1.0f, 1.0f, 1.0f));
        Info.InverseBindPose = FMatrix::Identity;
        BoneInfos.push_back(Info);
        CurrentLocalPose.push_back(Info.ReferenceLocalTransform);
        CurrentGlobalPose.push_back(Info.ReferenceLocalTransform);
    };

    AddBone("Root", INDEX_NONE, FVector(0.0f, 0.0f, 0.0f));
    AddBone("Spine", 0, FVector(0.0f, 0.0f, 0.65f));
    AddBone("Head", 1, FVector(0.0f, 0.0f, 0.60f));
    AddBone("LeftArm", 1, FVector(0.0f, -0.45f, 0.35f));
    AddBone("RightArm", 1, FVector(0.0f, 0.45f, 0.35f));

    BonesHierarchy.resize(BoneInfos.size());

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(BoneInfos.size()); ++BoneIndex)
    {
        const int32 ParentIndex = BoneInfos[BoneIndex].ParentIndex;

        if (ParentIndex < 0)
        {
            continue;
        }

        if (ParentIndex < static_cast<int32>(BoneInfos.size()))
        {
            BonesHierarchy[ParentIndex].push_back(BoneIndex);
            CurrentGlobalPose[BoneIndex].Location =
                CurrentGlobalPose[ParentIndex].Location + CurrentLocalPose[BoneIndex].Location;
        }
    }
    RecalculatePoseFromBone(RootBoneIndex);

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

void FEditorSkeletalMeshViewerPanel::RenderBoneDebugLine(int32 BoneIndex, bool bInSelectedSubtree)
{
    if (BoneInfos.empty())
        return;

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneInfos.size()))
        return;

    if (BoneIndex >= static_cast<int32>(BonesHierarchy.size()))
        return;

    if (BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
        return;

    FScene* ScenePtr = Owner->GetPreviewScene().GetScene();
    if (!ScenePtr)
        return;

    FScene& Scene = *ScenePtr;

    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;
    const bool bThisIsSelected = (BoneIndex == SelectedBoneIndex);
    const bool bSelectedSubtree = bInSelectedSubtree || bThisIsSelected;

    for (int32 ChildIndex : BonesHierarchy[BoneIndex])
    {
        if (ChildIndex < 0 || ChildIndex >= static_cast<int32>(CurrentGlobalPose.size()))
            continue;

        const FVector ParentJoint = CurrentGlobalPose[BoneIndex].Location;
        const FVector ChildJoint = CurrentGlobalPose[ChildIndex].Location;

        FColor LineColor = BoneColor;
        if (BoneIndex == SelectedBoneIndex)
        {
            LineColor = SelectedColor;
        }
        else if (bSelectedSubtree)
        {
            // 선택된 bone 아래 모든 child/descendant line
            LineColor = SelectedChildColor; // White
        }
        else if (ChildIndex == SelectedBoneIndex)
        {
            // 선택된 bone으로 들어오는 부모 line
            LineColor = SelectedParentColor;
        }

        RenderDebugLine(Scene, ParentJoint, ChildJoint, LineColor, 0.0f);

        RenderBoneDebugLine(ChildIndex, bSelectedSubtree);
    }
}

void FEditorSkeletalMeshViewerPanel::RecalculatePoseFromBone(int32 BoneIndex)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
    {
        return;
    }

    RecalculatePoseRecursive(BoneIndex);
}

void FEditorSkeletalMeshViewerPanel::RecalculatePoseRecursive(int32 BoneIndex)
{
    if (BoneIndex < 0 ||
        BoneIndex >= static_cast<int32>(BoneInfos.size()) ||
        BoneIndex >= static_cast<int32>(CurrentLocalPose.size()) ||
        BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
    {
        return;
    }

    const FViewerBoneInfo& Bone = BoneInfos[BoneIndex];
    const FTransform& LocalTransform = CurrentLocalPose[BoneIndex];

    if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(CurrentGlobalPose.size()))
    {
        const FTransform& ParentGlobal = CurrentGlobalPose[Bone.ParentIndex];
        FTransform& GlobalTransform = CurrentGlobalPose[BoneIndex];

        GlobalTransform.Location = ParentGlobal.Location + LocalTransform.Location;
        GlobalTransform.Rotation = ParentGlobal.Rotation * LocalTransform.Rotation;
        GlobalTransform.Scale = FVector(
            ParentGlobal.Scale.X * LocalTransform.Scale.X,
            ParentGlobal.Scale.Y * LocalTransform.Scale.Y,
            ParentGlobal.Scale.Z * LocalTransform.Scale.Z);
    }
    else
    {
        CurrentGlobalPose[BoneIndex] = LocalTransform;
    }

    if (BoneIndex >= static_cast<int32>(BonesHierarchy.size()))
    {
        return;
    }

    for (int32 ChildIndex : BonesHierarchy[BoneIndex])
    {
        RecalculatePoseRecursive(ChildIndex);
    }
}
