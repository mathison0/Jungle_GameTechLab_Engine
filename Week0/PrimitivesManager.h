#pragma once
#include "UBall.h"
#include "planet.h"
#include "Camera.h"
#include "Sprite.h"
#include "IGameStateListener.h"

#include <string>

struct PlanetData
{
	std::string name;
	float relativeRadius;
};

class FPrimitivesManager : public IGameStateListener 
{
private:
	std::vector<UPrimitive*> objects;
	
	UBall* player = nullptr;
	Moon* moon = nullptr;
	Meteor* meteors[2] = { nullptr, nullptr };
	Camera* camera = nullptr;
	Sprite* background = nullptr;

	float highestPlayerY = 0.0f;
	float nextSpawnY = 1.0f;
	float spawnInterval = 1.5f;
	bool isRunning = false;

	static PlanetData planetDataList[];

public:
	FPrimitivesManager() {}
	~FPrimitivesManager();

	void AddObject(UPrimitive* obj);
	UPrimitive* GetPrimitive(int index);
	int GetObjectCount() const { return (int)objects.size(); }
	std::vector<UPrimitive*> GetObjects() const { return objects; }

	UBall* GetPlayer() const { return player; }
	Camera* GetCamera() const { return camera; }

	void InitializeGameObjects(URenderer renderer);
	void SpawnRandomPlanet(float spawnBaseY);
	void Reset();

	void Update(const float deltaTime, const FVector3& ExternalForcePos);
	void Render(URenderer& renderer);

	void OnGameStateChanged(EGameState newState) override;

};