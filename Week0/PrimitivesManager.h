#pragma once
#include "UBall.h"
#include "planet.h"
#include "Camera.h"
#include "Sprite.h"
#include "GoalLine.h"
#include "IGameStateListener.h"

#include <string>
class URenderer;

struct PlanetData
{
	std::string name;
	float relativeRadius;
};

struct StageGroup
{
	std::string planetName;
	int count;
};

class FPrimitivesManager : public IGameStateListener 
{
private:
	std::vector<UPrimitive*> objects;
	
	UBall* player = nullptr;
	Moon* moon = nullptr;
	Meteor* meteors[2] = { nullptr, nullptr };
	Camera* camera = nullptr;
	Image* background = nullptr;
	GoalLine* goalLine = nullptr;

	float highestPlayerY = 0.0f;
	bool isRunning = false;

	static PlanetData planetDataList[];
	static std::vector<std::vector<StageGroup>> stageGroupData;

public:
	FPrimitivesManager() {}
	~FPrimitivesManager();

	void AddObject(UPrimitive* obj);
	UPrimitive* GetPrimitive(int index);
	int GetObjectCount() const { return (int)objects.size(); }
	std::vector<UPrimitive*> GetObjects() const { return objects; }

	UBall* GetPlayer() const { return player; }
	Camera* GetCamera() const { return camera; }

	void InitializeGameObjects();
	//void SpawnRandomPlanet();
	void SpawnPlanetsForAllStages();
	void SpawnPlanetsForStage(int stageIndex);
	void Reset();

	void Update(const float deltaTime, const FVector3& ExternalForcePos);
	void Render(URenderer& renderer);

	void OnGameStateChanged(EGameState newState) override;


	void ApplyCheat(char key);
};