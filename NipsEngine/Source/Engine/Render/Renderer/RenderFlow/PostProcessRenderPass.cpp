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

	/*  이 패스는 범용 scene post process 슬롯으로 남겨둡니다.
		anti-aliasing 전후에 맞춰야 하는 카메라 색 보정은 현재 FFXAARenderPass에서 처리하고,
		fade/letterbox 같은 최종 화면 overlay는 FPostProcessOutlineRenderPass에서 처리합니다.
		bloom/tonemap/color grading 등이 이곳에 구현되기 전까지는 기존 ScenePostProcess
		렌더 타겟 handoff를 유지하는 fullscreen pass-through 역할을 합니다. */

	ShaderBinding->ApplyFrameParameters(*Context->RenderBus);
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
