#include "Image.h"

ID3D11Buffer* Image::QuadVertexBuffer = nullptr;


void Image::Render(URenderer& renderer)
{
	auto textureResourceView = renderer.GetTexture(textureName);

	if (textureResourceView == nullptr)
	{
		return;
	}

	renderer.DeviceContext->VSSetShader(renderer.backgroundVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.backgroundPixelShader, nullptr, 0);
	renderer.DeviceContext->PSSetShaderResources(0, 1, &textureResourceView);
	renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);

	renderer.UpdateConstant(position, 0.0f, scale);
	renderer.RenderPrimitive(QuadVertexBuffer, 6);

	renderer.DeviceContext->VSSetShader(renderer.SimpleVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.SimplePixelShader, nullptr, 0);


	ID3D11ShaderResourceView* nullSRV = nullptr;
	renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}