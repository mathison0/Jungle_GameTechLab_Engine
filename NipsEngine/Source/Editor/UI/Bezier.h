#pragma once

namespace Bezier
{
	float BezierValue(float t, const float cp[4]);
	int   Bezier(const char* label, float cp[4]);
}