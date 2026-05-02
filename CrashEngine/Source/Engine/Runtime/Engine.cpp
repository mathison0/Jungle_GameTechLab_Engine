// 런타임 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Engine/Runtime/Engine.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "Profiling/Stats.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Resource/ResourceManager.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Mesh/ObjManager.h"
#include "Texture/Texture2D.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Core/TickFunction.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Serialization/SceneSaveManager.h"
#include "Materials/MaterialManager.h"
#include "SimpleJSON/json.hpp"
#include <fstream>
#include "Input/GameInput.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

namespace
{
ELevelTick ToLevelTickType(EWorldType WorldType)
{
    switch (WorldType)
    {
    case EWorldType::Editor:
        return ELevelTick::LEVELTICK_ViewportsOnly;
    case EWorldType::PIE:
    case EWorldType::Game:
        return ELevelTick::LEVELTICK_All;
    default:
        return ELevelTick::LEVELTICK_TimeOnly;
    }
}
} // namespace

void UEngine::Init(FWindowsWindow* InWindow)
{
    UE_LOG(Engine, Info, "Initializing runtime engine.");
    Window = InWindow;

    FNamePool::Get();
    FObjectFactory::Get();

    Renderer.Create(Window->GetHWND());
    UE_LOG(Engine, Debug, "Renderer created.");

    // Game.ini 로드 (해상도 설정 등)
    std::wstring GameIniPath = FPaths::GameSettingsFilePath();
    if (std::filesystem::exists(GameIniPath))
    {
        std::ifstream File(GameIniPath);
        if (File.is_open())
        {
            std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
            json::JSON Config = json::JSON::Load(Content);
            if (Config.hasKey("Resolution"))
            {
                int Width = Config["Resolution"][0].ToInt();
                int Height = Config["Resolution"][1].ToInt();
                UE_LOG(Engine, Info, "Setting resolution from Game.ini: %dx%d", Width, Height);
                // 실제 창 크기 조절 로직은 WinApplication에서 처리하는 것이 좋으나, 
                // 여기서는 렌더러 리사이즈 정도만 보장
                Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
            }
        }
    }

    GameViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
    UE_LOG(Engine, Debug, "GameViewportClient created.");

    ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
    FMeshBufferManager::Get().Initialize(Device);

    // 에셋 자동 스캔 (스탠드얼론 대응)
    FObjManager::ScanMeshAssets();
    FObjManager::ScanObjSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    UE_LOG(Engine, Info, "Asset registries scanned for runtime.");

    FResourceManager::Get().LoadFromFile(FPaths::ToUtf8(FPaths::ResourceFilePath()), Device);
    UE_LOG(Engine, Info, "Runtime engine initialization completed.");

	ScriptSystem.Initialize();
}

void UEngine::Shutdown()
{
    UE_LOG(Engine, Info, "Shutting down runtime engine.");
    FResourceManager::Get().ReleaseGPUResources();
    UTexture2D::ReleaseAllGPU();
    FObjManager::ReleaseAllGPU();
    FMeshBufferManager::Get().Release();
    Renderer.Release();

	ScriptSystem.Shutdown();
}

void UEngine::BeginPlay()
{
    // Startup Scene 로드
    std::wstring GameIniPath = FPaths::GameSettingsFilePath();
    FString StartupScenePath;
    if (std::filesystem::exists(GameIniPath))
    {
        std::ifstream File(GameIniPath);
        if (File.is_open())
        {
            std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
            json::JSON Config = json::JSON::Load(Content);
            if (Config.hasKey("StartupScene"))
            {
                StartupScenePath = Config["StartupScene"].ToString();
            }
        }
    }

    if (!StartupScenePath.empty())
    {
        UE_LOG(Engine, Info, "Loading startup scene: %s", StartupScenePath.c_str());
        
        FWorldContext LoadCtx;
        FPerspectiveCameraData DummyCam;
        FSceneSaveManager::LoadSceneFromJSON(FPaths::ToUtf8(FPaths::ContentDir()) + "Scene/" + StartupScenePath, LoadCtx, DummyCam);
        
        if (LoadCtx.World)
        {
            LoadCtx.WorldType = EWorldType::Game;
            LoadCtx.World->SetWorldType(EWorldType::Game);
            WorldList.push_back(LoadCtx);
            SetActiveWorld(LoadCtx.ContextHandle);
        }
        else
        {
            UE_LOG(Engine, Error, "Failed to load startup scene. Creating default world.");
            FWorldContext& Context = CreateWorldContext(EWorldType::Game, FName("GameWorld"));
            SetActiveWorld(Context.ContextHandle);
        }
    }
    else
    {
        // 지정된 시작 씬이 없으면 빈 기본 월드 생성
        FWorldContext& Context = CreateWorldContext(EWorldType::Game, FName("GameWorld"));
        SetActiveWorld(Context.ContextHandle);
    }

    FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    if (Context && Context->World)
    {
        if (Context->WorldType == EWorldType::Game || Context->WorldType == EWorldType::PIE)
        {
            Context->World->BeginPlay();
        }
    }
}

void UEngine::Tick(float DeltaTime)
{
    InputSystem::Get().Tick(Window->IsForeground());

    // FGameInput 상태 업데이트 (정적 입력 브릿지)
    const FInputSnapshot& Snapshot = InputSystem::Get().GetSnapshot();
    for (int i = 0; i < 256; ++i)
    {
        FGameInput::UpdateKeyState(i, Snapshot.KeyDown[i], Snapshot.KeyPressed[i], Snapshot.KeyReleased[i]);
    }

    if (GameViewportClient)
    {
        GameViewportClient->BeginInputFrame();
        GameViewportClient->Tick(DeltaTime);
    }

    WorldTick(DeltaTime);
    Render(DeltaTime);

    // 프레임 종료 시 입력 상태 리셋 (Pressed/Released 플래그)
    FGameInput::ResetFrameState();
}

