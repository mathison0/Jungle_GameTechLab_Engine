#include "Core.h"

#include <array>
#include <cmath>
#include <filesystem>

#include "BVH/BVHDebugRenderer.h"
#include "Camera/Camera.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Grid/Grid.h"
#include "Hud/HudRenderer.h"
#include "Input/Input.h"
#include "Math/MathUtility.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Scene.h"
#include "Stats/StatsSystem.h"
#include "Visibility/VisibilitySystem.h"
#include "BVH/BVHBuilder.h"

namespace
{
	constexpr float DefaultCameraSpeed = 20.0f;
	constexpr float DefaultCameraSensitivity = 0.12f;
	constexpr float OcclusionHistoryInvalidateDistance = 10.0f;
	constexpr float OcclusionHistoryInvalidateAngleDegrees = 5.0f;

	std::filesystem::path SearchForSceneFrom(const std::filesystem::path& InStartDirectory)
	{
		static const std::array<std::filesystem::path, 2> RelativeCandidates =
		{
			std::filesystem::path(L"PerformanceEngine/Data/Scene/Default.scene"),
			std::filesystem::path(L"Data/Scene/Default.scene"),
		};

		std::filesystem::path Cursor = InStartDirectory;
		while (!Cursor.empty())
		{
			for (const std::filesystem::path& RelativeCandidate : RelativeCandidates)
			{
				const std::filesystem::path Candidate = Cursor / RelativeCandidate;
				if (std::filesystem::exists(Candidate))
				{
					return std::filesystem::absolute(Candidate);
				}
			}

			if (!Cursor.has_parent_path())
			{
				break;
			}

			const std::filesystem::path Parent = Cursor.parent_path();
			if (Parent == Cursor)
			{
				break;
			}

			Cursor = Parent;
		}

		return {};
	}

	std::filesystem::path FindDefaultScenePath()
	{
		if (const std::filesystem::path CurrentCandidate = SearchForSceneFrom(std::filesystem::current_path()); !CurrentCandidate.empty())
		{
			return CurrentCandidate;
		}

		std::array<wchar_t, MAX_PATH> ModulePath = {};
		const DWORD CharacterCount = GetModuleFileNameW(nullptr, ModulePath.data(), static_cast<DWORD>(ModulePath.size()));
		if (CharacterCount > 0)
		{
			const std::filesystem::path ModuleDirectory = std::filesystem::path(ModulePath.data()).parent_path();
			return SearchForSceneFrom(ModuleDirectory);
		}

		return {};
	}
}

FCore::FCore() = default;
FCore::~FCore() = default;

bool FCore::Initialize(const FCoreInitArgs& Args)
{
	if (Args.Hwnd == nullptr)
	{
		return false;
	}

	Input = std::make_unique<FInput>();
	Camera = std::make_unique<FCamera>();
	RHI = std::make_unique<FD3D11RHI>();
	Scene = std::make_unique<FScene>();
	SceneRenderer = std::make_unique<FSceneRenderer>();
	HudRenderer = std::make_unique<FHudRenderer>();
	VisibilitySystem = std::make_unique<FVisibilitySystem>();
	PickingSystem = std::make_unique<FPickingSystem>();
	StatsSystem = std::make_unique<FStatsSystem>();
	BVHDebugRenderer = std::make_unique<FBVHDebugRenderer>();


	if (!Input || !Camera || !RHI || !Scene || !SceneRenderer || !HudRenderer || !VisibilitySystem || !PickingSystem || !StatsSystem || !BVHDebugRenderer)
	{
		Release();
		return false;
	}

	if (!RHI->Initialize(Args.Hwnd))
	{
		Release();
		return false;
	}

	Grid = std::make_unique<FGrid>();
	if (Grid && !Grid->Initialize(*RHI))
	{
		OutputDebugStringA("[Core] Failed to initialize grid renderer. Continuing without grid.\n");
		Grid.reset();
	}

	if (!LoadDefaultScene())
	{
		Release();
		return false;
	}

	const FSceneCameraInitData& InitialCamera = Scene->GetInitialCamera();
	Camera->SetTransform(InitialCamera.Transform);
	Camera->SetFOV(InitialCamera.FovDegrees);
	Camera->SetNearClip(InitialCamera.NearClip);
	Camera->SetFarClip(InitialCamera.FarClip);
	Camera->SetSpeed(DefaultCameraSpeed);
	Camera->SetSensitivity(DefaultCameraSensitivity);

	const int32 ViewportWidth = RHI->GetViewportWidth();
	const int32 ViewportHeight = RHI->GetViewportHeight();
	if (ViewportWidth > 0 && ViewportHeight > 0)
	{
		Camera->SetAspectRatio(static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight));
	}
	else if (Args.Width > 0 && Args.Height > 0)
	{
		Camera->SetAspectRatio(static_cast<float>(Args.Width) / static_cast<float>(Args.Height));
	}

	if (!SceneRenderer->Initialize(*RHI) || !HudRenderer->Initialize(*RHI) || !BVHDebugRenderer->Initialize(*RHI))
	{
		Release();
		return false;
	}

	VisibilitySystem->Reset();
	PickingSystem->Reset();
	StatsSystem->Reset();
	VisibilityFrameInput = FVisibilityFrameInput();
	VisibilityResults = FVisibilityResults();
	PickState = FPickState();
	LastViewportWidth = Args.Width;
	LastViewportHeight = Args.Height;
	LastCameraFov = Camera ? Camera->GetFOV() : 0.0f;
	LastCameraLocation = Camera ? Camera->GetLocation() : FVector::ZeroVector;
	LastCameraForward = Camera ? Camera->GetTransform().GetUnitAxis(EAxis::X).GetSafeNormal() : FVector::ForwardVector;
	bHasLastCameraPose = Camera != nullptr;
	bInitialized = true;
	return true;
}

