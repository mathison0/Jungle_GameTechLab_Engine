#include "planet.h"
#include <windows.h>

Planet::Planet(FVector3 L, float r)
{
	Location = L;
	radius = r;
	mass = r * r * 3.14;
}

bool Planet::Collision(UBall* other)
{

	return false;
}

void Planet::Boom(UBall* other)
{

}
