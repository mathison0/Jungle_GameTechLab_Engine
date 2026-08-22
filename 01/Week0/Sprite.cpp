#include "Sprite.h"


Sprite::~Sprite()
{
}

void Sprite::Update(float deltaTime)
{

	curFrameX = curFrameX + 1 % maxFrameX;
}

void Sprite::Render(URenderer& renderer)
{

	auto textureResourceView = renderer.GetTexture(textureName);

	if (!textureResourceView)
	{
		return;
	}

	float scaleX = 1.0f / (float)maxFrameX;
	float scaleY = 1.0f / (float)maxFrameY;

	float offsetX = (float)curFrameX * scaleX;
	float offsetY = (float)curFrameY * scaleY;

	renderer.EnableAlphaBlending(true);

	renderer.DeviceContext->VSSetShader(renderer.UIVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.UIPixelShader, nullptr, 0);
	renderer.DeviceContext->PSSetShaderResources(0, 1, &textureResourceView);
	renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);

	renderer.UpdateConstant(position, angle, scale, color, { offsetX, offsetY }, { scaleX, scaleY }, flag);
	renderer.RenderPrimitive(QuadVertexBuffer, 6);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);

	renderer.EnableAlphaBlending(false);
}



