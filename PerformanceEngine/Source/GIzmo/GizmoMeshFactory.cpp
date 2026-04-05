#include "Gizmo/GizmoMeshFactory.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr float HalfPi = 0.5f * Pi;
	constexpr float TwoPi = 2.0f * Pi;

	constexpr float AxisLength = 35.0f;
	constexpr float ScaleAxisLength = 25.0f;
	constexpr float CylinderRadius = 0.6f;
	constexpr float ConeHeadOffset = 12.0f;
	constexpr float ConeScale = -13.0f;
	constexpr int32 AxisCircleSides = 24;
	constexpr float RotationInnerRadius = 44.0f;
	constexpr float RotationOuterRadius = 52.0f;
	constexpr float TranslateCornerStart = 7.0f;
	constexpr float TranslateCornerLength = 12.0f;
	constexpr float TranslateCornerThickness = 1.2f;
	constexpr float TranslateScreenSphereRadius = 4.0f;
	constexpr float ScaleLineHalfThickness = 0.35f;
	constexpr float ScaleCenterCubeHalf = 2.5f;

	struct FGizmoBasis3
	{
		FVector X = FVector::ForwardVector;
		FVector Y = FVector::RightVector;
		FVector Z = FVector::UpVector;
	};

	FVector NormalizeSafe(const FVector& InVector, const FVector& InFallback)
	{
		const FVector Result = InVector.GetSafeNormal();
		return Result.IsNearlyZero() ? InFallback : Result;
	}

	float DegreesToRadians(float InDegrees)
	{
		return InDegrees * (Pi / 180.0f);
	}

	FGizmoColor MakeColor(float InR, float InG, float InB, float InA = 1.0f)
	{
		return FGizmoColor{ InR, InG, InB, InA };
	}

	FGizmoColor AxisColorX()
	{
		return MakeColor(0.594f, 0.0197f, 0.0f, 1.0f);
	}

	FGizmoColor AxisColorY()
	{
		return MakeColor(0.1349f, 0.3959f, 0.0f, 1.0f);
	}

	FGizmoColor AxisColorZ()
	{
		return MakeColor(0.0251f, 0.2070f, 0.85f, 1.0f);
	}

	FGizmoColor ScreenColor()
	{
		return MakeColor(196.0f / 255.0f, 196.0f / 255.0f, 196.0f / 255.0f, 1.0f);
	}

	FGizmoBasis3 MakeBasisFromX(const FVector& InAxis)
	{
		const FVector X = NormalizeSafe(InAxis, FVector::ForwardVector);
		const FVector UpSeed = (std::fabs(X.X) < 0.95f) ? FVector::ForwardVector : FVector::RightVector;
		const FVector Z = NormalizeSafe(FVector::CrossProduct(X, UpSeed), FVector::UpVector);
		const FVector Y = NormalizeSafe(FVector::CrossProduct(Z, X), FVector::RightVector);
		return FGizmoBasis3{ X, Y, Z };
	}

	FGizmoBasis3 MakeBasisFromXAndNormal(const FVector& InAxis, const FVector& InPreferredNormal)
	{
		const FVector X = NormalizeSafe(InAxis, FVector::ForwardVector);
		FVector Z = NormalizeSafe(InPreferredNormal, FVector::UpVector);
		if (std::fabs(FVector::DotProduct(X, Z)) > 0.999f)
		{
			Z = (std::fabs(X.Z) < 0.95f) ? FVector::UpVector : FVector::RightVector;
		}

		const FVector Y = NormalizeSafe(FVector::CrossProduct(Z, X), FVector::RightVector);
		Z = NormalizeSafe(FVector::CrossProduct(X, Y), Z);
		return FGizmoBasis3{ X, Y, Z };
	}

	FVector TransformVector(const FGizmoBasis3& InBasis, const FVector& InLocal)
	{
		return InBasis.X * InLocal.X + InBasis.Y * InLocal.Y + InBasis.Z * InLocal.Z;
	}

	FVector TransformPoint(const FGizmoBasis3& InBasis, const FVector& InOrigin, const FVector& InLocal)
	{
		return InOrigin + TransformVector(InBasis, InLocal);
	}

	FVector TransformNormal(const FGizmoBasis3& InBasis, const FVector& InLocal)
	{
		return NormalizeSafe(TransformVector(InBasis, InLocal), FVector::UpVector);
	}

	FVector SwapXZ(const FVector& InValue)
	{
		return FVector(InValue.Z, InValue.Y, InValue.X);
	}

	uint32 ComputeCircleSides(int32 InTransformGizmoSize)
	{
		if (InTransformGizmoSize > 0)
		{
			return static_cast<uint32>(AxisCircleSides + (InTransformGizmoSize / 5));
		}

		return static_cast<uint32>(AxisCircleSides);
	}

	void AppendTriangle(FGizmoMesh& InOutMesh, uint32 InA, uint32 InB, uint32 InC)
	{
		InOutMesh.Indices.push_back(InA);
		InOutMesh.Indices.push_back(InB);
		InOutMesh.Indices.push_back(InC);
	}

	void AppendVertex(FGizmoMesh& InOutMesh, const FVector& InPosition, const FVector& InNormal, const FGizmoColor& InColor)
	{
		FGizmoVertex Vertex;
		Vertex.Position = InPosition;
		Vertex.Normal = InNormal;
		Vertex.Color = InColor;
		InOutMesh.Vertices.push_back(Vertex);
	}

	void AppendQuad(
		FGizmoMesh& InOutMesh,
		const FVector& InV0,
		const FVector& InV1,
		const FVector& InV2,
		const FVector& InV3,
		const FVector& InNormal,
		const FGizmoColor& InColor)
	{
		const uint32 BaseIndex = static_cast<uint32>(InOutMesh.Vertices.size());
		AppendVertex(InOutMesh, InV0, InNormal, InColor);
		AppendVertex(InOutMesh, InV1, InNormal, InColor);
		AppendVertex(InOutMesh, InV2, InNormal, InColor);
		AppendVertex(InOutMesh, InV3, InNormal, InColor);
		AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 1, BaseIndex + 2);
		AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 2, BaseIndex + 3);
	}

	void AppendLocalVertex(
		FGizmoMesh& InOutMesh,
		const FGizmoBasis3& InBasis,
		const FVector& InOrigin,
		const FVector& InLocalPosition,
		const FVector& InLocalNormal,
		const FGizmoColor& InColor)
	{
		AppendVertex(
			InOutMesh,
			TransformPoint(InBasis, InOrigin, InLocalPosition),
			TransformNormal(InBasis, InLocalNormal),
			InColor);
	}

	void AppendLocalQuad(
		FGizmoMesh& InOutMesh,
		const FGizmoBasis3& InBasis,
		const FVector& InOrigin,
		const FVector& InV0,
		const FVector& InV1,
		const FVector& InV2,
		const FVector& InV3,
		const FVector& InLocalNormal,
		const FGizmoColor& InColor)
	{
		AppendQuad(
			InOutMesh,
			TransformPoint(InBasis, InOrigin, InV0),
			TransformPoint(InBasis, InOrigin, InV1),
			TransformPoint(InBasis, InOrigin, InV2),
			TransformPoint(InBasis, InOrigin, InV3),
			TransformNormal(InBasis, InLocalNormal),
			InColor);
	}

	void AppendOrientedBox(FGizmoMesh& InOutMesh, const FVector& InCenter, const FGizmoBasis3& InBasis, const FVector& InHalfExtents, const FGizmoColor& InColor)
	{
		const FVector PX = InBasis.X * InHalfExtents.X;
		const FVector PY = InBasis.Y * InHalfExtents.Y;
		const FVector PZ = InBasis.Z * InHalfExtents.Z;

		const FVector Corners[8] =
		{
			InCenter - PX - PY - PZ,
			InCenter + PX - PY - PZ,
			InCenter + PX + PY - PZ,
			InCenter - PX + PY - PZ,
			InCenter - PX - PY + PZ,
			InCenter + PX - PY + PZ,
			InCenter + PX + PY + PZ,
			InCenter - PX + PY + PZ,
		};

		AppendQuad(InOutMesh, Corners[1], Corners[5], Corners[6], Corners[2], InBasis.X, InColor);
		AppendQuad(InOutMesh, Corners[4], Corners[0], Corners[3], Corners[7], InBasis.X * -1.0f, InColor);
		AppendQuad(InOutMesh, Corners[3], Corners[2], Corners[6], Corners[7], InBasis.Y, InColor);
		AppendQuad(InOutMesh, Corners[0], Corners[4], Corners[5], Corners[1], InBasis.Y * -1.0f, InColor);
		AppendQuad(InOutMesh, Corners[4], Corners[5], Corners[6], Corners[7], InBasis.Z, InColor);
		AppendQuad(InOutMesh, Corners[0], Corners[3], Corners[2], Corners[1], InBasis.Z * -1.0f, InColor);
	}

	void AppendCylinder(FGizmoMesh& InOutMesh, const FVector& InStart, const FVector& InEnd, float InRadius, uint32 InSides, const FGizmoColor& InColor)
	{
		InSides = std::max<uint32>(InSides, 3u);
		const FVector Axis = NormalizeSafe(InEnd - InStart, FVector::ForwardVector);
		const FGizmoBasis3 Basis = MakeBasisFromX(Axis);
		const uint32 BaseIndex = static_cast<uint32>(InOutMesh.Vertices.size());

		for (uint32 SideIndex = 0; SideIndex < InSides; ++SideIndex)
		{
			const float Angle = TwoPi * static_cast<float>(SideIndex) / static_cast<float>(InSides);
			const FVector Radial = Basis.Y * std::cos(Angle) + Basis.Z * std::sin(Angle);
			AppendVertex(InOutMesh, InStart + Radial * InRadius, Radial, InColor);
			AppendVertex(InOutMesh, InEnd + Radial * InRadius, Radial, InColor);
		}

		for (uint32 SideIndex = 0; SideIndex < InSides; ++SideIndex)
		{
			const uint32 NextIndex = (SideIndex + 1) % InSides;
			const uint32 V0 = BaseIndex + SideIndex * 2 + 0;
			const uint32 V1 = BaseIndex + SideIndex * 2 + 1;
			const uint32 V2 = BaseIndex + NextIndex * 2 + 1;
			const uint32 V3 = BaseIndex + NextIndex * 2 + 0;
			AppendTriangle(InOutMesh, V0, V2, V1);
			AppendTriangle(InOutMesh, V0, V3, V2);
		}
	}

	FVector CalcConeVertex(float InAngle1, float InAngle2, float InAzimuthAngle)
	{
		const float SinXOver2 = std::sin(0.5f * InAngle1);
		const float SinYOver2 = std::sin(0.5f * InAngle2);
		const float SinSqXOver2 = SinXOver2 * SinXOver2;
		const float SinSqYOver2 = SinYOver2 * SinYOver2;
		const float Phi = std::atan2(std::sin(InAzimuthAngle) * SinSqYOver2, std::cos(InAzimuthAngle) * SinSqXOver2);
		const float SinPhi = std::sin(Phi);
		const float CosPhi = std::cos(Phi);
		const float SinSqPhi = SinPhi * SinPhi;
		const float CosSqPhi = CosPhi * CosPhi;
		const float RadiusSq = SinSqXOver2 * SinSqYOver2 / (SinSqXOver2 * SinSqPhi + SinSqYOver2 * CosSqPhi);
		const float Radius = std::sqrt(RadiusSq);
		const float Root = std::sqrt(1.0f - RadiusSq);
		const float Alpha = Radius * CosPhi;
		const float Beta = Radius * SinPhi;
		return FVector(1.0f - 2.0f * RadiusSq, 2.0f * Root * Alpha, 2.0f * Root * Beta);
	}

	void AppendUnrealCone(FGizmoMesh& InOutMesh, const FVector& InTipPosition, const FVector& InAxisDirection, float InScale, float InAngle, uint32 InSides, const FGizmoColor& InColor)
	{
		const FGizmoBasis3 Basis = MakeBasisFromX(InAxisDirection);
		for (uint32 SideIndex = 0; SideIndex < InSides; ++SideIndex)
		{
			const uint32 NextIndex = (SideIndex + 1) % InSides;
			const float Angle0 = TwoPi * static_cast<float>(SideIndex) / static_cast<float>(InSides);
			const float Angle1 = TwoPi * static_cast<float>(NextIndex) / static_cast<float>(InSides);
			const FVector Tip = InTipPosition;
			const FVector Point0 = InTipPosition + TransformVector(Basis, CalcConeVertex(InAngle, InAngle, Angle0) * InScale);
			const FVector Point1 = InTipPosition + TransformVector(Basis, CalcConeVertex(InAngle, InAngle, Angle1) * InScale);
			const FVector Normal = NormalizeSafe(FVector::CrossProduct(Point0 - Tip, Point1 - Tip), InAxisDirection);

			const uint32 BaseIndex = static_cast<uint32>(InOutMesh.Vertices.size());
			AppendVertex(InOutMesh, Tip, Normal, InColor);
			AppendVertex(InOutMesh, Point0, Normal, InColor);
			AppendVertex(InOutMesh, Point1, Normal, InColor);
			AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 1, BaseIndex + 2);
		}
	}

	void AppendSphere(FGizmoMesh& InOutMesh, const FVector& InCenter, float InRadius, uint32 InSlices, uint32 InStacks, const FGizmoColor& InColor)
	{
		InSlices = std::max<uint32>(InSlices, 3u);
		InStacks = std::max<uint32>(InStacks, 2u);
		const uint32 BaseIndex = static_cast<uint32>(InOutMesh.Vertices.size());

		for (uint32 StackIndex = 0; StackIndex <= InStacks; ++StackIndex)
		{
			const float V = static_cast<float>(StackIndex) / static_cast<float>(InStacks);
			const float Phi = V * Pi;
			const float SinPhi = std::sin(Phi);
			const float CosPhi = std::cos(Phi);

			for (uint32 SliceIndex = 0; SliceIndex <= InSlices; ++SliceIndex)
			{
				const float U = static_cast<float>(SliceIndex) / static_cast<float>(InSlices);
				const float Theta = U * TwoPi;
				const FVector Normal(SinPhi * std::cos(Theta), SinPhi * std::sin(Theta), CosPhi);
				AppendVertex(InOutMesh, InCenter + Normal * InRadius, Normal, InColor);
			}
		}

		const uint32 Stride = InSlices + 1;
		for (uint32 StackIndex = 0; StackIndex < InStacks; ++StackIndex)
		{
			for (uint32 SliceIndex = 0; SliceIndex < InSlices; ++SliceIndex)
			{
				const uint32 A = BaseIndex + StackIndex * Stride + SliceIndex;
				const uint32 B = A + Stride;
				const uint32 C = B + 1;
				const uint32 D = A + 1;
				AppendTriangle(InOutMesh, A, B, C);
				AppendTriangle(InOutMesh, A, C, D);
			}
		}
	}

	void AppendSegmentBox(FGizmoMesh& InOutMesh, const FVector& InStart, const FVector& InEnd, float InHalfThickness, const FVector& InPlaneNormal, const FGizmoColor& InColor)
	{
		const FVector Segment = InEnd - InStart;
		const float Length = Segment.Size();
		if (Length <= 1.0e-6f)
		{
			return;
		}

		const FGizmoBasis3 Basis = MakeBasisFromXAndNormal(Segment, InPlaneNormal);
		const FVector Center = (InStart + InEnd) * 0.5f;
		AppendOrientedBox(InOutMesh, Center, Basis, FVector(Length * 0.5f, InHalfThickness, InHalfThickness), InColor);
	}

	void AppendCornerHelperStrip(FGizmoMesh& InOutMesh, const FGizmoBasis3& InBasis, const FVector& InOrigin, const FVector& InLength, float InThickness, const FGizmoColor& InColor, bool bSwapXZ)
	{
		const float TX = InLength.X * 0.5f;
		const float TY = InLength.Y * 0.5f;
		const float TZ = InLength.Z * 0.5f;
		const float TH = InThickness;

		const auto MapLocal = [&](const FVector& InValue) -> FVector
		{
			return bSwapXZ ? SwapXZ(InValue) : InValue;
		};

		const auto AddLocalVertex = [&](const FVector& InLocalPosition, const FVector& InLocalNormal)
		{
			AppendLocalVertex(InOutMesh, InBasis, InOrigin, MapLocal(InLocalPosition), MapLocal(InLocalNormal), InColor);
		};

		AppendLocalQuad(
			InOutMesh,
			InBasis,
			InOrigin,
			MapLocal(FVector(-TX, -TY, +TZ)),
			MapLocal(FVector(-TX, +TY, +TZ)),
			MapLocal(FVector(+TX, +TY, +TZ)),
			MapLocal(FVector(+TX, -TY, +TZ)),
			MapLocal(FVector(0.0f, 0.0f, 1.0f)),
			InColor);

		AppendLocalQuad(
			InOutMesh,
			InBasis,
			InOrigin,
			MapLocal(FVector(-TX, -TY, TZ - TH)),
			MapLocal(FVector(-TX, -TY, TZ)),
			MapLocal(FVector(-TX, +TY, TZ)),
			MapLocal(FVector(-TX, +TY, TZ - TH)),
			MapLocal(FVector(-1.0f, 0.0f, 0.0f)),
			InColor);

		{
			const uint32 BaseIndex = static_cast<uint32>(InOutMesh.Vertices.size());
			const FVector LocalNormal = MapLocal(FVector(0.0f, 1.0f, 0.0f));
			AddLocalVertex(FVector(-TX, +TY, TZ - TH), LocalNormal);
			AddLocalVertex(FVector(-TX, +TY, +TZ), LocalNormal);
			AddLocalVertex(FVector(+TX - TH, +TY, +TZ), LocalNormal);
			AddLocalVertex(FVector(+TX, +TY, +TZ), LocalNormal);
			AddLocalVertex(FVector(+TX - TH, +TY, TZ - TH), LocalNormal);
			AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 1, BaseIndex + 2);
			AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 2, BaseIndex + 4);
			AppendTriangle(InOutMesh, BaseIndex + 4, BaseIndex + 2, BaseIndex + 3);
		}

		{
			const uint32 BaseIndex = static_cast<uint32>(InOutMesh.Vertices.size());
			const FVector LocalNormal = MapLocal(FVector(0.0f, -1.0f, 0.0f));
			AddLocalVertex(FVector(-TX, -TY, TZ - TH), LocalNormal);
			AddLocalVertex(FVector(-TX, -TY, +TZ), LocalNormal);
			AddLocalVertex(FVector(+TX - TH, -TY, +TZ), LocalNormal);
			AddLocalVertex(FVector(+TX, -TY, +TZ), LocalNormal);
			AddLocalVertex(FVector(+TX - TH, -TY, TZ - TH), LocalNormal);
			AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 2, BaseIndex + 1);
			AppendTriangle(InOutMesh, BaseIndex + 0, BaseIndex + 4, BaseIndex + 2);
			AppendTriangle(InOutMesh, BaseIndex + 4, BaseIndex + 3, BaseIndex + 2);
		}

		AppendLocalQuad(
			InOutMesh,
			InBasis,
			InOrigin,
			MapLocal(FVector(-TX, -TY, TZ - TH)),
			MapLocal(FVector(-TX, +TY, TZ - TH)),
			MapLocal(FVector(+TX - TH, +TY, TZ - TH)),
			MapLocal(FVector(+TX - TH, -TY, TZ - TH)),
			MapLocal(FVector(0.0f, 0.0f, -1.0f)),
			InColor);
	}

	void AppendTranslatePlane(FGizmoMesh& InOutMesh, const FVector& InAxis0, const FVector& InAxis1, const FVector& InNormal, const FGizmoColor& InAxis0Color, const FGizmoColor& InAxis1Color, float InScale)
	{
		const FGizmoBasis3 LocalToWorld
		{
			NormalizeSafe(InAxis0, FVector::ForwardVector),
			NormalizeSafe(InNormal, FVector::UpVector),
			NormalizeSafe(InAxis1, FVector::RightVector),
		};
		const FVector Origin = InAxis0 * (TranslateCornerStart * InScale) + InAxis1 * (TranslateCornerStart * InScale);
		const FVector Length(TranslateCornerLength * InScale, TranslateCornerThickness * InScale, TranslateCornerLength * InScale);
		const float Thickness = TranslateCornerThickness * InScale;

		AppendCornerHelperStrip(InOutMesh, LocalToWorld, Origin, Length, Thickness, InAxis1Color, false);
		AppendCornerHelperStrip(InOutMesh, LocalToWorld, Origin, Length, Thickness, InAxis0Color, true);
	}

	void AppendScalePlane(FGizmoMesh& InOutMesh, const FVector& InAxis0, const FVector& InAxis1, const FVector& InNormal, const FGizmoColor& InAxis0Color, const FGizmoColor& InAxis1Color, float InScale)
	{
		const FVector Point0 = InAxis0 * (24.0f * InScale);
		const FVector Point1 = InAxis0 * (12.0f * InScale) + InAxis1 * (12.0f * InScale);
		const FVector Point2 = InAxis1 * (24.0f * InScale);
		AppendSegmentBox(InOutMesh, Point0, Point1, ScaleLineHalfThickness * InScale, InNormal, InAxis0Color);
		AppendSegmentBox(InOutMesh, Point1, Point2, ScaleLineHalfThickness * InScale, InNormal, InAxis1Color);
	}

	void AppendRingBand(FGizmoMesh& InOutMesh, const FGizmoBasis3& InBasis, float InInnerRadius, float InOuterRadius, float InStartAngle, float InEndAngle, const FGizmoColor& InColor, uint32 InCircleSides)
	{
		const float Range = InEndAngle - InStartAngle;
		const uint32 QuarterSides = std::max<uint32>(InCircleSides, 4u);
		const uint32 PointCount = std::max<uint32>(2u, static_cast<uint32>(std::floor(QuarterSides * std::fabs(Range) / HalfPi)) + 1u);
		const FVector Normal = NormalizeSafe(InBasis.Z, FVector::UpVector);

		const uint32 OuterBase = static_cast<uint32>(InOutMesh.Vertices.size());
		for (uint32 PointIndex = 0; PointIndex <= PointCount; ++PointIndex)
		{
			const float T = static_cast<float>(PointIndex) / static_cast<float>(PointCount);
			const float Angle = InStartAngle + Range * T;
			const FVector Direction = NormalizeSafe(InBasis.X * std::cos(Angle) + InBasis.Y * std::sin(Angle), InBasis.X);
			AppendVertex(InOutMesh, Direction * InOuterRadius, Normal, InColor);
		}

		const uint32 InnerBase = static_cast<uint32>(InOutMesh.Vertices.size());
		for (uint32 PointIndex = 0; PointIndex <= PointCount; ++PointIndex)
		{
			const float T = static_cast<float>(PointIndex) / static_cast<float>(PointCount);
			const float Angle = InStartAngle + Range * T;
			const FVector Direction = NormalizeSafe(InBasis.X * std::cos(Angle) + InBasis.Y * std::sin(Angle), InBasis.X);
			AppendVertex(InOutMesh, Direction * InInnerRadius, Normal, InColor);
		}

		for (uint32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			AppendTriangle(InOutMesh, OuterBase + PointIndex, OuterBase + PointIndex + 1, InnerBase + PointIndex);
			AppendTriangle(InOutMesh, OuterBase + PointIndex + 1, InnerBase + PointIndex + 1, InnerBase + PointIndex);
		}
	}

	FGizmoMesh CreateTranslationAxisMesh(const FVector& InAxis, const FGizmoColor& InColor, float InScale, uint32 InCircleSides)
	{
		(void)InCircleSides;
		FGizmoMesh Mesh;
		const FVector Start = FVector::ZeroVector;
		const FVector End = InAxis * (AxisLength * InScale);
		const float ConeTip = AxisLength + ConeHeadOffset;
		const float ConeAngle = DegreesToRadians(Pi * 5.0f);
		AppendCylinder(Mesh, Start, End, CylinderRadius * InScale, 16, InColor);
		AppendUnrealCone(
			Mesh,
			InAxis * (ConeTip * InScale),
			InAxis,
			ConeScale * InScale,
			ConeAngle,
			32,
			InColor);
		return Mesh;
	}

	FGizmoMesh CreateScaleAxisMesh(const FVector& InAxis, const FVector& InPlaneNormal, const FGizmoColor& InColor, float InScale)
	{
		FGizmoMesh Mesh;
		AppendSegmentBox(Mesh, FVector::ZeroVector, InAxis * (ScaleAxisLength * InScale), ScaleLineHalfThickness * InScale, InPlaneNormal, InColor);
		const FGizmoBasis3 Basis = MakeBasisFromXAndNormal(InAxis, InPlaneNormal);
		AppendOrientedBox(Mesh, InAxis * (ScaleAxisLength * InScale), Basis, FVector(2.2f * InScale, 2.2f * InScale, 2.2f * InScale), InColor);
		return Mesh;
	}
}

