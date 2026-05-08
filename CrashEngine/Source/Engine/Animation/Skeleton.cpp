#include "Skeleton.h"

void USkeleton::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << Bones;
}

void USkeleton::AddBone(const FString& InName, int32 InParentIndex, const FTransform& InRefTransform)
{
    FBoneInfo Bone;
    Bone.Name = InName;
    Bone.ParentIndex = InParentIndex;
    Bone.ReferenceTransform = InRefTransform;
    Bones.push_back(Bone);
}

int32 USkeleton::FindBoneIndex(const FString& InName) const
{
    for (int32 i = 0; i < (int32)Bones.size(); ++i)
    {
        if (Bones[i].Name == InName)
        {
            return i;
        }
    }
    return -1;
}
