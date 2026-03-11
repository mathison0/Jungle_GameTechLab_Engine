#include "PrimitivesManager.h"
#include "GameContext.h"
#include <algorithm>

PlanetData FPrimitivesManager::planetDataList[] = {
	{"Venus", 1.949f},
	{"Uranus", 2.331f},
	{"Mercury", 2.483f},
	{"Pluto", 3.145f},
	{"Neptune", 5.883f},
	{"Mars", 3.832f},
	{"Jupiter", 6.21f},
};

std::vector<std::vector<StageGroup>> FPrimitivesManager::stageGroupData = {
	{
		{"Mercury", 7},
		{"Mars", 8},
		{"Venus", 7},
		{"Pluto", 6},
		{"Neptune", 7},
	},
	{
		{"Uranus", 12},
		{"Mercury", 15},
		{"Pluto", 10},
		{"Neptune", 5},
	},
	{
		{"Uranus", 12},
		{"Mercury", 15},
		{"Pluto", 10},
		{"Neptune", 5},
		{"Mars", 5},
	},
	{
		{"Uranus", 12},
		{"Mercury", 15},
		{"Pluto", 10},
		{"Neptune", 5},
		{"Jupiter", 5},
	},
	{
		{"Venus", 12},
		{"Uranus", 12},
		{"Mercury", 15},
		{"Pluto", 10},
		{"Neptune", 5},
		{"Jupiter", 5},
		{"Mars", 5},
	},
};


FPrimitivesManager::~FPrimitivesManager()
{
	Reset();
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
	if (index >= 0 && index < (int)objects.size())
	{
		return objects[index];
	}
	return nullptr;
}

