#include "Viewport/PreviewViewportClient.h"

#include "Component/SceneComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "EditorEngine.h"
#include "GameFramework/AActor.h"
#include "Input/SkelViewerViewportInputController.h"
#include "Preview/PreviewSceneContext.h"
#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Renderer.h"
#include "Render/Scene/Scene.h"
#include "Settings/EditorSettings.h"
#include "Mesh/Skeleton.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"
#include <Render/Scene/Debug/DebugRenderAPI.h>
#include <cmath>

namespace
{
float SafeDiv(float Numerator, float Denominator)
{
    return std::abs(Denominator) > FMath::Epsilon ? Numerator / Denominator : Numerator;
}

float DeltaDegrees(float From, float To)
{
    float Delta = std::fmod(To - From + 180.0f, 360.0f);
    if (Delta < 0.0f)
    {
        Delta += 360.0f;
    }
    return Delta - 180.0f;
}

FQuat ExtractRotationNoScale(const FMatrix& Matrix)
{
    FMatrix RotationMatrix = Matrix;

    for (int32 Row = 0; Row < 3; ++Row)
    {
        FVector Axis(
            RotationMatrix.M[Row][0],
            RotationMatrix.M[Row][1],
            RotationMatrix.M[Row][2]);

        if (Axis.LengthSquared() > FMath::Epsilon * FMath::Epsilon)
        {
            Axis.Normalize();
            RotationMatrix.M[Row][0] = Axis.X;
            RotationMatrix.M[Row][1] = Axis.Y;
            RotationMatrix.M[Row][2] = Axis.Z;
        }
    }

    RotationMatrix.M[0][3] = 0.0f;
    RotationMatrix.M[1][3] = 0.0f;
    RotationMatrix.M[2][3] = 0.0f;
    RotationMatrix.M[3][0] = 0.0f;
    RotationMatrix.M[3][1] = 0.0f;
    RotationMatrix.M[3][2] = 0.0f;
    RotationMatrix.M[3][3] = 1.0f;

    return RotationMatrix.ToQuat();
}

FMatrix GetBoneComponentToDebugWorldMatrix(USkeletalMeshComponent* MeshComp, int32 BoneIndex)
{
    if (!MeshComp || BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
    {
        return FMatrix::Identity;
    }

    const FMatrix& BoneComponentMatrix = MeshComp->GetBoneComponentMatrix(BoneIndex);
    const FMatrix BoneDebugWorldMatrix = MeshComp->GetBoneDebugWorldMatrix(BoneIndex);
    return BoneComponentMatrix.GetInverse() * BoneDebugWorldMatrix;
}

FVector DebugWorldToBoneComponentPosition(
    USkeletalMeshComponent* MeshComp,
    int32 BoneIndex,
    const FVector& DebugWorldPosition)
{
    const FMatrix ComponentToDebugWorld = GetBoneComponentToDebugWorldMatrix(MeshComp, BoneIndex);
    return ComponentToDebugWorld.GetInverse().TransformPositionWithW(DebugWorldPosition);
}
}

FPreviewViewportClient::FPreviewViewportClient() = default;

void FPreviewViewportClient::Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice, FPreviewSceneContext* InPreviewContext, FSkeletalMeshViewer* InOwner)
{
    Viewport = new FViewport();
    Viewport->Initialize(InDevice, static_cast<uint32>(ViewportWidth), static_cast<uint32>(ViewportHeight));
    Viewport->SetClient(this);

    InputController = std::make_unique<FSkelViewerViewportInputController>(InOwner, this);

    EditorEngine = InEditorEngine;
    PreviewContext = InPreviewContext;
    OwnerViewer = InOwner;
    ResetCamera();
}

void FPreviewViewportClient::Release()
{
    if (Viewport)
    {
        Viewport->Release();
        delete Viewport;
        Viewport = nullptr;
    }

    PreviewContext = nullptr;
    EditorEngine = nullptr;
    OwnerViewer = nullptr;
}

