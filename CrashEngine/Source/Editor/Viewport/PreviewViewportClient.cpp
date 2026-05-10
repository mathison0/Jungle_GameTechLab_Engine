#include "Viewport/PreviewViewportClient.h"
#include "Preview/PreviewSceneContext.h"
#include "Viewport/Viewport.h"
#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"
#include "Render/Execute/Context/RenderCollectContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Renderer.h"
#include "Render/Scene/Scene.h"
#include <Render/Scene/Debug/DebugRenderAPI.h>
#include <Component/Collision/ShapeComponent.h>


void FPreviewViewportClient::Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice, FPreviewSceneContext* InPreviewContext)
{
    Viewport = new FViewport();
    Viewport->Initialize(InDevice, static_cast<uint32>(ViewportWidth), static_cast<uint32>(ViewportHeight));
    Viewport->SetClient(this);

    EditorEngine = InEditorEngine;
    PreviewContext = InPreviewContext;
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
}
void FPreviewViewportClient::Update()
{
	
}

//Todo: Input처리시 키보드 입력도 진행할것 
void FPreviewViewportClient::Tick(float DeltaTime)
{
    (void)DeltaTime;

    if (!PreviewContext || (!Input.bHovered && !Input.bActive))
    {
        return;
    }

    UCameraComponent* Camera = PreviewContext->GetCamera();
    if (!Camera)
    {
        return;
    }

    const float OrbitSpeed = 0.25f;
    const float PanSpeed = 0.5f;
    const float ZoomSpeed = 4.0f;

    if (Input.bHovered && Input.MouseWheel != 0.0f)
    {
        OrbitDistance -= Input.MouseWheel * ZoomSpeed;
        OrbitDistance = Clamp(OrbitDistance, 1.0f, 100.0f);
    }

    // Left drag: orbit
    if (Input.bActive && Input.bLeftDown)
    {
		OrbitYaw += Input.MouseDelta.X * OrbitSpeed;
		OrbitPitch += Input.MouseDelta.Y * OrbitSpeed;

		OrbitPitch = Clamp(OrbitPitch, -89.0f, 89.0f);
	}

	// Middle drag: pan
    if (Input.bActive && Input.bMiddleDown)
    {
        const FRotator ViewRotation(OrbitPitch, OrbitYaw, 0.0f);
        const FVector Right = ViewRotation.GetRightVector();
        const FVector Up = ViewRotation.GetUpVector();
        const float ScaledPanSpeed = PanSpeed * (OrbitDistance / 300.0f);

        OrbitPivot -= Right * Input.MouseDelta.X * ScaledPanSpeed;
        OrbitPivot += Up * Input.MouseDelta.Y * ScaledPanSpeed;
    }

    ApplyOrbitToCamera();
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

UCameraComponent* FPreviewViewportClient::GetCamera() const
{
    return PreviewContext ? PreviewContext->GetCamera() : nullptr;
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
        "##PreviewViewport",
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
    ShowFlags.bGizmo = false;
    ShowFlags.bUUIDText = false;
    ShowFlags.bSceneBVH = false;
    ShowFlags.bSceneOctree = false;
    ShowFlags.bWorldBound = false;
    ShowFlags.bLightDebugLines = false;
    ShowFlags.bFog = false;
    ShowFlags.bFXAA = true;

    const EViewMode ViewMode = EViewMode::Lit_Phong;

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

    if (Options.bShowMesh)
    {
        Renderer.CollectWorld(World, CollectContext);
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