#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"

#include "Core/CoreGlobals.h"
#include "Component/SkeletalMeshComponent.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"
#include "Mesh/Skeleton.h"

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
    RenderInspector();
    ImGui::EndChild();

    ImGui::End();
}


void FEditorSkeletalMeshViewerPanel::Update()
{
	
}

void FEditorSkeletalMeshViewerPanel::RenderToolbar()
{
	FSkeletalMeshViewerState& State = Owner->GetState();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

	const auto RenderToggleButton = [](const char* Label, bool& bValue) -> bool
	{
		const ImVec4 ActiveColor(0.18f, 0.42f, 0.78f, 1.0f);
		const ImVec4 ActiveHoveredColor(0.24f, 0.50f, 0.88f, 1.0f);
		const ImVec4 InactiveColor(0.18f, 0.18f, 0.20f, 1.0f);
		const ImVec4 InactiveHoveredColor(0.26f, 0.26f, 0.29f, 1.0f);

		ImGui::PushStyleColor(ImGuiCol_Button, bValue ? ActiveColor : InactiveColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bValue ? ActiveHoveredColor : InactiveHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ActiveColor);

		const bool bClicked = ImGui::Button(Label);
		if (bClicked)
		{
			bValue = !bValue;
		}

		ImGui::PopStyleColor(3);
		return bClicked;
	};

	ImGui::TextUnformatted("View");
	ImGui::SameLine();

	RenderToggleButton("Mesh", State.bShowMesh);
	ImGui::SameLine();

	if (RenderToggleButton("Skeleton", State.bShowSkeleton))
	{
		Owner->ApplyPreviewFlags();
	}
	ImGui::SameLine();

	RenderToggleButton("Bone Names", State.bShowBoneNames);
	ImGui::SameLine();

	if (RenderToggleButton("FBX Local Bones", State.bUseFbxLocalSkeleton))
	{
        if (USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent())
        {
            MeshComp->SetSkeletalDebugPoseMode(
                State.bUseFbxLocalSkeleton
                    ? ESkeletalDebugPoseMode::FbxLocalPose
                    : ESkeletalDebugPoseMode::SkinBindPose);
        }
	}

	const ImGuiStyle& Style = ImGui::GetStyle();
	const float SaveWidth = ImGui::CalcTextSize("Save").x + Style.FramePadding.x * 2.0f;
	const float ResetWidth = ImGui::CalcTextSize("Reset Pose").x + Style.FramePadding.x * 2.0f;
	const float ActionWidth = SaveWidth + ResetWidth + Style.ItemSpacing.x;
	const float ActionStartX = ImGui::GetWindowContentRegionMax().x - ActionWidth;

	if (ImGui::GetCursorPosX() < ActionStartX)
	{
		ImGui::SameLine(ActionStartX);
	}
	else
	{
		ImGui::SameLine();
	}

	//Save랑 Reset은 기능없음
    if(ImGui::Button("Save"))
    {
    }
	ImGui::SameLine();
	if(ImGui::Button("Reset Pose")){
        GetPreviewMeshComponent()->ResetToReferencePose();
	}

	ImGui::PopStyleVar(2);
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
        USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
        const int32 BoneCount = MeshComp ? MeshComp->GetNumBones() : 0;
        for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            const FBoneInfo* Bone = MeshComp->GetBoneInfo(BoneIndex);
            if (Bone && Bone->ParentIndex < 0)
            {
                RenderBoneDebugLine(BoneIndex, false);
            }
        }
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

    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    const int32 BoneCount = MeshComp ? MeshComp->GetNumBones() : 0;
    if (BoneCount <= 0)
    {
        ImGui::TextDisabled("No skeleton.");
        return;
    }

    ImGui::Text("Bones: %d", BoneCount);
    ImGui::Separator();

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo* Bone = MeshComp->GetBoneInfo(BoneIndex);
        if (Bone && Bone->ParentIndex < 0)
        {
            RenderBoneNode(BoneIndex);
        }
    }
}

void FEditorSkeletalMeshViewerPanel::RenderBoneNode(uint32 BoneIndex)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    if (!MeshComp || BoneIndex >= static_cast<uint32>(MeshComp->GetNumBones()))
        return;

    if (BoneIndex >= BonesHierarchy.size())
        return;

    const FBoneInfo* Bone = MeshComp->GetBoneInfo(static_cast<int32>(BoneIndex));
    if (!Bone)
        return;

    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;

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
        Bone->Name.ToString().c_str(),
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

