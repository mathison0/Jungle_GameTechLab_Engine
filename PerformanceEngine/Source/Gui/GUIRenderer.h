#pragma once

#include <Windows.h>

class FD3D11RHI;
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
	bool Render(const FD3D11RHI& InRHI, FScene& InScene, const FPickState& InPickState);

	bool WantsMouseCapture() const;
	bool WantsKeyboardCapture() const;

private:
	bool DrawSelectedObjectWindow(FScene& InScene, const FPickState& InPickState);

private:
	bool bInitialized = false;
	bool bWantsMouseCapture = false;
	bool bWantsKeyboardCapture = false;
};
