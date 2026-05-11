#include "Skeleton.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkeleton, UObject)

void USkeleton::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    
    // NOTE: 
    // Bones(TArray<FBoneInfo>)는 FBoneInfo가 trivially_copyable로 판정되어 
    // TArray의 operator<<에서 고속 복사가 일어날 수 있습니다.
    // 하지만 FBoneInfo::operator<<는 FName을 문자열로 변환하여 저장하는 커스텀 로직을 가지므로,
    // 반드시 개별 요소별로 루프를 돌며 직렬화해야 합니다.
    uint32 ArrayNum = static_cast<uint32>(Bones.size());
    Ar << ArrayNum;

    if (Ar.IsLoading())
        Bones.resize(ArrayNum);

    for (auto& Bone : Bones)
    {
        Ar << Bone;
    }
}

int32 USkeleton::AddBone(FName InName, int32 InParentIndex, const FTransform& InRefTransform)
{
    FBoneInfo Bone;
    Bone.Name = InName;
    Bone.ParentIndex = InParentIndex;
    Bone.ReferenceTransform = InRefTransform;
    Bones.push_back(Bone);
    
    return static_cast<int32>(Bones.size() - 1);
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
