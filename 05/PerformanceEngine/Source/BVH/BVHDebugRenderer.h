#pragma once

#include <memory>

#include "BVH/BVHTypes.h"
#include "Graphics/D3D11/D3D11Common.h"
#include "Picking/AABBNode.h"
#include "Types/Array.h"

class FCamera;
class FD3D11RHI;
class FScene;

class FBVHDebugRenderer
{
public:
	FBVHDebugRenderer();
	~FBVHDebugRenderer();

	bool Initialize(FD3D11RHI& InRHI);
	void Shutdown();

	void Render(const FD3D11RHI& InRHI, const FCamera& InCamera, const FScene& InScene);
	void RenderWorldNodeBoxes(const FD3D11RHI& InRHI, const FCamera& InCamera, const TArray<AABBNode>& InNodes);

private:
	bool BeginRenderPass(const FD3D11RHI& InRHI, const FCamera& InCamera, ID3D11DeviceContext*& OutDeviceContext);
	void EndRenderPass(ID3D11DeviceContext* InDeviceContext);
	void RenderNodes(ID3D11DeviceContext* InDeviceContext, const TArray<FBVHNode>& InNodes);
	void RenderWorldNodeBoxes(ID3D11DeviceContext* InDeviceContext, const TArray<AABBNode>& InNodes);

	struct FResources;
	std::unique_ptr<FResources> Resources;
};