void FCore::Tick()
{
	StatsSystem->BeginFrame();
	Input->Tick();
	Camera->Update(*Input, static_cast<float>(StatsSystem->GetFrameTimeMs() * 0.001));

	const int32 CurrentViewportWidth = RHI ? RHI->GetViewportWidth() : 0;
	const int32 CurrentViewportHeight = RHI ? RHI->GetViewportHeight() : 0;
	const float CurrentCameraFov = Camera ? Camera->GetFOV() : 0.0f;
	const FVector CurrentCameraLocation = Camera ? Camera->GetLocation() : FVector::ZeroVector;
	const FVector CurrentCameraForward = Camera ? Camera->GetTransform().GetUnitAxis(EAxis::X).GetSafeNormal() : FVector::ForwardVector;

	bool bInvalidateOcclusionHistory = CurrentViewportWidth != LastViewportWidth
		|| CurrentViewportHeight != LastViewportHeight
		|| std::fabs(CurrentCameraFov - LastCameraFov) > 1.0e-4f;
	if (!bInvalidateOcclusionHistory && bHasLastCameraPose)
	{
		const float CameraTravelDistance = FVector::Dist(CurrentCameraLocation, LastCameraLocation);
		const float ForwardDot = FMath::Clamp(FVector::DotProduct(CurrentCameraForward, LastCameraForward), -1.0f, 1.0f);
		const float CameraTurnAngleDegrees = FMath::RadiansToDegrees(std::acos(ForwardDot));

		bInvalidateOcclusionHistory = CameraTravelDistance >= OcclusionHistoryInvalidateDistance
			|| CameraTurnAngleDegrees >= OcclusionHistoryInvalidateAngleDegrees;
	}

	if (bInvalidateOcclusionHistory)
	{
		InvalidateOcclusionState();
	}

	LastViewportWidth = CurrentViewportWidth;
	LastViewportHeight = CurrentViewportHeight;
	LastCameraFov = CurrentCameraFov;
	LastCameraLocation = CurrentCameraLocation;
	LastCameraForward = CurrentCameraForward;
	bHasLastCameraPose = Camera != nullptr;

	FVisibilityResults PreparedVisibilityResults;
	VisibilitySystem->PrepareFrame(*Scene, *Camera, VisibilityFrameInput, PreparedVisibilityResults);
	BeginFrame();

	TArray<uint32> VisibleClusterIndices;
	FVisibilityFrameInput ResolvedVisibilityFrameInput;
	FOcclusionTimingStats GpuOcclusionTimings = {};
	bool bResolvedDelayedResult = false;
	bool bHasPendingReadback = false;
	if (!SceneRenderer->ResolveGpuVisibility(
		*RHI,
		*Camera,
		VisibilityFrameInput,
		VisibleClusterIndices,
		GpuOcclusionTimings,
		ResolvedVisibilityFrameInput,
		bResolvedDelayedResult,
		bHasPendingReadback))
	{
		OutputDebugStringA("[Core] GPU visibility resolve failed. Falling back to frustum-visible cluster submission for this frame.\n");
		GpuOcclusionTimings = {};
		ResolvedVisibilityFrameInput = FVisibilityFrameInput();
		bResolvedDelayedResult = false;
		bHasPendingReadback = false;
		VisibleClusterIndices.clear();
	}

	if (bResolvedDelayedResult)
	{
		VisibilitySystem->FinalizeFrame(
			*Scene,
			ResolvedVisibilityFrameInput,
			VisibleClusterIndices,
			GpuOcclusionTimings,
			true,
			VisibilityResults);
	}
	else if (!bHasPendingReadback || VisibilityResults.FrameNumber == 0)
	{
		VisibilitySystem->FinalizeFrame(
			*Scene,
			VisibilityFrameInput,
			VisibleClusterIndices,
			GpuOcclusionTimings,
			false,
			VisibilityResults);
	}

	if (Input->IsMouseButtonPressed(FInput::MOUSE_LEFT))
	{
		PickingSystem->UpdatePick(
			*Scene,
			*Camera,
			VisibilityResults,
			Input->GetMousePositionClient(),
			RHI->GetViewportWidth(),
			RHI->GetViewportHeight(),
			PickState);
		PickingSystem->UpdatePickWorldBVH
		(
			*Scene,
			*Camera,
			VisibilityResults,
			Input->GetMousePositionClient(),
			RHI->GetViewportWidth(),
			RHI->GetViewportHeight(),
			PickState);
	}
	if (Input->IsKeyPressed('R'))
	{
		//Scene->GetRenderItems()[0].Transform.
		StatsSystem->RecordPickEvent(PickState);
	}
	StatsSystem->ApplyPickState(PickState);
	if (!SceneRenderer->RenderVisibleScene(*RHI, *Scene, *Camera, VisibilityResults, PickState))
	{
		InvalidateOcclusionState();
	}

	if (Grid)
	{
		Grid->Render(*RHI, *Camera);
	}


	// BVHDebugRenderer->Render(*RHI, *Camera, *Scene);
	HudRenderer->Render(*RHI, *Camera, *Scene, *StatsSystem, VisibilityResults, PickState);
	EndFrame();
	StatsSystem->EndFrame();
}