void FPreviewViewportClient::SyncBoneGizmoToSelection()
{
    if (!PreviewContext)
    {
        return;
    }

    UGizmoComponent* Gizmo = OwnerViewer ? OwnerViewer->GetBoneGizmo() : nullptr;
    AActor* TargetActor = OwnerViewer ? OwnerViewer->GetBoneGizmoTargetActor() : nullptr;
    USkeletalMeshComponent* MeshComp = OwnerViewer ? OwnerViewer->GetPreviewMeshComponent() : nullptr;
    if (!Gizmo || !TargetActor || !MeshComp)
    {
        return;
    }

    if (Gizmo->IsHolding())
    {
        ApplyBoneGizmoToSelection();
        return;
    }

    const int32 BoneIndex = Options.SelectedBoneIndex;
    if (BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
    {
        Gizmo->SetVisibility(false);
        bHasLastBoneGizmoComponentRotation = false;
        return;
    }

    if (!Gizmo->HasTarget())
    {
        Gizmo->SetTarget(TargetActor);
    }

    const FMatrix BoneWorldMatrix = MeshComp->GetBoneDebugWorldMatrix(BoneIndex);
    if (USceneComponent* Root = TargetActor->GetRootComponent())
    {
        Root->SetRelativeLocation(BoneWorldMatrix.GetLocation());
        Root->SetRelativeRotationWithEulerHint(FQuat::Identity, FRotator());
        Root->SetRelativeScale(OwnerViewer
                                   ? OwnerViewer->GetCachedBoneComponentScale(BoneIndex)
                                   : BoneWorldMatrix.GetScale());

        LastBoneGizmoComponentRotation = FQuat();
        bHasLastBoneGizmoComponentRotation = true;
    }

    Gizmo->SetVisibility(true);
    Gizmo->UpdateGizmoTransform();
}

void FPreviewViewportClient::ApplyBoneGizmoToSelection()
{
    if (!PreviewContext || !OwnerViewer)
    {
        return;
    }

    AActor* TargetActor = OwnerViewer->GetBoneGizmoTargetActor();
    USkeletalMeshComponent* MeshComp = OwnerViewer->GetPreviewMeshComponent();
    if (!TargetActor || !MeshComp)
    {
        return;
    }

    USceneComponent* TargetRoot = TargetActor->GetRootComponent();
    if (!TargetRoot)
    {
        return;
    }

    const int32 BoneIndex = Options.SelectedBoneIndex;
    if (BoneIndex < 0 || BoneIndex >= MeshComp->GetNumBones())
    {
        return;
    }

    FTransform CachedTransform;
    if (!OwnerViewer->GetCachedBoneLocalTransform(BoneIndex, CachedTransform))
    {
        return;
    }

    const FBoneInfo* BoneInfo = MeshComp->GetBoneInfo(BoneIndex);
    const int32 ParentIndex = BoneInfo ? BoneInfo->ParentIndex : -1;

    UGizmoComponent* Gizmo = OwnerViewer->GetBoneGizmo();
    const EGizmoMode GizmoMode = Gizmo ? Gizmo->GetMode() : EGizmoMode::Translate;

    if (GizmoMode == EGizmoMode::Translate)
    {
        // 기즈모가 실제로 놓인 DebugWorld 위치
        const FVector DebugWorldPosition = TargetRoot->GetWorldLocation();

        // DebugWorld -> SkeletalMesh Component Space
        const FVector BoneComponentPosition =
            DebugWorldToBoneComponentPosition(
                MeshComp,
                BoneIndex,
                DebugWorldPosition);

        if (ParentIndex >= 0 && ParentIndex < MeshComp->GetNumBones())
        {
            // Component Space -> Parent Bone Local Space
            const FMatrix ParentComponentInverse =
                MeshComp->GetBoneComponentMatrix(ParentIndex).GetInverse();

            CachedTransform.Location =
                ParentComponentInverse.TransformPositionWithW(BoneComponentPosition);
        }
        else
        {
            // root bone이면 component space 위치가 곧 local location
            CachedTransform.Location = BoneComponentPosition;
        }
    }
    else if (GizmoMode == EGizmoMode::Rotate)
    {
        if (!Gizmo || Gizmo->GetSelectedAxis() < 0)
        {
            return;
        }

        FQuat NewComponentRotation = TargetRoot->GetRelativeRotation().ToQuaternion();
        NewComponentRotation.Normalize();

        if (!bHasLastBoneGizmoComponentRotation)
        {
            LastBoneGizmoComponentRotation = NewComponentRotation;
            bHasLastBoneGizmoComponentRotation = true;
            return;
        }

        // component space에서 이번 프레임 동안 기즈모가 회전한 양
        FQuat DeltaComponentRotation =
            NewComponentRotation * LastBoneGizmoComponentRotation.Inverse();
        DeltaComponentRotation.Normalize();

        // 부모 본의 component rotation
        FQuat ParentComponentRotation = FQuat::Identity;
        if (ParentIndex >= 0 && ParentIndex < MeshComp->GetNumBones())
        {
            ParentComponentRotation =
                ExtractRotationNoScale(MeshComp->GetBoneComponentMatrix(ParentIndex));
            ParentComponentRotation.Normalize();
        }

        // component-space delta를 parent-local-space delta로 변환
        FQuat DeltaLocalRotation =
            ParentComponentRotation.Inverse() * DeltaComponentRotation * ParentComponentRotation;
        DeltaLocalRotation.Normalize();

        // bone local rotation에 적용
        CachedTransform.Rotation = DeltaLocalRotation * CachedTransform.Rotation;
        CachedTransform.Rotation.Normalize();

        LastBoneGizmoComponentRotation = NewComponentRotation;
    }
    else if (GizmoMode == EGizmoMode::Scale)
    {
        CachedTransform.Scale = TargetRoot->GetRelativeScale();

        if (ParentIndex >= 0 && ParentIndex < MeshComp->GetNumBones())
        {
            const FVector ParentScale = OwnerViewer->GetCachedBoneComponentScale(ParentIndex);
            const FVector ComponentScale = TargetRoot->GetRelativeScale();
            CachedTransform.Scale = FVector(
                SafeDiv(ComponentScale.X, ParentScale.X),
                SafeDiv(ComponentScale.Y, ParentScale.Y),
                SafeDiv(ComponentScale.Z, ParentScale.Z));
        }
    }

    OwnerViewer->SetCachedBoneLocalTransform(BoneIndex, CachedTransform, true);
}

void FPreviewViewportClient::Tick(float DeltaTime)
{
    if (InputController)
    {
        InputController->HandleInput(DeltaTime);
    }
}

void FPreviewViewportClient::ResetCamera()
{
    OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
    OrbitDistance = 8.0f;
    OrbitYaw = -135.0f;
    OrbitPitch = 23.0f;

    ApplyOrbitToCamera();
}

void FPreviewViewportClient::ApplyOrbitToCamera()
{
    if (!PreviewContext)
    {
        return;
    }

    UCameraComponent* Camera = PreviewContext->GetCamera();
    if (!Camera)
    {
        return;
    }

    const FRotator ViewRotation(OrbitPitch, OrbitYaw, 0.0f);
    const FVector Forward = ViewRotation.GetForwardVector();
    const FVector ViewLocation = OrbitPivot - Forward * OrbitDistance;

    Camera->SetWorldLocation(ViewLocation);
    Camera->LookAt(OrbitPivot);
}

void FPreviewViewportClient::SetViewportRect(float X, float Y, float Width, float Height)
{
    ViewportX = X;
    ViewportY = Y;
    ViewportWidth = Width;
    ViewportHeight = Height;

    if (!Viewport)
    {
        return;
    }

    const uint32 W = static_cast<uint32>(Width > 1.0f ? Width : 1.0f);
    const uint32 H = static_cast<uint32>(Height > 1.0f ? Height : 1.0f);

    if (W != Viewport->GetWidth() || H != Viewport->GetHeight())
    {
        Viewport->RequestResize(W, H);
    }
}

bool FPreviewViewportClient::GetViewportRect(FRect& OutRect) const
{
    if (!Viewport || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return false;
    }

    OutRect.X = ViewportX;
    OutRect.Y = ViewportY;
    OutRect.Width = ViewportWidth;
    OutRect.Height = ViewportHeight;
    return true;
}

UCameraComponent* FPreviewViewportClient::GetCamera() const
{
    return PreviewContext ? PreviewContext->GetCamera() : nullptr;
}

UGizmoComponent* FPreviewViewportClient::GetGizmo()
{
    return OwnerViewer ? OwnerViewer->GetBoneGizmo() : nullptr;
}

UWorld* FPreviewViewportClient::GetWorld() const
{
    return PreviewContext ? PreviewContext->GetWorld() : nullptr;
}

void FPreviewViewportClient::RenderViewportImage()
{
    if (!Viewport || !Viewport->GetSRV())
    {
        return;
    }

    if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return;
    }

    ImVec2 Min(ViewportX, ViewportY);
    ImVec2 Max(ViewportX + ViewportWidth, ViewportY + ViewportHeight);

    ImGui::GetWindowDrawList()->AddImage(
        (ImTextureID)Viewport->GetSRV(),
        Min,
        Max);

    ImGui::SetCursorScreenPos(Min);
    ImGui::InvisibleButton(
        "##PreviewViewportInput",
        ImVec2(ViewportWidth, ViewportHeight),
        ImGuiButtonFlags_MouseButtonLeft |
        ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);
}

