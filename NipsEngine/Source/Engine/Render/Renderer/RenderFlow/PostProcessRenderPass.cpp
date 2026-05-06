#include "PostProcessRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"

bool FPostProcessRenderPass::Initialize()
{
	return true;
}

bool FPostProcessRenderPass::Release()
{
	ShaderBinding.reset();
	return true;
}

bool FPostProcessRenderPass::Begin(const FRenderPassContext* Context)
{
	OutSRV = Context->RenderTargets->ScenePostProcessSRV;
	OutRTV = Context->RenderTargets->ScenePostProcessRTV;
	
	const FRenderTargetSet* RenderTargets = Context->RenderTargets;
	ID3D11RenderTargetView* RTVs[1] = { RenderTargets->ScenePostProcessRTV };
	Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, nullptr);

	UShader* PostProcessShader = FResourceManager::Get().GetShader("Shaders/Multipass/PostProcessPass.hlsl");
	if (!PostProcessShader)
	{
		return false;
	}

	if (!ShaderBinding || ShaderBinding->GetShader() != PostProcessShader)
	{
		ShaderBinding = PostProcessShader->CreateBindingInstance(Context->Device);
	}

	if (!ShaderBinding)
	{
		return false;
	}

	ShaderBinding->ApplyFrameParameters(*Context->RenderBus);
	ShaderBinding->SetSRV("FinalSceneColor", PrevPassSRV);

	float Width = Context->RenderTargets->Width;
	float Height = Context->RenderTargets->Height;
	ShaderBinding->SetVector2("InvResolution", FVector2((Width > 0.0f) ? (1.0f / Width) : 0.0f, (Height > 0.0f) ? (1.0f / Height) : 0.0f));
	ShaderBinding->SetAllSamplers(FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear));

	Context->DeviceContext->IASetInputLayout(nullptr);
	Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return true;
}

bool FPostProcessRenderPass::DrawCommand(const FRenderPassContext* Context)
{
	if (!ShaderBinding)
	{
		return false;
	}

	const FPostProcessSettings& PostProcess = Context->RenderBus->GetPostProcessSettings();
	const FCameraOverlaySettings& Overlay = Context->RenderBus->GetCameraOverlaySettings();

	ShaderBinding->ApplyFrameParameters(*Context->RenderBus);

	ShaderBinding->SetVector4("FadeColor", FVector4(Overlay.FadeColor));
	ShaderBinding->SetFloat("VignetteIntensity", PostProcess.VignetteIntensity);
	ShaderBinding->SetFloat("VignetteRadius", PostProcess.VignetteRadius);
	ShaderBinding->SetFloat("VignetteSoftness", PostProcess.VignetteSoftness);

	ShaderBinding->SetFloat("Gamma", PostProcess.Gamma);
	ShaderBinding->SetFloat("LetterBoxRatio", Overlay.LetterBoxRatio);

	ShaderBinding->Bind(Context->DeviceContext);
	Context->DeviceContext->Draw(3, 0);
	return true;
}

bool FPostProcessRenderPass::End(const FRenderPassContext* Context)
{
	ID3D11ShaderResourceView* NullSRVs[] = { nullptr };
	Context->DeviceContext->PSSetShaderResources(0, 1, NullSRVs);
	return true;
}
