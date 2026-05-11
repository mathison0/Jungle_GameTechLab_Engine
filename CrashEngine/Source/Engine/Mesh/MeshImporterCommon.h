#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

// Blender 스타일 Forward 축 선택
enum class EForwardAxis : uint8
{
    X,
    NegX, // +X, -X
    Y,
    NegY, // +Y, -Y
    Z,
    NegZ // +Z, -Z
};

// EWindingOrder는 메시 처리에서 사용할 선택지를 정의합니다.
enum class EWindingOrder : uint8
{
    CCW_to_CW, // OBJ CCW → DX CW (인덱스 [0,2,1]) — 기본값
    Keep       // 원본 유지 [0,1,2]
};

// FImportOptions는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct FImportOptions
{
    float Scale = 1.0f;
    EForwardAxis ForwardAxis = EForwardAxis::NegY; // Blender 기본: Z-up, -Y Forward
    EWindingOrder WindingOrder = EWindingOrder::CCW_to_CW;
    static FImportOptions Default()
    {
        // NOTE: 
        // FBX SDK에서 축 보정하면서 CCW_to_CW 변환까지 자동으로 적용해주는 경우가 있어서 
        // Default Option은 WindingOrder 항목을 Keep으로 둡니다.
        
        FImportOptions Options;
        Options.WindingOrder = EWindingOrder::Keep;
        return Options;
    }
};

class FMeshImporterUtils
{
public:
    static FVector RemapPosition(const FVector& InPos, EForwardAxis Axis)
    {
        switch (Axis)
        {
        case EForwardAxis::X:
            return FVector(InPos.X, InPos.Z, InPos.Y);
        case EForwardAxis::NegX:
            return FVector(-InPos.X, InPos.Z, InPos.Y);
        case EForwardAxis::Y:
            return FVector(InPos.Y, InPos.Z, InPos.X);
        case EForwardAxis::NegY:
            return FVector(-InPos.Y, InPos.Z, InPos.X);
        case EForwardAxis::Z:
            return FVector(InPos.Z, InPos.X, InPos.Y);
        case EForwardAxis::NegZ:
            return FVector(-InPos.Z, InPos.X, InPos.Y);
        default:
            return FVector(InPos.X, InPos.Z, InPos.Y);
        }
    }
};
