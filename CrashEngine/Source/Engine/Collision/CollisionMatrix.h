#pragma once
#include "CollisionChannels.h"

class FCollisionMatrix
{
public:
    static ECollisionResponse GetResponse(ECollisionChannel A, ECollisionChannel B)
    {
        const int32 Row = static_cast<int32>(A);
        const int32 Col = static_cast<int32>(B);

        if (Row < 0 || Row >= static_cast<int32>(ECollisionChannel::Count) ||
            Col < 0 || Col >= static_cast<int32>(ECollisionChannel::Count))
        {
            return ECollisionResponse::Ignore;
        }

        return Table[Row][Col];
    }

private:
    static constexpr ECollisionResponse Table
        [(int)ECollisionChannel::Count]
        [(int)ECollisionChannel::Count] = {
            // WorldStatic 기준
            {
                ECollisionResponse::Block,  // WorldStatic
                ECollisionResponse::Block,  // WorldDynamic
                ECollisionResponse::Block,  // Projectile
                ECollisionResponse::Block,  // Enemy
                ECollisionResponse::Block,  // Player
                ECollisionResponse::Ignore, // Trigger
                ECollisionResponse::Ignore, // Pickup
                ECollisionResponse::Ignore  // FlyingEnemy
            },

            // WorldDynamic 기준
            {
                ECollisionResponse::Block,   // WorldStatic
                ECollisionResponse::Block,   // WorldDynamic
                ECollisionResponse::Block,   // Projectile
                ECollisionResponse::Block,   // Enemy
                ECollisionResponse::Block,   // Player
                ECollisionResponse::Overlap, // Trigger
                ECollisionResponse::Ignore,  // Pickup
                ECollisionResponse::Ignore   // FlyingEnemy
            },

            // Projectile 기준
            {
                ECollisionResponse::Block,   // WorldStatic
                ECollisionResponse::Block,   // WorldDynamic
                ECollisionResponse::Ignore,  // Projectile
                ECollisionResponse::Overlap, // Enemy
                ECollisionResponse::Ignore,  // Player
                ECollisionResponse::Ignore,  // Trigger
                ECollisionResponse::Ignore,  // Pickup
                ECollisionResponse::Ignore   // FlyingEnemy
            },

            // Enemy 기준
            {
                ECollisionResponse::Block,   // WorldStatic
                ECollisionResponse::Block,   // WorldDynamic
                ECollisionResponse::Overlap, // Projectile
                ECollisionResponse::Block,   // Enemy
                ECollisionResponse::Block,   // Player
                ECollisionResponse::Overlap, // Trigger
                ECollisionResponse::Ignore,  // Pickup
                ECollisionResponse::Ignore   // FlyingEnemy
            },

            // Player 기준
            {
                ECollisionResponse::Block,   // WorldStatic
                ECollisionResponse::Block,   // WorldDynamic
                ECollisionResponse::Ignore,  // Projectile
                ECollisionResponse::Block,   // Enemy
                ECollisionResponse::Ignore,  // Player
                ECollisionResponse::Overlap, // Trigger
                ECollisionResponse::Overlap, // Pickup
                ECollisionResponse::Overlap  // FlyingEnemy
            },

            // Trigger 기준
            {
                ECollisionResponse::Ignore,  // WorldStatic
                ECollisionResponse::Overlap, // WorldDynamic
                ECollisionResponse::Ignore,  // Projectile
                ECollisionResponse::Overlap, // Enemy
                ECollisionResponse::Overlap, // Player
                ECollisionResponse::Ignore,  // Trigger
                ECollisionResponse::Ignore,  // Pickup
                ECollisionResponse::Ignore   // FlyingEnemy
            },

            // Pickup 기준
            {
                ECollisionResponse::Ignore,  // WorldStatic
                ECollisionResponse::Ignore,  // WorldDynamic
                ECollisionResponse::Ignore,  // Projectile
                ECollisionResponse::Ignore,  // Enemy
                ECollisionResponse::Overlap, // Player
                ECollisionResponse::Ignore,  // Trigger
                ECollisionResponse::Ignore,  // Pickup
                ECollisionResponse::Ignore   // FlyingEnemy
            },

            // FlyingEnemy 기준
            {
                ECollisionResponse::Ignore,  // WorldStatic
                ECollisionResponse::Ignore,  // WorldDynamic
                ECollisionResponse::Ignore,  // Projectile
                ECollisionResponse::Ignore,  // Enemy
                ECollisionResponse::Overlap, // Player
                ECollisionResponse::Ignore,  // Trigger
                ECollisionResponse::Ignore,  // Pickup
                ECollisionResponse::Ignore   // FlyingEnemy
            }
        };
};