#pragma once
#include "GameFramework/AActor.h"

class AExplodeVfxActor : public AActor
{
public:
    DECLARE_CLASS(AExplodeVfxActor, AActor);
    void InitDefaultComponents() override;
};
