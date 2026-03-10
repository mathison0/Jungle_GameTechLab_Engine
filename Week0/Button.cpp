#include "Button.h"

void Button::Update(float mouseX, float mouseY, bool isMousePressed)
{
	if (IsMouseOver(mouseX, mouseY) && isMousePressed)
	{

		//Hover Effect


		if (OnClickCallback)
		{
			OnClickCallback();
		}
	}
}

void Button::Render(URenderer& renderer)
{
	Sprite::Render(renderer);

	if (textureName != "")
	{
		ID3D11ShaderResourceView* srv = renderer.GetTexture(textureName);
		if (srv)
		{
			renderer.DeviceContext->PSSetShaderResources(0, 1, &srv);
			renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);
			// Render the button quad
			renderer.UpdateConstant(position, 0.0f, scale, uvOffset);
			renderer.RenderPrimitive(QuadVertexBuffer, 6);
			// Unbind the texture after rendering
			ID3D11ShaderResourceView* nullSRV = nullptr;
			renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
		}

	}
}
