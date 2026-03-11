#pragma once
#include "Sprite.h"

#include <string>
class TimeLap
{
private:
	std::vector<Sprite*> digits;
	float spacing = 5.0f;
	float totalWidth = 0.0f;
	float maxHeight = 0.0f;

	float totalSeconds = 0.0f;

	std::string currentTimeStr = "00:00";
};

