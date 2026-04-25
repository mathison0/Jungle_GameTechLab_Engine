#include "ShadowPass.h"

bool FShadowRenderPass::Initialize()
{
	return true;
}

bool FShadowRenderPass::Release()
{
	return true;
}

bool FShadowRenderPass::Begin(const FRenderPassContext* Context)
{
	return true;
}

bool FShadowRenderPass::DrawCommand(const FRenderPassContext* Context)
{
	// TODO : Directional


	// TODO : Spot

	return true;
}

bool FShadowRenderPass::End(const FRenderPassContext* Context)
{
	return true;
}
