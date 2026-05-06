#include "PostProcessRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"

bool FPostProcessRenderPass::Initialize()
{
	return true;
}

bool FPostProcessRenderPass::Release()
{
	return true;
}

bool FPostProcessRenderPass::Begin(const FRenderPassContext* Context)
{
	OutSRV = PrevPassSRV ? PrevPassSRV : Context->RenderTargets->SceneColorSRV;
	OutRTV = PrevPassRTV ? PrevPassRTV : Context->RenderTargets->SceneColorRTV;
	return true;
}

bool FPostProcessRenderPass::DrawCommand(const FRenderPassContext* Context)
{
	Context->DeviceContext->Draw(3, 0);
	return true;
}

bool FPostProcessRenderPass::End(const FRenderPassContext* Context)
{
	(void)Context;
	return true;
}
