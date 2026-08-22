#include "GoalLine.h"

void GoalLine::Render(URenderer& renderer)
{

	auto textureResourceView = renderer.GetTexture(textureName);

	if (!textureResourceView)
	{
		return;
	}

	renderer.DeviceContext->PSSetShaderResources(0, 1, &textureResourceView);
	renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);

	renderer.DeviceContext->VSSetShader(renderer.UIVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.UIPixelShader, nullptr, 0);

	renderer.UpdateConstant(position, 0.0f, scale, { 1,1,1,1 }, { 0,0 }, { 20.f, 1.f }, 0);
	renderer.RenderPrimitive(QuadVertexBuffer, 6);
}
