#pragma once
#include <functional>
#include "Sprite.h"

class Button : public Sprite
{
public:
	std::function<void()> OnClickCallback;

	Button(FVector3 position, FVector3 scale, const std::string& textureName, std::function<void()> onClickCallback)
		: Sprite(textureName), OnClickCallback(onClickCallback)
	{
		SetPosition(position.x, position.y);
		SetScale(scale.x, scale.y);
	}

	void Update(float mouseX, float mouseY, bool isMousePressed);

	inline bool IsMouseOver(float mx, float my) {
		float left = position.x - (scale.x * 0.5f);
		float right = position.x + (scale.x * 0.5f);
		float top = position.y + (scale.y * 0.5f);
		float bottom = position.y - (scale.y * 0.5f);

		return (mx >= left && mx <= right && my >= bottom && my <= top);
	}

	void Render(URenderer& renderer) override;

};

