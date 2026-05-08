#pragma once

#include "Object/Object.h"
#include "Animation/Skeleton.h"

class UAnimationSequence : public UObject
{
public:
    DECLARE_CLASS(UAnimationSequence, UObject)

    UAnimationSequence() = default;

    void Serialize(FArchive& Ar) override
    {
        UObject::Serialize(Ar);
        // Animation data serialization will be implemented in the animation phase
    }

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

private:
    USkeleton* Skeleton = nullptr;
};
