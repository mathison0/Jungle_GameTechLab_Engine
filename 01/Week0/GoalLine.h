#pragma once
#include "Image.h"

class GoalLine : public Image
{
public:
	GoalLine(const std::string& textureName) : Image(textureName) 
	{
		position = { 0.f, 100.0f, 0.f };
		SetScale(2.0f, 0.1f);
	};
	~GoalLine() {};

	void Render(URenderer& renderer) override;
};

