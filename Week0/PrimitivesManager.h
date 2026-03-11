#pragma once
#include "UBall.h"
#include "IGameStateListener.h"

class FPrimitivesManager : public IGameStateListener 
{
private:
	std::vector<UPrimitive*> objects;

public:
	FPrimitivesManager() {}
	~FPrimitivesManager();

	void AddObject(UPrimitive* obj);
	UPrimitive* GetPrimitive(int index);
	int GetObjectCount() const { return objects.size(); }

	void Update(const float deltaTime, const FVector3& ExternalForcePos);
	void Render(URenderer& renderer);

	void OnGameStateChanged(EGameState newState) override;

};