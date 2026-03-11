#pragma once
#include "dx11math.h"
#include "Image.h"

class Sprite : public Image
{
private:
	float width;
	float height;

	int maxFrameX = 1;
	int maxFrameY = 1;

	int curFrameX = 0;
	int curFrameY = 0;

public:

	Sprite(const std::string& textureName, int frameCountX, int frameCountY, float fullWidth, float fullHeight) :
		Image(textureName), maxFrameX{ frameCountX }, maxFrameY{ frameCountY}
	{
		this->width = fullWidth / (float)frameCountX;
		this->height = fullHeight / (float)frameCountY;

	};
	virtual ~Sprite();

	virtual void Render(URenderer& renderer) override;

	void Update(float deltaTime);
};