void FPrimitivesManager::InitializeGameObjects()
{
	// Player 생성
	player = new UBall();
	player->Location = { 0.0f, -0.95f, 0.0f };
	player->Radius = 0.05f;
	player->TextureName = "Earth";
	player->brightness = 1.0f;
	AddObject(player);

	// Moon 생성
	moon = new Moon({ 0.0f, -1000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, player->Radius * 0.7f, player, "Moon");
	moon->brightness = 1.0f;
	AddObject(moon);

	// 재사용할 Meteor 2개 생성
	float meteorRadius = 0.05f * 0.883f; // baseRadius * meteorRelativeRadius
	for (int i = 0; i < 2; ++i)
	{
		meteors[i] = new Meteor(player, player->Radius * 1.7f, "Meteor");
		// 겹치지 않게 초기 생성 시간에 차이를 둡니다.
		meteors[i]->waitTimer = i * 2.5f;
		AddObject(meteors[i]);
	}

	GravityPlanet::SetGravitySystem(player);

	// Camera 생성
	camera = new Camera();

	// Background 생성
	background = new Image("Background");

	highestPlayerY = 0.0f;

	SpawnPlanetsForAllStages();
}

//void FPrimitivesManager::SpawnRandomPlanet()
//{
//	const float baseRadius = 0.05f;
//	const float minSpeed = 0.01f;
//	const float maxSpeed = 0.03f;
//
//	int lastIndex = (sizeof(planetDataList) / sizeof(planetDataList[0]));
//	int randIndex = rand() % lastIndex;
//	float radius = baseRadius * planetDataList[randIndex].relativeRadius;
//
//	FVector3 newPos;
//	bool bPositionValid = false;
//	const int maxAttempts = 100;
//
//	for (int attempt = 0; attempt < maxAttempts; ++attempt)
//	{
//		float newX = ((rand() % 1000) / 1000.0f) * 2.0f - 1.0f;
//		float offsetY = ((rand() % 1000) / 1000.0f) * 0.5f;
//		float newY = spawnBaseY + 1.5f + offsetY;
//		newPos = FVector3(newX, newY, 0.0f);
//
//		bPositionValid = true;
//
//		for (UPrimitive* obj : objects)
//		{
//			if (obj == nullptr) continue;
//			UBall* bobj = dynamic_cast<UBall*>(obj);
//			if (!bobj) continue;
//
//			float dx = newPos.x - bobj->Location.x;
//			float dy = newPos.y - bobj->Location.y;
//			float distance = sqrtf(dx * dx + dy * dy);
//
//			float minSpacing = (radius + bobj->Radius) * 1.5f;
//
//			if (distance < minSpacing)
//			{
//				bPositionValid = false;
//				break;
//			}
//		}
//		if (bPositionValid) break;
//	}
//
//	float randomAngle = (rand() % 360) * (FVector3::PI / 180.0f);
//	float randomSpeed = minSpeed + ((rand() % 1000) / 1000.0f) * (maxSpeed - minSpeed);
//	float sizeMultiplier = 0.05f / sqrtf(radius);
//	randomSpeed *= sizeMultiplier;
//
//	FVector3 randomVelocity;
//	std::string planetName = planetDataList[randIndex].name;
//
//	if (planetName == "Meteor")
//	{
//		randomVelocity.x = (((rand() % 100) / 100.0f) - 0.5f) * 0.02f;
//		randomVelocity.y = -(randomSpeed * 2.0f);
//		randomVelocity.z = 0.0f;
//	}
//	else
//	{
//		randomVelocity.x = cosf(randomAngle) * randomSpeed;
//		randomVelocity.y = sinf(randomAngle) * randomSpeed;
//		randomVelocity.z = 0.0f;
//	}
//
//	Planet* newPlanet = nullptr;
//	if (planetName == "Jupiter")
//	{
//		newPlanet = new GravityPlanet(newPos, randomVelocity, radius, planetName, PlanetType::pull);
//		newPlanet->brightness = 1.5f;
//	}
//	else if (planetName == "Mars")
//	{
//		newPlanet = new GravityPlanet(newPos, randomVelocity, radius, planetName, PlanetType::push);
//		newPlanet->brightness = 1.5f;
//	}
//	else
//	{
//		newPlanet = new Planet(newPos, randomVelocity, radius, planetName);
//		newPlanet->brightness = 0.7f + (rand() % 40) / 100.0f;
//	}
//
//	AddObject(newPlanet);
//}

void FPrimitivesManager::SpawnPlanetsForAllStages()
{
	for (int stageIndex = 0; stageIndex < 5; ++stageIndex)
	{
		SpawnPlanetsForStage(stageIndex);
	}
}

void FPrimitivesManager::SpawnPlanetsForStage(int stageIndex)
{
	if (stageIndex < 0 || stageIndex >= (int)stageGroupData.size())
	{
		return;
	}

	const float baseRadius = 0.05f;
	const float minHeight = GameContext::MinHeight;
	const float maxHeight = GameContext::MaxHeight - 2.0f;;
	const float heightRange = maxHeight - minHeight;
	const float stageHeight = heightRange / 5.0f;
	const float stageMinY = minHeight + (stageIndex * stageHeight);
	const float stageMaxY = minHeight + ((stageIndex + 1) * stageHeight);

	const std::vector<StageGroup>& currentStageGroups = stageGroupData[stageIndex];

	// 각 그룹의 행성들을 생성
	for (const StageGroup& group : currentStageGroups)
	{
		PlanetData* planetData = nullptr;
		int planetDataCount = sizeof(planetDataList) / sizeof(planetDataList[0]);
		for (int i = 0; i < planetDataCount; ++i)
		{
			if (planetDataList[i].name == group.planetName)
			{
				planetData = &planetDataList[i];
				break;
			}
		}

		if (planetData == nullptr)
		{
			continue;
		}
		
		float radius = baseRadius * planetData->relativeRadius;

		//해당 그룹의 행성 개수만큼 생성
		for (int i = 0; i < group.count; ++i)
		{
			FVector3 newPos;
			bool bPositionValid = false;
			const int maxAttempts = 1200;

			//겹치지 않는 위치 찾기
			for (int attempt = 0; attempt < maxAttempts; ++attempt)
			{
				float newX = ((rand() % 1000) / 1000.0f) * 2.0f - 1.0f;
				float expandedStageMinY = stageMinY - 0.5f;
				float expandedStageMaxY = stageMaxY + 8.5f;
				float newY = expandedStageMinY + ((rand() % 1000) / 1000.0f) * (expandedStageMaxY - expandedStageMinY);
				newPos = FVector3(newX, newY, 0.0f);

				bPositionValid = true;

				// 모든 기존 오브젝트와의 충돌 체크
				for (UPrimitive* obj : objects)
				{
					if (obj == nullptr) continue;
					UBall* bobj = dynamic_cast<UBall*>(obj);
					if (!bobj) continue;

					float dx = newPos.x - bobj->Location.x;
					float dy = newPos.y - bobj->Location.y;
					float distance = sqrtf(dx * dx + dy * dy);

					float minSpacing = (radius + bobj->Radius) * 2.0f;

					if (distance < minSpacing)
					{
						bPositionValid = false;
						break;
					}
				}

				if (bPositionValid) break;
			}

			// 행성 생성 (고정, velocity = 0)
			FVector3 zeroVelocity(0.0f, 0.0f, 0.0f);
			Planet* newPlanet = nullptr;

			if (planetData->name == "Jupiter")
			{
				newPlanet = new GravityPlanet(newPos, zeroVelocity, radius, planetData->name, PlanetType::pull);
				newPlanet->brightness = 1.5f;
			}
			else if (planetData->name == "Mars")
			{
				newPlanet = new GravityPlanet(newPos, zeroVelocity, radius, planetData->name, PlanetType::push);
				newPlanet->brightness = 1.5f;
			}
			else
			{
				newPlanet = new Planet(newPos, zeroVelocity, radius, planetData->name);
				newPlanet->brightness = 0.7f + (rand() % 40) / 100.0f;
			}

			AddObject(newPlanet);
		}
	}
}

void FPrimitivesManager::Reset()
{
	for (UPrimitive* obj : objects)
	{
		if (obj != nullptr)
		{
			delete obj;
		}
	}
	objects.clear();

	if (camera) delete camera;
	if (background) delete background;

	player = nullptr;
	moon = nullptr;
	camera = nullptr;
	meteors[0] = nullptr;
	meteors[1] = nullptr;
	background = nullptr;
}

void FPrimitivesManager::Update(const float deltaTime, const FVector3& ExternalForcePos)
{
	if (isRunning == false)
	{
		return;
	}

	// 오브젝트 업데이트
	for (UPrimitive* obj : objects)
	{
		if (obj != nullptr)
		{
			obj->Update(deltaTime);
			obj->ApplyAttraction(ExternalForcePos, 0.000001f);
		}
	}

	// 충돌 체크
	if (objects.size() >= 1)
	{
		for (size_t i = 1; i < objects.size(); ++i)
		{
			objects[i]->HandleCollision(objects[0]); // Player
		}
	}
	if (objects.size() >= 2)
	{
		for (size_t i = 2; i < objects.size(); ++i)
		{
			objects[i]->HandleCollision(objects[1]); // Moon
		}
	}

	// 카메라 업데이트
	if (camera && player)
	{
		camera->Update(deltaTime, player);
	}
}

void FPrimitivesManager::Render(URenderer& renderer)
{
	if (background)
	{
		background->Render(renderer);
	}

	for (UPrimitive* obj : objects)
	{
		if (obj != nullptr)
		{
			obj->Render(renderer);
		}
	}
}

void FPrimitivesManager::OnGameStateChanged(EGameState newState)
{
	switch (newState)
	{
	case EGameState::Running:
		
		if (!isRunning)
		{ 
			Reset();
			InitializeGameObjects();
		}

		isRunning = true;
		break;

	case EGameState::Title:
		isRunning = false;
		break;
		
	case EGameState::Ending:
		isRunning = false;
		break;
	}
}