void FPreviewViewportClient::Draw(FViewport* Viewport, float DeltaTime)
{
    (void)DeltaTime;

    if (!EditorEngine || !PreviewContext || !Viewport)
    {
        return;
    }

    UWorld* World = PreviewContext->GetWorld();
    UCameraComponent* Camera = PreviewContext->GetCamera();
    if (!World || !Camera)
    {
        return;
    }

    FRenderer& Renderer = EditorEngine->GetRenderer();
    ID3D11DeviceContext* Context = Renderer.GetFD3DDevice().GetDeviceContext();
    if (!Context)
    {
        return;
    }

    if (Viewport->ApplyPendingResize())
    {
        Camera->OnResize(static_cast<int32>(Viewport->GetWidth()), static_cast<int32>(Viewport->GetHeight()));
    }

    const float ClearColor[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
    Viewport->BeginRender(Context, ClearColor);

    FViewportRenderTargets RenderTargets = {};
    RenderTargets.Reset();
    RenderTargets.SetFromViewport(Viewport);

    FScene& Scene = World->GetScene();
    Scene.ClearFrameData();

    FShowFlags ShowFlags = {};
    ShowFlags.bGrid = true;
    ShowFlags.bGizmo = true;
    ShowFlags.bUUIDText = false;
    ShowFlags.bSceneBVH = false;
    ShowFlags.bSceneOctree = false;
    ShowFlags.bWorldBound = false;
    ShowFlags.bLightDebugLines = false;
    ShowFlags.bFog = false;
    ShowFlags.bFXAA = true;

    const EViewMode ViewMode = OwnerViewer->GetState().ViewMode;

    FSceneView PreviewSceneView = {};
    PreviewSceneView.SetCameraInfo(Camera);
    PreviewSceneView.SetRenderSettings(ViewMode, ShowFlags);
    PreviewSceneView.RenderPath = FEditorSettings::Get().RenderShadingPath;
    PreviewSceneView.SetViewportInfo(Viewport);
    PreviewSceneView.ViewportType = ELevelViewportType::Perspective;
    PreviewSceneView.LODContext = World->PrepareLODContext();

    Renderer.BeginCollect(PreviewSceneView, Scene.GetPrimitiveProxyCount());

    FRenderPipelineContext PipelineContext = Renderer.CreatePipelineContext(PreviewSceneView, &RenderTargets, &Scene);
    PipelineContext.ViewMode.ActiveViewMode = ViewMode;

    FRenderCollectContext CollectContext = {};
    CollectContext.SceneView = &PreviewSceneView;
    CollectContext.Scene = &Scene;
    CollectContext.Renderer = &Renderer;
    CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
    CollectContext.ActiveViewMode = ViewMode;
    CollectContext.CollectedPrimitives = const_cast<FCollectedPrimitives*>(&Renderer.GetCollectedPrimitives());

    SyncBoneGizmoToSelection();

    if (Options.bShowMesh)
    {
        Renderer.CollectWorld(World, CollectContext);
    }
    if (Options.bShowSkeleton)
    {
        //OwnerViewer->RenderBoneDebugLines();
        Renderer.CollectSkeletalDebug(Scene);
    }

	Renderer.CollectGrid(1.0f, 20, Scene);
    Renderer.CollectDebugRender(Scene);

    Renderer.BuildDrawCommands(PipelineContext);
    Renderer.RunRootPipeline(ERenderPipelineType::EditorRootPipeline, PipelineContext);

    ID3D11RenderTargetView* FrameRTV = Renderer.GetFD3DDevice().GetFrameBufferRTV();
    ID3D11DepthStencilView* FrameDSV = Renderer.GetFD3DDevice().GetDepthStencilView();
    Context->OMSetRenderTargets(1, &FrameRTV, FrameDSV);

    D3D11_VIEWPORT FrameViewport = Renderer.GetFD3DDevice().GetViewport();
    Context->RSSetViewports(1, &FrameViewport);
}

FPreviewViewportClient::~FPreviewViewportClient() = default;

bool FPreviewViewportClient::InputKey(const FViewportKeyEvent& Event)
{
    return InputController ? InputController->InputKey(Event) : false;
}

bool FPreviewViewportClient::InputAxis(const FViewportAxisEvent& Event)
{
    return InputController ? InputController->InputAxis(Event) : false;
}

bool FPreviewViewportClient::InputPointer(const FViewportPointerEvent& Event)
{
    return InputController ? InputController->InputPointer(Event) : false;
}

void FPreviewViewportClient::ResetInputState()
{
    if (InputController)
    {
        InputController->ResetInputState();
    }
}

void FPreviewViewportClient::ResetKeyboardInputState()
{
    if (InputController)
    {
        InputController->ResetKeyboardInputState();
    }
}

void FPreviewViewportClient::BeginInputFrame()
{
    if (InputController)
    {
        InputController->BeginInputFrame();
    }
}
