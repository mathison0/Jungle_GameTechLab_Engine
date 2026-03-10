#include "PrimitivesManager.h"

FPrimitivesManager::~FPrimitivesManager()
{
	for (UPrimitive* obj : objects)
	{
		if (obj != nullptr)
		{
			delete obj;
		}
	}
	objects.clear();
}

void FPrimitivesManager::AddObject(UPrimitive* obj)
{
	if (obj != nullptr)
	{
		objects.push_back(obj);
	}
}

UPrimitive* FPrimitivesManager::GetPrimitive(int index)
{
	if (index >= 0 && index < objects.size())
	{
		return objects[index];
	}
	return nullptr;
}

void FPrimitivesManager::Update(const float deltaTime, const FVector3& ExternalForcePos)
{
	// 모든 객체 업데이트
	for (UPrimitive* obj : objects)
	{
		if (obj != nullptr)
		{
			obj->Update(deltaTime);
			obj->ApplyAttraction(ExternalForcePos, 0.000001f);
		}
	}

	// 충돌 체크
	for (size_t i = 1; i < objects.size(); ++i)
	{
		objects[i]->HandleCollision(objects[0]);
	}
	for (size_t i = 2; i < objects.size(); ++i)
	{
		objects[i]->HandleCollision(objects[1]);
	}
}

void FPrimitivesManager::Render(URenderer& renderer)
{
	for (UPrimitive* obj : objects)
	{
		if (obj != nullptr)
		{
			obj->Render(renderer);
		}
	}
}