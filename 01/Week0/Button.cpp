#include "Button.h"

void Button::Update(float mouseX, float mouseY, bool isMousePressed)
{
	if(IsActive() == false)
	{
		return;
	}

	if (IsMouseOver(mouseX, mouseY))
	{
		//Hover Effect
		color.x = 0.5f;
		color.y = 0.5f;
		color.z = 0.5f;

		if (OnClickCallback && isMousePressed)
		{
			SetActive(false);
			OnClickCallback();
		}
	}

	else
	{
		color.x = 1.0f;
		color.y = 1.0f;
		color.z = 1.0f;
	}
}

void Button::Render(URenderer& renderer)
{
	if (IsActive() == false)
	{
		return;
	}

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

			renderer.UpdateConstant(position, 0.0f, scale, color);
			renderer.RenderPrimitive(QuadVertexBuffer, 6);

			ID3D11ShaderResourceView* nullSRV = nullptr;
			renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);

			renderer.EnableAlphaBlending(false);
		}

	}
}
