#include "Sprite.h"

ID3D11Buffer* Sprite::QuadVertexBuffer = nullptr;

Sprite::~Sprite()
{
}

void Sprite::Render(URenderer& renderer)
{
	textureResourceView = renderer.GetTexture(textureName);

	if (textureResourceView == nullptr)
	{
		return;
	}


	renderer.DeviceContext->VSSetShader(renderer.SpriteVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.SpritePixelShader, nullptr, 0);
	renderer.DeviceContext->PSSetShaderResources(0, 1, &textureResourceView);
	renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);

	renderer.UpdateConstant(position, 0.0f, scale, uvOffset);
	renderer.RenderPrimitive(QuadVertexBuffer, 6);

	renderer.DeviceContext->VSSetShader(renderer.SimpleVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.SimplePixelShader, nullptr, 0);


	ID3D11ShaderResourceView* nullSRV = nullptr;
	renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

