#include "CollisionShapeQuery.h"

#include "Math/Intersection.h"
#include "Math/MathUtils.h"

namespace CollisionShapeQuery
{
bool OverlapSphereSphere(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    const float RadiusSum = A.Radius + B.Radius;
    return (A.Center - B.Center).LengthSquared() <= RadiusSum * RadiusSum;
}

bool OverlapBoxSphere(const FCollisionShapeGeometry& Box, const FCollisionShapeGeometry& Sphere)
{
    const FVector LocalSphereCenter = Box.Rotation.Inverse().RotateVector(Sphere.Center - Box.Center);

    const FVector ClosestPoint(
        FMath::Clamp(LocalSphereCenter.X, -Box.BoxExtent.X, Box.BoxExtent.X),
        FMath::Clamp(LocalSphereCenter.Y, -Box.BoxExtent.Y, Box.BoxExtent.Y),
        FMath::Clamp(LocalSphereCenter.Z, -Box.BoxExtent.Z, Box.BoxExtent.Z));

    return (LocalSphereCenter - ClosestPoint).LengthSquared() <= Sphere.Radius * Sphere.Radius;
}

bool OverlapBoxBox(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    const FVector AxesA[3] = {
        A.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };
    const FVector AxesB[3] = {
        B.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };

    return FMath::IntersectOBBOBB(A.Center, A.BoxExtent, AxesA, B.Center, B.BoxExtent, AxesB);
}

static float SegmentSegmentDistanceSquared(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2)
{
    const FVector D1 = Q1 - P1;
    const FVector D2 = Q2 - P2;
    const FVector R = P1 - P2;
    const float A = D1.Dot(D1);
    const float E = D2.Dot(D2);
    const float F = D2.Dot(R);

    float S = 0.0f;
    float T = 0.0f;

    if (A <= FMath::Epsilon && E <= FMath::Epsilon)
    {
        return (P1 - P2).LengthSquared();
    }

    if (A <= FMath::Epsilon)
    {
        T = FMath::Clamp(F / E, 0.0f, 1.0f);
    }
    else
    {
        const float C = D1.Dot(R);
        if (E <= FMath::Epsilon)
        {
            S = FMath::Clamp(-C / A, 0.0f, 1.0f);
        }
        else
        {
            const float B = D1.Dot(D2);
            const float Denom = A * E - B * B;

            if (Denom != 0.0f)
            {
                S = FMath::Clamp((B * F - C * E) / Denom, 0.0f, 1.0f);
            }

            T = (B * S + F) / E;
            if (T < 0.0f)
            {
                T = 0.0f;
                S = FMath::Clamp(-C / A, 0.0f, 1.0f);
            }
            else if (T > 1.0f)
            {
                T = 1.0f;
                S = FMath::Clamp((B - C) / A, 0.0f, 1.0f);
            }
        }
    }

    const FVector Closest1 = P1 + D1 * S;
    const FVector Closest2 = P2 + D2 * T;
    return (Closest1 - Closest2).LengthSquared();
}

static void GetCapsuleSegment(const FCollisionShapeGeometry& Capsule, FVector& OutStart, FVector& OutEnd)
{
    const FVector Axis = Capsule.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized();
    const float SegmentHalf = (Capsule.HalfHeight > Capsule.Radius) ? (Capsule.HalfHeight - Capsule.Radius) : 0.0f;

    OutStart = Capsule.Center - Axis * SegmentHalf;
    OutEnd = Capsule.Center + Axis * SegmentHalf;
}

static float PointAABBDistanceSquared(const FVector& Point, const FVector& Extent)
{
    float DistanceSquared = 0.0f;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float Value = Point.Data[Axis];
        const float Min = -Extent.Data[Axis];
        const float Max = Extent.Data[Axis];

        if (Value < Min)
        {
            const float Delta = Min - Value;
            DistanceSquared += Delta * Delta;
        }
        else if (Value > Max)
        {
            const float Delta = Value - Max;
            DistanceSquared += Delta * Delta;
        }
    }

    return DistanceSquared;
}

