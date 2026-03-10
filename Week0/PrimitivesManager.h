#pragma once
#include "UBall.h"

class FPrimitivesManager
{
private:
	UPrimitive** PrimitiveList = nullptr;
	int fillPrimitiveCount = 0;
	int capacity = 100;

public:
	FPrimitivesManager()
	{
		PrimitiveList = new UPrimitive * [capacity];
	}

	void addElement(UPrimitive* element);

	void RemoveRandomElement();

	~FPrimitivesManager();

	UPrimitive* GetPrimitive(int index);

	void Update(const float deltaTime, const FVector3& ExternalForcePos);


	void Render(URenderer& renderer);
};