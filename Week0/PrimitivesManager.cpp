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
		{"Venus", 15},
		{"Uranus", 12},
		{"Mercury", 15},
		{"Pluto", 5},
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
		//좀 더 적은 영역에 배치하므로 개수를 일부 줄임.
		{"Venus", 10},
		{"Uranus", 10},
		{"Mercury", 13},
		{"Pluto", 8},
		{"Neptune", 4},
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
	moon = new Moon({ 0.0f, -1000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, player->Radius * 0.8f, player, "Moon");
	moon->brightness = 1.0f;
	AddObject(moon);

	// 재사용할 Meteor 2개 생성
	float meteorRadius = 0.05f * 0.883f;
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

	goalLine = new GoalLine("Checkerboard");

	highestPlayerY = 0.0f;

	SpawnPlanetsForAllStages();
}

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
	//최대 초기화 시도 횟수이며 겹치는 경우를 허용합니다. 행성의 개수가 많지 않기에 1000번 시도해도 큰 오버헤드가 아닙니다.
	const int maxAttempts = 1000;

	const float baseRadius = 0.05f;
	const float minHeight = GameContext::MinHeight;
	const float maxHeight = GameContext::MaxHeight - 2.0f;
	const float heightRange = maxHeight - minHeight;
	const float stageHeight = heightRange / 5.0f;
	const float stageMinY = minHeight + (stageIndex * stageHeight);
	const float stageMaxY = minHeight + ((stageIndex + 1) * stageHeight);

	const std::vector<StageGroup>& currentStageGroups = stageGroupData[stageIndex];

	// 각 그룹의 행성들을 가져와 차례대로 배치합니다.
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
		float expandedStageMinY = stageMinY - 0.5f;
		//마지막 스테이지에서는 y값을 확장하지 않습니다(치트 생성 후 제대로 반영되었는지 확인)
		float expandedStageMaxY = (stageIndex != (int)stageGroupData.size() -1) ? stageMaxY + 8.5f : stageMaxY;

		//해당 그룹의 행성 개수만큼 생성
		for (int i = 0; i < group.count; ++i)
		{
			FVector3 newPos;
			bool bPositionValid = false;
			int attempt;
			//겹치지 않는 위치 찾기
			for (attempt = 0; attempt < maxAttempts; ++attempt)
			{
				float newX = ((rand() % 1000) / 1000.0f) * 2.0f - 1.0f;
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
			if (attempt == maxAttempts) continue;

			FVector3 zeroVelocity(0.0f, 0.0f, 0.0f);
			Planet* newPlanet = nullptr;

			if (planetData->name == "Jupiter")
			{
				newPlanet = new GravityPlanet(newPos, zeroVelocity, radius, planetData->name, PlanetType::pull);
			}
			else if (planetData->name == "Mars")
			{
				newPlanet = new GravityPlanet(newPos, zeroVelocity, radius, planetData->name, PlanetType::push);
			}
			else
			{
				newPlanet = new Planet(newPos, zeroVelocity, radius, planetData->name);
			}
			newPlanet->brightness = 0.7f + (rand() % 40) / 100.0f;
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

	int lastIdx = (int)objects.size() - 1 ;
	for (int i = lastIdx; i >= 0; --i)
	{
		UPrimitive* obj = objects[i];
		if (obj != nullptr)
		{
			obj->Render(renderer);
		}
	}

	if (goalLine)
	{
		goalLine->Render(renderer);
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

void FPrimitivesManager::ApplyCheat(char key)
{
	if (player == nullptr || (GameContext::GetiNSTANCE().GetState() != EGameState::Running))
	{
		return;
	}


	switch (key)
	{
	case '1':
		player->Location.y = 20.0f;
		break;
	case '2':
		player->Location.y = 40.0f;
		break;
	case '3':
		player->Location.y = 60.0f;
		break;
	case '4':
		player->Location.y = 80.0f;
		break;
	case '5':
		player->Location.y = 98.0f;
		break;
	}
}
