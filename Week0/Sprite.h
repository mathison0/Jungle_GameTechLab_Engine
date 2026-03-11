#pragma once
#include "dx11math.h"
#include "Image.h"

class Sprite : public Image
{
private:


public:

	Sprite(const std::string& textureName) :Image(textureName) {};
	virtual ~Sprite();


	virtual void Render(URenderer& renderer) override;
};

