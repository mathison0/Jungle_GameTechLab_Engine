#pragma once
#include "Sprite.h"
class Digit : public Sprite
{
private :
	char currentChar = '\0f';
	int offset = 32; // ' ' Index

public:
    Digit(FVector3 position, FVector3 scale, const std::string& textureName, int frameX, int frameY, float fullWidth, float fullHeight)
        : Sprite(position, scale, textureName, frameX, frameY, fullWidth, fullHeight) {
    }

    void SetChar(char c)
    {
        if (currentChar == c) return; 
        currentChar = c;

        int index = (int)c - offset;
        if (index < 0) index = 0; 

        int cols = GetMaxFrameX();

        int frameX = index % cols;
        int frameY = index / cols;

        this->SetFrame(frameX, frameY);
    }
};

