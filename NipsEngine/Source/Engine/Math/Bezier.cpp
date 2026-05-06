#include "Bezier.h"

#include "Core/CoreTypes.h"
#include "Utils.h"
#include <cmath>

// 고정 끝점 P0=0, P3=1인 Cubic Bezier 값을 계산합니다.
float Bezier::EvaluateCubic(float T, float ControlPointA, float ControlPointB)
{
	const float ClampedT = MathUtil::Clamp(T, 0.0f, 1.0f);
	const float U = 1.0f - ClampedT;

	return (3.0f * U * U * ClampedT * ControlPointA)
		+ (3.0f * U * ClampedT * ClampedT * ControlPointB)
	+ (ClampedT * ClampedT * ClampedT);
}

// 고정 끝점 P0=0, P3=1인 Cubic Bezier 도함수 값을 계산합니다.
float Bezier::EvaluateCubicDerivative(float T, float ControlPointA, float ControlPointB)
{
	const float ClampedT = MathUtil::Clamp(T, 0.0f, 1.0f);
	const float U = 1.0f - ClampedT;

	return (3.0f * U * U * ControlPointA)
		+ (6.0f * U * ClampedT * (ControlPointB - ControlPointA))
		+ (3.0f * ClampedT * ClampedT * (1.0f - ControlPointB));
}

// 목표 시간 진행도(X)가 주어졌을 때, 그에 대응하는 Bezier Curve의 T를 계산합니다.
float Bezier::SolveTForX(float X, float ControlPointA, float ControlPointB)
{
	const float ClampedX = MathUtil::Clamp(X, 0.0f, 1.0f);
	const float ClampedA = MathUtil::Clamp(ControlPointA, 0.0f, 1.0f);
	const float ClampedB = MathUtil::Clamp(ControlPointB, 0.0f, 1.0f);

	float T = ClampedX;
	for (int32 Iteration = 0; Iteration < 5; ++Iteration)
	{
		const float CurrentX = EvaluateCubic(T, ClampedA, ClampedB);
		const float Slope = EvaluateCubicDerivative(T, ClampedA, ClampedB);
		if (std::abs(Slope) < 1.0e-5f)
		{
			break;
		}

		T = MathUtil::Clamp(T - (CurrentX - ClampedX) / Slope, 0.0f, 1.0f);
	}

	float MinT = 0.0f;
	float MaxT = 1.0f;
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		const float CurrentX = EvaluateCubic(T, ClampedA, ClampedB);
		if (std::abs(CurrentX - ClampedX) < 1.0e-5f)
		{
			break;
		}

		if (CurrentX < ClampedX)
		{
			MinT = T;
		}
		else
		{
			MaxT = T;
		}

		T = 0.5f * (MinT + MaxT);
	}

	return T;
}

// 정규화 시간 X에 대한 cubic-bezier(x1, y1, x2, y2) easing(가감속) 값을 계산합니다.
float Bezier::EvaluateCubicEasing(float X, const float ControlPoints[4])
{
	if (ControlPoints == nullptr)
	{
		return MathUtil::Clamp(X, 0.0f, 1.0f);
	}

	const float ControlPointAX = MathUtil::Clamp(ControlPoints[0], 0.0f, 1.0f);
	const float ControlPointBX = MathUtil::Clamp(ControlPoints[2], 0.0f, 1.0f);
	const float T = SolveTForX(X, ControlPointAX, ControlPointBX);
	return MathUtil::Clamp(EvaluateCubic(T, ControlPoints[1], ControlPoints[3]), 0.0f, 1.0f);
}

// 기존 카메라/에디터 코드와 호환되는 Bezier easing(가감속) 값을 계산합니다.
float Bezier::BezierValue(float T, const float ControlPoints[4])
{
	return EvaluateCubicEasing(T, ControlPoints);
}
