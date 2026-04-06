#pragma once

#include <memory>
#include <Windows.h>

#include "BVH/BVHDebugRenderer.h"
#include "Picking/PickingSystem.h"
#include "Visibility/VisibilitySystem.h"

#include "Types/PlatformTypes.h"

class FGrid;
class FCamera;
class FD3D11RHI;
class FGUIRenderer;
class FGizmo;
class FGizmoRenderer;
class FHudRenderer;
class FInput;
class FPickingSystem;
class FScene;
class FSceneRenderer;
class FStatsSystem;
class FVisibilitySystem;
class FWindowsWindow;
class FBVHBuilder;

struct FCoreInitArgs
{
	FWindowsWindow* MainWindow = nullptr;
	HWND Hwnd = nullptr;
	int32 Width = 0;
	int32 Height = 0;
};

class FCore
{
public:
	FCore();
	~FCore();

	FCore(const FCore&) = delete;
	FCore(FCore&&) = delete;
	FCore& operator=(const FCore&) = delete;
	FCore& operator=(FCore&&) = delete;

	bool Initialize(const FCoreInitArgs& Args);
	void Tick();
	void Shutdown();
	bool HandleMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void HandleResize(int32 Width, int32 Height);

	void Release();

private:
	void BeginFrame();
	void EndFrame();
	bool LoadDefaultScene();
	void InvalidateOcclusionState();

private:
	std::unique_ptr<FD3D11RHI> RHI;
	std::unique_ptr<FInput> Input;
	std::unique_ptr<FCamera> Camera;
	std::unique_ptr<FScene> Scene;
	std::unique_ptr<FSceneRenderer> SceneRenderer;
	std::unique_ptr<FGizmoRenderer> GizmoRenderer;
	std::unique_ptr<FHudRenderer> HudRenderer;
	std::unique_ptr<FGUIRenderer> GUIRenderer;
	std::unique_ptr<FGizmo> Gizmo;
	std::unique_ptr<FVisibilitySystem> VisibilitySystem;
	std::unique_ptr<FPickingSystem> PickingSystem;
	std::unique_ptr<FStatsSystem> StatsSystem;
	std::unique_ptr<FGrid> Grid;
	std::unique_ptr<FBVHDebugRenderer> BVHDebugRenderer;

	FVisibilityFrameInput VisibilityFrameInput;
	FVisibilityResults VisibilityResults;
	FPickState PickState;
	int32 LastViewportWidth = 0;
	int32 LastViewportHeight = 0;
	float LastCameraFov = 0.0f;
	FVector LastCameraLocation = FVector::ZeroVector;
	FVector LastCameraForward = FVector::ForwardVector;
	bool bHasLastCameraPose = false;

	bool bInitialized = false;
};
