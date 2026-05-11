#include "Skeleton.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkeleton, UObject)

void USkeleton::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    // FBoneInfo contains FName, so serialize each element through FBoneInfo::operator<<.
    uint32 ArrayNum = static_cast<uint32>(Bones.size());
    Ar << ArrayNum;

    if (Ar.IsLoading())
    {
        Bones.resize(ArrayNum);
    }

    for (auto& Bone : Bones)
    {
        Ar << Bone;
    }

    Ar << DisplayTransforms;

    if (Ar.IsLoading() && DisplayTransforms.size() != Bones.size())
    {
        DisplayTransforms.clear();
        DisplayTransforms.reserve(Bones.size());
        for (const FBoneInfo& Bone : Bones)
        {
            DisplayTransforms.push_back(Bone.ReferenceTransform);
        }
    }
}

int32 USkeleton::AddBone(FName InName, int32 InParentIndex, const FTransform& InRefTransform)
{
    FBoneInfo Bone;
    Bone.Name = InName;
    Bone.ParentIndex = InParentIndex;
    Bone.ReferenceTransform = InRefTransform;
    Bones.push_back(Bone);
    DisplayTransforms.push_back(InRefTransform);
    
    return static_cast<int32>(Bones.size() - 1);
}

bool USkeleton::SetBoneReferenceTransform(int32 BoneIndex, const FTransform& InRefTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return false;
    }

    Bones[BoneIndex].ReferenceTransform = InRefTransform;
    return true;
}

bool USkeleton::SetBoneDisplayTransform(int32 BoneIndex, const FTransform& InDisplayTransform)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(DisplayTransforms.size()))
    {
        return false;
    }

    DisplayTransforms[BoneIndex] = InDisplayTransform;
    return true;
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
