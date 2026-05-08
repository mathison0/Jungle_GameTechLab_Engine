#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Mesh/SkeletalMesh.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override = default;

    void SetSkeletalMesh(USkeletalMesh* InMesh);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

    void Serialize(FArchive& Ar) override;
    
    // Property Editor 지원
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

private:
    USkeletalMesh* SkeletalMesh = nullptr;
    FString SkeletalMeshPath = "None";
};
