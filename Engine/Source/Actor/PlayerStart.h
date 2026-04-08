#pragma once

#include "Actor.h"

class UBillboardComponent;

class ENGINE_API APlayerStart : public AActor
{
public:
	DECLARE_RTTI(APlayerStart, AActor)

	void PostSpawnInitialize() override;
	virtual void Tick(float fTimeDelta) override;
private:
	UBillboardComponent* BillboardComponent = nullptr;
};
