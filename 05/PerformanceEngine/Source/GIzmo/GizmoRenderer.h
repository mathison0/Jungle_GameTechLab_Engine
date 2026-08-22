#pragma once

#include <memory>
#include <vector>

#include "Graphics/D3D11/D3D11Common.h"

class FCamera;
class FD3D11RHI;
struct FGizmoDrawCommand;

class FGizmoRenderer
{
public:
	FGizmoRenderer();
	~FGizmoRenderer();

	bool Initialize(FD3D11RHI& InRHI);
	void Shutdown();
	void Render(const FD3D11RHI& InRHI, const FCamera& InCamera, const std::vector<FGizmoDrawCommand>& InDrawCommands);

private:
	struct FResources;
	std::unique_ptr<FResources> Resources;
};