void FCore::Shutdown()
{
	Release();
}

bool FCore::HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (Input)
	{
		Input->ProcessMessage(Hwnd, Msg, WParam, LParam);
	}

	return false;
}

void FCore::HandleResize(int32 Width, int32 Height)
{
	if (!RHI || Width <= 0 || Height <= 0)
	{
		return;
	}

	RHI->Resize(Width, Height);

	if (Camera)
	{
		Camera->SetAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
	}

	LastViewportWidth = Width;
	LastViewportHeight = Height;
	InvalidateOcclusionState();
}

void FCore::Release()
{
	if (Grid)
	{
		Grid->Release();
		Grid.reset();
	}

	if (HudRenderer)
	{
		HudRenderer->Shutdown();
		HudRenderer.reset();
	}

	if (BVHDebugRenderer)
	{
		BVHDebugRenderer->Shutdown();
		BVHDebugRenderer.reset();
	}

	if (SceneRenderer)
	{
		SceneRenderer->Shutdown();
		SceneRenderer.reset();
	}

	if (Scene)
	{
		Scene->Release();
		Scene.reset();
	}

	if (bInitialized && StatsSystem && RHI)
	{
		FBenchmarkRunMetadata Metadata;
		Metadata.AdapterName = RHI->GetAdapterName();
		Metadata.DedicatedVideoMemoryMB = RHI->GetAdapterDedicatedVideoMemoryMB();
		Metadata.ViewportWidth = RHI->GetViewportWidth();
		Metadata.ViewportHeight = RHI->GetViewportHeight();
		StatsSystem->WriteBenchmarkLogs(Metadata);
	}

	if (RHI)
	{
		RHI->Shutdown();
		RHI.reset();
	}
	StatsSystem.reset();
	PickingSystem.reset();
	VisibilitySystem.reset();
	Camera.reset();
	Input.reset();

	VisibilityResults = FVisibilityResults();
	VisibilityFrameInput = FVisibilityFrameInput();
	PickState = FPickState();
	LastViewportWidth = 0;
	LastViewportHeight = 0;
	LastCameraFov = 0.0f;
	LastCameraLocation = FVector::ZeroVector;
	LastCameraForward = FVector::ForwardVector;
	bHasLastCameraPose = false;
	bInitialized = false;
}

void FCore::InvalidateOcclusionState()
{
	if (VisibilitySystem)
	{
		VisibilitySystem->InvalidateHistory();
	}

	if (SceneRenderer)
	{
		SceneRenderer->InvalidateDelayedVisibility();
	}
}

void FCore::BeginFrame()
{
	if (RHI)
	{
		RHI->BeginFrame();
	}
}

void FCore::EndFrame()
{
	if (RHI)
	{
		RHI->EndFrame();
	}
}

bool FCore::LoadDefaultScene()
{
	if (!Scene || !RHI)
	{
		return false;
	}

	const std::filesystem::path ScenePath = FindDefaultScenePath();
	if (ScenePath.empty())
	{
		return false;
	}

	if (!Scene->LoadFromFile(RHI->GetDevice(), RHI->GetDeviceContext(), ScenePath))
	{
		return false;
	}

	return true;
}
