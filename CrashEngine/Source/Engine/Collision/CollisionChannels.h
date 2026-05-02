#pragma once

#include "Core/CoreTypes.h"

enum class ECollisionChannel : uint8
{
	//당장 쓰는거만 두겠습니다
    WorldStatic,
    WorldDynamic,
    //Pawn,
    Projectile,
    Enemy,
    Player,
    //EnemyAttack,
    Trigger,
    Pickup,
    //Camera,
    Count
};

enum class ECollisionResponse : uint8
{
    Ignore,
    Overlap,
    Block
};