static float SegmentAABBDistanceSquared(const FVector& Start, const FVector& End, const FVector& Extent)
{
    const FVector Direction = End - Start;
    float Candidates[8] = { 0.0f, 1.0f };
    int32 CandidateCount = 2;

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const float Delta = Direction.Data[Axis];
        if (std::abs(Delta) <= FMath::Epsilon)
        {
            continue;
        }

        const float MinT = (-Extent.Data[Axis] - Start.Data[Axis]) / Delta;
        const float MaxT = (Extent.Data[Axis] - Start.Data[Axis]) / Delta;

        if (MinT > 0.0f && MinT < 1.0f)
        {
            Candidates[CandidateCount++] = MinT;
        }

        if (MaxT > 0.0f && MaxT < 1.0f)
        {
            Candidates[CandidateCount++] = MaxT;
        }
    }

    for (int32 i = 1; i < CandidateCount; ++i)
    {
        const float Value = Candidates[i];
        int32 j = i - 1;
        while (j >= 0 && Candidates[j] > Value)
        {
            Candidates[j + 1] = Candidates[j];
            --j;
        }
        Candidates[j + 1] = Value;
    }

    float BestDistanceSquared = PointAABBDistanceSquared(Start, Extent);
    const float EndDistanceSquared = PointAABBDistanceSquared(End, Extent);
    if (EndDistanceSquared < BestDistanceSquared)
    {
        BestDistanceSquared = EndDistanceSquared;
    }

    for (int32 i = 0; i < CandidateCount - 1; ++i)
    {
        const float MinT = Candidates[i];
        const float MaxT = Candidates[i + 1];
        if (MaxT - MinT <= FMath::Epsilon)
        {
            continue;
        }

        const float MidT = (MinT + MaxT) * 0.5f;
        const FVector MidPoint = Start + Direction * MidT;
        float Numerator = 0.0f;
        float Denominator = 0.0f;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const float Value = MidPoint.Data[Axis];
            const float Min = -Extent.Data[Axis];
            const float Max = Extent.Data[Axis];
            float Bound = 0.0f;
            bool bOutside = false;

            if (Value < Min)
            {
                Bound = Min;
                bOutside = true;
            }
            else if (Value > Max)
            {
                Bound = Max;
                bOutside = true;
            }

            if (bOutside)
            {
                Numerator += Direction.Data[Axis] * (Start.Data[Axis] - Bound);
                Denominator += Direction.Data[Axis] * Direction.Data[Axis];
            }
        }

        float BestT = MinT;
        if (Denominator > FMath::Epsilon)
        {
            BestT = FMath::Clamp(-Numerator / Denominator, MinT, MaxT);
        }

        const float DistanceSquared = PointAABBDistanceSquared(Start + Direction * BestT, Extent);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
        }
    }

    return BestDistanceSquared;
}

bool OverlapCapsuleSphere(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Sphere)
{
    FVector SegmentStart;
    FVector SegmentEnd;
    GetCapsuleSegment(Capsule, SegmentStart, SegmentEnd);

    const float RadiusSum = Capsule.Radius + Sphere.Radius;
    return SegmentSegmentDistanceSquared(SegmentStart, SegmentEnd, Sphere.Center, Sphere.Center) <= RadiusSum * RadiusSum;
}

bool OverlapCapsuleBox(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Box)
{
    FVector SegmentStart;
    FVector SegmentEnd;
    GetCapsuleSegment(Capsule, SegmentStart, SegmentEnd);

    const FQuat InvBoxRotation = Box.Rotation.Inverse();
    const FVector LocalStart = InvBoxRotation.RotateVector(SegmentStart - Box.Center);
    const FVector LocalEnd = InvBoxRotation.RotateVector(SegmentEnd - Box.Center);

    return SegmentAABBDistanceSquared(LocalStart, LocalEnd, Box.BoxExtent) <= Capsule.Radius * Capsule.Radius;
}

bool OverlapCapsuleCapsule(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    FVector A0;
    FVector A1;
    FVector B0;
    FVector B1;
    GetCapsuleSegment(A, A0, A1);
    GetCapsuleSegment(B, B0, B1);

    const float RadiusSum = A.Radius + B.Radius;
    return SegmentSegmentDistanceSquared(A0, A1, B0, B1) <= RadiusSum * RadiusSum;
}

bool OverlapShapeGeometry(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    switch (A.Type)
    {
    case ECollisionShapeType::Sphere:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return OverlapSphereSphere(A, B);
        case ECollisionShapeType::Box:
            return OverlapBoxSphere(B, A);
        case ECollisionShapeType::Capsule:
            return OverlapCapsuleSphere(B, A);
        default:
            break;
        }
        break;

    case ECollisionShapeType::Box:
        switch (B.Type)
        {
        case ECollisionShapeType::Box:
            return OverlapBoxBox(A, B);
        case ECollisionShapeType::Sphere:
            return OverlapBoxSphere(A, B);
        case ECollisionShapeType::Capsule:
            return OverlapCapsuleBox(B, A);
        default:
            break;
        }
        break;

    case ECollisionShapeType::Capsule:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return OverlapCapsuleSphere(A, B);
        case ECollisionShapeType::Box:
            return OverlapCapsuleBox(A, B);
        case ECollisionShapeType::Capsule:
            return OverlapCapsuleCapsule(A, B);
        default:
            break;
        }
        break;
    }

    return false;
}
} // namespace CollisionShapeQuery
