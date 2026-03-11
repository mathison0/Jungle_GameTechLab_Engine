#include "PrimitivesManager.h"
#include <algorithm>

PlanetData FPrimitivesManager::planetDataList[] = {
	{"Mercury", 1.383f},
	{"Venus", 1.949f},
	{"Mars", 1.532f},
	{"Jupiter", 7.21f},
	{"Neptune", 4.883f},
	{"Uranus", 2.331f},
	{"Pluto", 2.745f},
	{"Meteor", 0.883f},
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
	player->Location = { 0.0f, -1.0f, 0.0f };
	player->Radius = 0.05f;
	player->TextureName = "Earth";
	player->brightness = 1.0f;
	AddObject(player);

	// Moon 생성
	moon = new Moon({ 0.0f, -1000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, player->Radius * 0.7f, player, "Moon");
	moon->brightness = 1.0f;
	AddObject(moon);

	// Camera 생성
	camera = new Camera();

	// Background 생성
	background = new Sprite("Background");

	highestPlayerY = 0.0f;
	nextSpawnY = 1.0f;
}

void FPrimitivesManager::SpawnRandomPlanet(float spawnBaseY)
{
	const float baseRadius = 0.05f;
	const float minSpeed = 0.01f;
	const float maxSpeed = 0.03f;

	int randIndex = rand() % 8; // planetDataList size
	float radius = baseRadius * planetDataList[randIndex].relativeRadius;

	FVector3 newPos;
	bool bPositionValid = false;
	const int maxAttempts = 50;

	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		float newX = ((rand() % 1000) / 1000.0f) * 2.0f - 1.0f;
		float offsetY = ((rand() % 1000) / 1000.0f) * 0.5f;
		float newY = spawnBaseY + 1.5f + offsetY;
		newPos = FVector3(newX, newY, 0.0f);

		bPositionValid = true;

		for (UPrimitive* obj : objects)
		{
			if (obj == nullptr) continue;
			UBall* bobj = dynamic_cast<UBall*>(obj);
			if (!bobj) continue;

			float dx = newPos.x - bobj->Location.x;
			float dy = newPos.y - bobj->Location.y;
			float distance = sqrtf(dx * dx + dy * dy);

			float minSpacing = (radius + bobj->Radius) * 1.5f;

			if (distance < minSpacing)
			{
				bPositionValid = false;
				break;
			}
		}
		if (bPositionValid) break;
	}

	float randomAngle = (rand() % 360) * (FVector3::PI / 180.0f);
	float randomSpeed = minSpeed + ((rand() % 1000) / 1000.0f) * (maxSpeed - minSpeed);
	float sizeMultiplier = 0.05f / sqrtf(radius);
	randomSpeed *= sizeMultiplier;

	FVector3 randomVelocity;
	std::string planetName = planetDataList[randIndex].name;

	if (planetName == "Meteor")
	{
		randomVelocity.x = (((rand() % 100) / 100.0f) - 0.5f) * 0.02f;
		randomVelocity.y = -(randomSpeed * 2.0f);
		randomVelocity.z = 0.0f;
	}
	else
	{
		randomVelocity.x = cosf(randomAngle) * randomSpeed;
		randomVelocity.y = sinf(randomAngle) * randomSpeed;
		randomVelocity.z = 0.0f;
	}

	Planet* newPlanet = nullptr;
	if (planetName == "Meteor")
	{
		newPlanet = new Meteor(newPos, randomVelocity, radius, planetName);
	}
	else if (planetName == "Jupiter")
	{
		newPlanet = new GravityPlanet(newPos, randomVelocity, radius, planetName, PlanetType::pull);
		newPlanet->brightness = 1.5f;
	}
	else if (planetName == "Mars")
	{
		newPlanet = new GravityPlanet(newPos, randomVelocity, radius, planetName, PlanetType::push);
		newPlanet->brightness = 1.5f;
	}
	else
	{
		newPlanet = new Planet(newPos, randomVelocity, radius, planetName);
		newPlanet->brightness = 0.7f + (rand() % 40) / 100.0f;
	}

	AddObject(newPlanet);
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


	// 행성 생성 로직
	if (player != nullptr)
	{
		highestPlayerY = std::max<float>(highestPlayerY, player->Location.y);
		while (highestPlayerY > nextSpawnY)
		{
			int spawnCount = (rand() % 3) + 1;
			for (int i = 0; i < spawnCount; ++i)
			{
				SpawnRandomPlanet(nextSpawnY);
			}
			nextSpawnY += spawnInterval;
		}
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
