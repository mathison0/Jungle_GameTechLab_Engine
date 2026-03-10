#include "PrimitivesManager.h"


void FPrimitivesManager::addElement(UPrimitive* element)
{
	if (UBall::GetTotalNumBalls() >= capacity)
	{
		// Resize
		int newCapacity = capacity * 2;
		UPrimitive** newList = new UPrimitive * [newCapacity];
		memcpy(newList, PrimitiveList, fillPrimitiveCount * sizeof(UPrimitive*));

		delete[] PrimitiveList;
		PrimitiveList = newList;
		capacity = newCapacity;
	}

	PrimitiveList[fillPrimitiveCount] = element;
	fillPrimitiveCount++;
}

void FPrimitivesManager::RemoveRandomElement()
{
	if (fillPrimitiveCount == 0)
	{
		return;
	}

	int indexToRemove = rand() % fillPrimitiveCount;
	delete PrimitiveList[indexToRemove];

	if (indexToRemove != fillPrimitiveCount - 1)
	{
		PrimitiveList[indexToRemove] = PrimitiveList[fillPrimitiveCount - 1];
	}

	fillPrimitiveCount--;
	PrimitiveList[fillPrimitiveCount] = nullptr;
}

FPrimitivesManager::~FPrimitivesManager()
{
	for (int i = 0; i < fillPrimitiveCount; ++i)
	{
		if (PrimitiveList[i] != nullptr)
		{
			delete PrimitiveList[i];
		}
	}
	delete[] PrimitiveList;
}

UPrimitive* FPrimitivesManager::GetPrimitive(int index)
{
	if (index >= 0 && index < fillPrimitiveCount) return PrimitiveList[index];
	return nullptr;
}

void FPrimitivesManager::Update(const float deltaTime, const FVector3& ExternalForcePos)
{
	for (int i = 0; i < fillPrimitiveCount; ++i)
	{
		UPrimitive* primitive = PrimitiveList[i];
		if (primitive != nullptr)
		{
			primitive->Update(deltaTime);
		}

		primitive->ApplyAttraction(ExternalForcePos, 0.000001f);

	}
	for (int i = 0; i < fillPrimitiveCount; ++i)
	{
		for (int j = i + 1; j < fillPrimitiveCount; ++j)
		{
			if (PrimitiveList[i] != nullptr && PrimitiveList[j] != nullptr)
			{
				PrimitiveList[i]->HandleCollision(PrimitiveList[j]);
			}
		}
	}
}

void FPrimitivesManager::Render(URenderer& renderer)
{
	for (int i = 0; i < fillPrimitiveCount; ++i)
	{
		UPrimitive* primitive = PrimitiveList[i];
		if (primitive != nullptr)
		{
			primitive->Render(renderer); // ∞¥√ºø°∞‘ ∑ª¥ı∏µ¿ª ¿ß¿”!
		}
	}
}