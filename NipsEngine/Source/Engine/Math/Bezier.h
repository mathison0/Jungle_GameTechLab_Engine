#pragma once

namespace Bezier
{
	float EvaluateCubic(float T, float ControlPointA, float ControlPointB);
	float EvaluateCubicDerivative(float T, float ControlPointA, float ControlPointB);
	float SolveTForX(float X, float ControlPointA, float ControlPointB);
	float EvaluateCubicEasing(float X, const float ControlPoints[4]);
	float BezierValue(float T, const float ControlPoints[4]);
}
