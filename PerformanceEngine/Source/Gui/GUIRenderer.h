#pragma once

#include <Windows.h>

#include "Types/PlatformTypes.h"
#include "Types/String.h"

class FD3D11RHI;
class FCamera;
class FScene;
struct FPickState;

class FGUIRenderer
{
public:
	FGUIRenderer();
	~FGUIRenderer();

	bool Initialize(FD3D11RHI& InRHI, HWND InWindowHandle);
	void Shutdown();

	bool HandleMessage(HWND InWindowHandle, UINT InMessage, WPARAM InWParam, LPARAM InLParam);
	bool Render(const FD3D11RHI& InRHI, FScene& InScene, const FCamera& InCamera, FPickState& InOutPickState);

	bool WantsMouseCapture() const;
	bool WantsKeyboardCapture() const;

private:
	bool DrawSceneWindow(FScene& InScene, const FCamera& InCamera, FPickState& InOutPickState);
	bool DrawSelectedObjectWindow(FScene& InScene, const FPickState& InPickState);

private:
	bool bInitialized = false;
	bool bWantsMouseCapture = false;
	bool bWantsKeyboardCapture = false;
	int32 SceneWindowSelectedMeshIndex = 0;
	FString SceneWindowSelectedMeshAssetKey;
};