FTranslationGizmoMeshes GenerateTranslationGizmoMeshes(const FTranslationGizmoDesc& InDesc)
{
	FTranslationGizmoMeshes Result;
	const float Scale = InDesc.UniformScale;
	const uint32 CircleSides = ComputeCircleSides(InDesc.TransformGizmoSize);

	Result.AxisX = CreateTranslationAxisMesh(FVector::ForwardVector, AxisColorX(), Scale, CircleSides);
	Result.AxisY = CreateTranslationAxisMesh(FVector::RightVector, AxisColorY(), Scale, CircleSides);
	Result.AxisZ = CreateTranslationAxisMesh(FVector::UpVector, AxisColorZ(), Scale, CircleSides);

	AppendTranslatePlane(Result.PlaneXY, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, AxisColorX(), AxisColorY(), Scale);
	AppendTranslatePlane(Result.PlaneXZ, FVector::ForwardVector, FVector::UpVector, FVector::RightVector * -1.0f, AxisColorX(), AxisColorZ(), Scale);
	AppendTranslatePlane(Result.PlaneYZ, FVector::RightVector, FVector::UpVector, FVector::ForwardVector, AxisColorY(), AxisColorZ(), Scale);

	if (InDesc.bIncludeScreenHandle)
	{
		AppendSphere(Result.ScreenSphere, FVector::ZeroVector, TranslateScreenSphereRadius * Scale, 10, 5, ScreenColor());
	}

	return Result;
}

