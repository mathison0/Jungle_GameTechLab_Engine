#include "Transform.h"

#include <cmath>

FTransform::FTransform(const FMatrix& Mat)
{
	Location = Mat.GetLocation();
	Scale = Mat.GetScale();

	// 회전 추출 전에 스케일 제거 — FQuat::FromMatrix 는 정규직교 행렬을 가정하므로
	// (trace 기반 분기) 스케일이 섞인 행렬을 그대로 넘기면 분기 판정이 깨져
	// 특정 자세/각도 경계에서 회전이 180° 뒤집힌다 (본 부착 무기 플립 버그의 원인).
	constexpr float DecomposeTolerance = 1.0e-6f;
	FMatrix RotationMatrix = Mat;
	RotationMatrix.M[3][0] = 0.0f;
	RotationMatrix.M[3][1] = 0.0f;
	RotationMatrix.M[3][2] = 0.0f;
	RotationMatrix.M[3][3] = 1.0f;

	if (std::fabs(Scale.X) > DecomposeTolerance)
	{
		RotationMatrix.M[0][0] /= Scale.X;
		RotationMatrix.M[0][1] /= Scale.X;
		RotationMatrix.M[0][2] /= Scale.X;
	}
	if (std::fabs(Scale.Y) > DecomposeTolerance)
	{
		RotationMatrix.M[1][0] /= Scale.Y;
		RotationMatrix.M[1][1] /= Scale.Y;
		RotationMatrix.M[1][2] /= Scale.Y;
	}
	if (std::fabs(Scale.Z) > DecomposeTolerance)
	{
		RotationMatrix.M[2][0] /= Scale.Z;
		RotationMatrix.M[2][1] /= Scale.Z;
		RotationMatrix.M[2][2] /= Scale.Z;
	}

	Rotation = RotationMatrix.ToQuat().GetNormalized();
}

FMatrix FTransform::ToMatrix() const
{
	FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

	FMatrix rotationMatrix = Rotation.ToMatrix();

	FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

	return scaleMatrix * rotationMatrix * translateMatrix;
}

FVector FTransform::TransformPosition(const FVector& Point) const
{
	FMatrix transformMatrix = ToMatrix();
	return transformMatrix.TransformPositionWithW(Point);
}
