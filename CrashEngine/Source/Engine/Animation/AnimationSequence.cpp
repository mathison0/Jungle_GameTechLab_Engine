#include "Animation/AnimationSequence.h"
#include "Animation/SkeletonManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UAnimationSequence, UObject)

void UAnimationSequence::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    FString SkeletonPath;
    if (Ar.IsSaving() && Skeleton)
    {
        SkeletonPath = Skeleton->GetFName().ToString(); 
    }
    
    Ar << SkeletonPath;

    Ar << NumFrames;
    Ar << SequenceLength;
    Ar << FPS;
    Ar << Tracks;
}
