#pragma once

#include "Actor.h"

class UBillboardComponent;
class UArrowComponent;
class FArchive;

class ENGINE_API APlayerStart : public AActor
{
public:
	DECLARE_RTTI(APlayerStart, AActor)

	void PostSpawnInitialize() override;
	void Serialize(FArchive& Ar) override;
	void BeginPlay() override;
	virtual void Tick(float fTimeDelta) override;

private:
	void EnsureVisualizerComponents();

	UArrowComponent* ArrowComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