void UEngine::Render(float DeltaTime)
{
    SCOPE_STAT_CAT("UEngine::Render", "2_Render");

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->BeginShadowFrame();
    }

    RenderTargets.Reset();
    FViewport* Viewport = GameViewportClient ? GameViewportClient->GetViewport() : nullptr;
    ID3D11DeviceContext* DeviceContext = Renderer.GetFD3DDevice().GetDeviceContext();

    UWorld* World = GetWorld();
    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;

    // 활성 카메라가 없으면 전역 Main 카메라로 폴백합니다.
    if (!Camera)
    {
        Camera = UCameraComponent::Main;
    }

    FScene* Scene = nullptr;
    if (Camera)
    {
        FShowFlags ShowFlags;
        EViewMode ViewMode = EViewMode::Lit_Phong;

        SceneView.SetCameraInfo(Camera);
        SceneView.SetRenderSettings(ViewMode, ShowFlags);

        if (Viewport && DeviceContext)
        {
            if (Viewport->ApplyPendingResize())
            {
                Camera->OnResize(static_cast<int32>(Viewport->GetWidth()), static_cast<int32>(Viewport->GetHeight()));
            }

            // Viewport 기반 렌더링 환경 설정
            RenderTargets.SetFromViewport(Viewport);
            SceneView.SetViewportInfo(Viewport);
        }
        else
        {
            // Viewport가 없는 경우 (Standalone 기본), 디바이스의 메인 백버퍼 정보를 사용합니다.
            const D3D11_VIEWPORT& DevVP = Renderer.GetFD3DDevice().GetViewport();
            SceneView.ViewportWidth = DevVP.Width;
            SceneView.ViewportHeight = DevVP.Height;
            Renderer.ReleaseViewModeSurfaces();
        }

        Scene = &World->GetScene();

        Renderer.BeginCollect(SceneView, Scene->GetPrimitiveProxyCount());

        FRenderCollectContext CollectContext = {};
        CollectContext.SceneView = &SceneView;
        CollectContext.Scene = Scene;
        CollectContext.Renderer = &Renderer;
        CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
        CollectContext.ActiveViewMode = SceneView.ViewMode;

        UWorld* CameraWorld = Camera->GetWorld();
        Renderer.CollectWorld(CameraWorld ? CameraWorld : World, CollectContext);
        Renderer.CollectDebugRender(*Scene);
    }
    else
    {
        Renderer.ReleaseViewModeSurfaces();
        Renderer.BeginCollect(SceneView);
    }

    {
        FRenderPipelineContext PipelineContext = Renderer.CreatePipelineContext(SceneView, &RenderTargets, Scene);
        
        // 지연된 Surface 확보
        if (Renderer.GetViewModePassRegistry() && Renderer.GetViewModePassRegistry()->HasConfig(SceneView.ViewMode))
        {
            PipelineContext.ViewMode.Surfaces =
                Renderer.AcquireViewModeSurfaces(Viewport, (uint32)SceneView.ViewportWidth, (uint32)SceneView.ViewportHeight);
        }

        Renderer.BuildDrawCommands(PipelineContext);
        
        // RenderFrame을 통해 BeginFrame -> RunRootPipeline -> EndFrame(Present) 과정을 완결합니다.
        Renderer.RenderFrame(ERenderPipelineType::DefaultRootPipeline, PipelineContext);
    }

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->EndShadowFrame();
    }
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    UE_LOG(Engine, Debug, "Window resized to %ux%u.", Width, Height);
    Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);

    if (GameViewportClient && GameViewportClient->GetViewport())
    {
        GameViewportClient->GetViewport()->Resize(Width, Height);
    }
}

void UEngine::WorldTick(float DeltaTime)
{
    SCOPE_STAT_CAT("UEngine::WorldTick", "1_WorldTick");

    bool bHasPIEWorld = false;
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::PIE && Ctx.World)
        {
            bHasPIEWorld = true;
            break;
        }
    }

    for (FWorldContext& Ctx : WorldList)
    {
        UWorld* World = Ctx.World;
        if (!World)
            continue;

        if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
        {
            continue;
        }

        const ELevelTick TickType = ToLevelTickType(Ctx.WorldType);

        World->Tick(DeltaTime, TickType);
    }
}

UWorld* UEngine::GetWorld() const
{
    const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
    return Context ? Context->World : nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
    FWorldContext Context;
    Context.WorldType = Type;
    Context.ContextHandle = Handle;
    Context.ContextName = Name.empty() ? Handle.ToString() : Name;
    Context.World = UObjectManager::Get().CreateObject<UWorld>();
    if (Context.World)
    {
        Context.World->SetWorldType(Type);
    }
    WorldList.push_back(Context);
    return WorldList.back();
}

void UEngine::DestroyWorldContext(const FName& Handle)
{
    for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
    {
        if (it->ContextHandle == Handle)
        {
            it->World->EndPlay();
            UObjectManager::Get().DestroyObject(it->World);
            WorldList.erase(it);
            return;
        }
    }
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle)
{
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.ContextHandle == Handle)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle) const
{
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.ContextHandle == Handle)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* World)
{
    for (FWorldContext& Ctx : WorldList)
    {
        if (Ctx.World == World)
        {
            return &Ctx;
        }
    }
    return nullptr;
}

void UEngine::SetActiveWorld(const FName& Handle)
{
    ActiveWorldHandle = Handle;
}