FRotationGizmoMeshes GenerateRotationGizmoMeshes(const FRotationGizmoDesc& InDesc)
{
	FRotationGizmoMeshes Result;
	const float Scale = InDesc.UniformScale;
	const uint32 CircleSides = ComputeCircleSides(InDesc.TransformGizmoSize);

	const FGizmoBasis3 XBASIS{ FVector::RightVector, FVector::UpVector, FVector::ForwardVector };
	const FGizmoBasis3 YBASIS{ FVector::UpVector, FVector::ForwardVector, FVector::RightVector };
	const FGizmoBasis3 ZBASIS{ FVector::ForwardVector, FVector::RightVector, FVector::UpVector };

	AppendRingBand(Result.RingX, XBASIS, RotationInnerRadius * Scale, RotationOuterRadius * Scale, 0.0f, TwoPi, AxisColorX(), CircleSides);
	AppendRingBand(Result.RingY, YBASIS, RotationInnerRadius * Scale, RotationOuterRadius * Scale, 0.0f, TwoPi, AxisColorY(), CircleSides);
	AppendRingBand(Result.RingZ, ZBASIS, RotationInnerRadius * Scale, RotationOuterRadius * Scale, 0.0f, TwoPi, AxisColorZ(), CircleSides);

	if (InDesc.bIncludeScreenRing)
	{
		const FGizmoBasis3 ScreenBasis
		{
			NormalizeSafe(InDesc.ViewRight, FVector::RightVector),
			NormalizeSafe(InDesc.ViewUp, FVector::UpVector),
			NormalizeSafe(InDesc.CameraDirection, FVector::ForwardVector),
		};
		AppendRingBand(Result.ScreenRing, ScreenBasis, RotationInnerRadius * Scale, RotationOuterRadius * Scale, 0.0f, TwoPi, ScreenColor(), CircleSides);
	}

	return Result;
}

