#include "RefractionPass.h"
#include "RenderPassRegistry.h"

#include "Render/Command/DrawCommandList.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"

REGISTER_RENDER_PASS(FRefractionPass)

FRefractionPass::FRefractionPass()
{
	PassType = ERenderPass::Refraction;
	RenderState = {
		EDepthStencilState::DepthReadOnly,
		EBlendState::Opaque,
		ERasterizerState::SolidNoCull,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		true
	};
}

bool FRefractionPass::BeginPass(const FPassContext& Ctx)
{
	return Ctx.Frame.SceneColorCopyTexture
		&& Ctx.Frame.SceneColorCopySRV
		&& Ctx.Frame.ViewportRenderTexture
		&& Ctx.Frame.ViewportRTV;
}

void FRefractionPass::Execute(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	// 현재까지 그려진 화면을 굴절 샘플용으로 고정
	DC->CopyResource(Ctx.Frame.SceneColorCopyTexture, Ctx.Frame.ViewportRenderTexture);

	DC->OMSetRenderTargets(1, &Ctx.Frame.ViewportRTV, Ctx.Frame.ViewportDSV);

	ID3D11ShaderResourceView* SceneColorSRV = Ctx.Frame.SceneColorCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);

	uint32 Start = 0;
	uint32 End = 0;
	Ctx.CommandList.GetPassRange(ERenderPass::Refraction, Start, End);
	Ctx.CommandList.SubmitRange(Start, End, Ctx.Device, Ctx.Resources, Ctx.Cache);
}

void FRefractionPass::EndPass(const FPassContext& Ctx)
{
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Ctx.Device.GetDeviceContext()->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
}
