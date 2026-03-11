#pragma once
#include "dx11math.h"
#include "Image.h"

class Sprite : public Image
{
protected:
	float width;
	float height;

	int maxFrameX = 1;
	int maxFrameY = 1;

	int curFrameX = 1;
	int curFrameY = 1;

	bool flag = 1;

public:

	Sprite(FVector3 position, FVector3 scale, const std::string& textureName, int frameCountX, int frameCountY, float fullWidth, float fullHeight) :
		Image(textureName), maxFrameX{ frameCountX }, maxFrameY{ frameCountY}
	{
		SetPosition(position.x, position.y);
		SetScale(scale.x, scale.y);

		this->width = fullWidth / (float)frameCountX;
		this->height = fullHeight / (float)frameCountY;

	};
	virtual ~Sprite();

	virtual void Render(URenderer& renderer) override;

	inline int GetMaxFrameX() { return maxFrameX; }
	inline int GetMaxFrameY() { return maxFrameY; }

	void SetFrame(int frameX, int frameY)
	{
		curFrameX = frameX;
		curFrameY = frameY;
	}

	void SetFlag(bool active) { flag = active; }

	void Update(float deltaTime);
};