void FEditorSkeletalMeshViewerPanel::RenderInspector()
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    const int32 BoneCount = MeshComp ? MeshComp->GetNumBones() : 0;
    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;

    if (!MeshComp || SelectedBoneIndex < 0 || SelectedBoneIndex >= BoneCount)
    {
        ImGui::TextDisabled("No selected Bone");
        return;
    }

    const FBoneInfo* Bone = MeshComp->GetBoneInfo(SelectedBoneIndex);
    if (!Bone)
    {
        ImGui::TextDisabled("No selected Bone");
        return;
    }

    ImGui::Text("Index: %d", SelectedBoneIndex);
    ImGui::Text("Name: %s", Bone->Name.ToString().c_str());
    ImGui::Text("Parent: %d", Bone->ParentIndex);

    if (Bone->ParentIndex >= 0 && Bone->ParentIndex < BoneCount)
    {
        const FBoneInfo* ParentBone = MeshComp->GetBoneInfo(Bone->ParentIndex);
        if (ParentBone)
        {
            ImGui::Text("Parent Name: %s", ParentBone->Name.ToString().c_str());
        }
    }

    ImGui::SeparatorText("Local Transform");

    const FMatrix& LocalMatrix = MeshComp->GetBoneLocalMatrix(SelectedBoneIndex);
    const FMatrix& GlobalMatrix = MeshComp->GetBoneComponentMatrix(SelectedBoneIndex);

    FTransform LocalTransform(
        LocalMatrix.GetLocation(),
        LocalMatrix.ToRotator(),
        LocalMatrix.GetScale());

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

    const bool bLocationChanged = ImGui::DragFloat3("Local Location", Location, 0.1f);
    const bool bRotationChanged = ImGui::DragFloat3("Local Rotation", Rotation, 0.1f);
    const bool bScaleChanged = ImGui::DragFloat3("Local Scale", Scale, 0.01f);

    if (bLocationChanged && !bRotationChanged && !bScaleChanged)
    {
        FMatrix EditedMatrix = LocalMatrix;
        EditedMatrix.M[3][0] = Location[0];
        EditedMatrix.M[3][1] = Location[1];
        EditedMatrix.M[3][2] = Location[2];
        MeshComp->SetBoneLocalMatrix(SelectedBoneIndex, EditedMatrix);
    }
    else if (bRotationChanged || bScaleChanged)
    {
        LocalTransform.Location = FVector(Location[0], Location[1], Location[2]);
        LocalTransform.Rotation = FRotator(Rotation[0], Rotation[1], Rotation[2]).ToQuaternion();
        LocalTransform.Scale = FVector(Scale[0], Scale[1], Scale[2]);

        MeshComp->SetBoneLocalMatrix(SelectedBoneIndex, LocalTransform.ToMatrix());
    }
    ImGui::Spacing();
    ImGui::SeparatorText("Global Transform");

    const FVector GlobalLocation = GlobalMatrix.GetLocation();
    const FVector GlobalRotation = GlobalMatrix.GetEuler();
    const FVector GlobalScale = GlobalMatrix.GetScale();

    ImGui::Text("Global Location: %.3f, %.3f, %.3f",
                GlobalLocation.X, GlobalLocation.Y, GlobalLocation.Z);

    ImGui::Text("Global Rotation: %.3f, %.3f, %.3f",
                GlobalRotation.X, GlobalRotation.Y, GlobalRotation.Z);

    ImGui::Text("Global Scale: %.3f, %.3f, %.3f",
                GlobalScale.X, GlobalScale.Y, GlobalScale.Z);
}

//저장은 외부에서 진행하게 변경할것
void FEditorSkeletalMeshViewerPanel::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMesh = InSkeletalMesh;

    if (Owner)
    {
        Owner->GetState().SelectedBoneIndex = -1;

        if (USkeletalMeshComponent* MeshComp =
                Owner->GetPreviewScene().GetPreviewMeshComponent())
        {
            MeshComp->SetSkeletalMesh(InSkeletalMesh);
            MeshComp->ResetToReferencePose();
        }
    }
    BuildBoneHierarchy();
}

void FEditorSkeletalMeshViewerPanel::BuildBoneHierarchy()
{
    BonesHierarchy.clear();

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        return;
    }

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeleton()->GetBones();

    BonesHierarchy.resize(Bones.size());

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

        if (ParentIndex < 0)
        {
            continue;
        }

        if (ParentIndex < static_cast<int32>(Bones.size()))
        {
            BonesHierarchy[ParentIndex].push_back(BoneIndex);
        }
    }
}

void FEditorSkeletalMeshViewerPanel::RenderBoneDebugLine(int32 BoneIndex, bool bInSelectedSubtree)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    if (!MeshComp || MeshComp->GetNumBones() <= 0)
        return;

    if (BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
        return;

    if (BoneIndex >= static_cast<int32>(BonesHierarchy.size()))
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
        if (ChildIndex < 0 || ChildIndex >= MeshComp->GetNumBones())
            continue;

        const FVector ParentJoint = MeshComp->GetBoneDebugWorldMatrix(BoneIndex).GetLocation();
        const FVector ChildJoint = MeshComp->GetBoneDebugWorldMatrix(ChildIndex).GetLocation();

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

USkeletalMeshComponent* FEditorSkeletalMeshViewerPanel::GetPreviewMeshComponent()
{
    return Owner ? Owner->GetPreviewScene().GetPreviewMeshComponent()
				 : nullptr;
}
