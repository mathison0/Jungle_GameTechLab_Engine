#include "Button.h"

void Button::Update(float mouseX, float mouseY, bool isMousePressed)
{
	if (IsMouseOver(mouseX, mouseY))
	{
		//Hover Effect
		color = { 0.5f, 0.5f, 0.5f };
		
		if (OnClickCallback && isMousePressed)
		{
			OnClickCallback();
		}
	}

	else
	{
		color = { 1.0f, 1.0f, 1.0f };
	}
}

void Button::Render(URenderer& renderer)
{
	//Sprite::Render(renderer);

	if (textureName != "")
	{
		ID3D11ShaderResourceView* srv = renderer.GetTexture(textureName);
		if (srv)
		{
			renderer.EnableAlphaBlending(true);
			
			renderer.DeviceContext->VSSetShader(renderer.UIVertexShader, nullptr, 0);
			renderer.DeviceContext->PSSetShader(renderer.UIPixelShader, nullptr, 0);
			renderer.DeviceContext->PSSetShaderResources(0, 1, &srv);
			renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);

			renderer.UpdateConstant(position, 0.0f, scale, uvOffset, color);
			renderer.RenderPrimitive(QuadVertexBuffer, 6);

			ID3D11ShaderResourceView* nullSRV = nullptr;
			renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);

			renderer.EnableAlphaBlending(false);
		}

	}
}