FScaleGizmoMeshes GenerateScaleGizmoMeshes(const FScaleGizmoDesc& InDesc)
{
	FScaleGizmoMeshes Result;
	const float Scale = InDesc.UniformScale;

	Result.AxisX = CreateScaleAxisMesh(FVector::ForwardVector, FVector::UpVector, AxisColorX(), Scale);
	Result.AxisY = CreateScaleAxisMesh(FVector::RightVector, FVector::UpVector, AxisColorY(), Scale);
	Result.AxisZ = CreateScaleAxisMesh(FVector::UpVector, FVector::ForwardVector, AxisColorZ(), Scale);

	AppendScalePlane(Result.PlaneXY, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, AxisColorX(), AxisColorY(), Scale);
	AppendScalePlane(Result.PlaneXZ, FVector::ForwardVector, FVector::UpVector, FVector::RightVector, AxisColorX(), AxisColorZ(), Scale);
	AppendScalePlane(Result.PlaneYZ, FVector::RightVector, FVector::UpVector, FVector::ForwardVector, AxisColorY(), AxisColorZ(), Scale);

	if (InDesc.bIncludeCenterCube)
	{
		AppendOrientedBox(Result.CenterCube, FVector::ZeroVector, FGizmoBasis3{}, FVector(ScaleCenterCubeHalf * Scale, ScaleCenterCubeHalf * Scale, ScaleCenterCubeHalf * Scale), ScreenColor());
	}

	return Result;
}
