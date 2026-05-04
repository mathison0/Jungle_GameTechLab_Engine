local WeaponDefs = {}

WeaponDefs.MainCannon = {
    DisplayName = "Main Cannon",
    AttackType = "LinearProjectile",
    MaxLevel = 3,
    MaxNormalLevel = 2,
    EvolutionRequiresChest = true,
    Levels = {
        [1] = {
            Damage = 50,
            FireInterval = 1.0,
            ProjectileSpeed = 150,
            Pierce = 1,
            ProjectileScale = 0.2,
            ColliderSize = 1.0,
            Sound = {
                Fire = "PlayerShot1",
                Bus = "SFX",
                Volume = 1.0,
            },
        },
        [2] = {
            Damage = 50,
            FireInterval = 0.7,
            ProjectileSpeed = 150,
            Pierce = 5,
            ProjectileScale = 0.4,
            ColliderSize = 1.0,
            Sound = {
                Fire = "PlayerShot2",
                Bus = "SFX",
                Volume = 1.0,
            },
        },
        [3] = {
            Damage = 50,
            FireInterval = 0.5,
            ProjectileSpeed = 150,
            Pierce = 999,
            ProjectileScale = 0.6,
            ColliderSize = 4.0,
            Sound = {
                Fire = "PlayerShot3",
                Bus = "SFX",
                Volume = 1.0,
            },
        },
    },
}

WeaponDefs.MachineTurret = {
    DisplayName = "Machine Turret",
    AttackType = "InstantTargetDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 10,
            FireInterval = 0.5,
            TurretCount = 1,
            RapidCount = 3,
            RapidInterval = 0.1,
            Range = 10.0,
            TargetRefreshInterval = 0.2,
        },
        [2] = {
            Damage = 10,
            FireInterval = 0.5,
            TurretCount = 2,
            RapidCount = 4,
            RapidInterval = 0.1,
            Range = 10.0,
            TargetRefreshInterval = 0.2,
        },
        [3] = {
            Damage = 22,
            FireInterval = 0.5,
            TurretCount = 4,
            RapidCount = 5,
            RapidInterval = 0.1,
            Range = 10.0,
            TargetRefreshInterval = 0.15,
        },
    },
}

WeaponDefs.VehicleRush = {
    DisplayName = "Vehicle Rush",
    AttackType = "MovingAreaDamage",
    MaxLevel = 3,
    MaxVehicleCount = 3,
    Levels = {
        [1] = {
            Damage = 9999,
            FireInterval = 5.5,
            VehicleSpeed = 36.0,
            SpawnDistance = 55.0,
            VehicleLength = 11.0,
            VehicleWidth = 4.8,
            VehicleCount = 1,
            LaneSpacing = 5.0,
            MinPassOffset = 8.0,
            MaxPassOffset = 18.0,
            Sound = {
                Spawn = "car_beep",
                Bus = "SFX",
                Volume = 1.0,
            },
        },
        [2] = {
            Damage = 9999,
            FireInterval = 5.0,
            VehicleSpeed = 40.0,
            SpawnDistance = 58.0,
            VehicleLength = 12.0,
            VehicleWidth = 5.2,
            VehicleCount = 3,
            LaneSpacing = 5.0,
            MinPassOffset = 10.0,
            MaxPassOffset = 20.0,
            Sound = {
                Spawn = "car_beep",
                Bus = "SFX",
                Volume = 1.0,
            },
        },
        [3] = {
            Damage = 9999,
            FireInterval = 5.0,
            VehicleSpeed = 32.0,
            SpawnDistance = 70.0,
            VehicleLength = 28.0,
            VehicleWidth = 3.0,
            VehicleCount = 1,
            LaneSpacing = 0.0,
            MinPassOffset = 9.0,
            MaxPassOffset = 22.0,
            Sound = {
                Spawn = "thomas",
                Bus = "SFX",
                Volume = 1.5,
                Resume = true,
                DurationMs = 30000.0,
            },
        },
    },
}

WeaponDefs.Aura = {
    DisplayName = "Aura",
    AttackType = "AreaTickDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 50,
            TickInterval = 0.5,
            Radius = 4.0,
            RotateSpeed = 90.0,
        },
        [2] = {
            Damage = 50,
            TickInterval = 0.4,
            Radius = 5.0,
            RotateSpeed = 120.0,
        },
        [3] = {
            Damage = 50,
            TickInterval = 0.3,
            Radius = 6.0,
            RotateSpeed = 180.0,
        },
    },
}

WeaponDefs.HomingMissile = {
    DisplayName = "Homing Missile",
    AttackType = "HomingMissile",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 30,
            FireInterval = 2.5,
            ProjectileSpeed = 12,
            TurnSpeed = 120,
            ImpactRadius = 3.0,
            ColliderSize = 0.8,
            ProjectileScale = 1.0,
            Range = 10000.0,
        },
        [2] = {
            Damage = 40,
            FireInterval = 2.2,
            ProjectileSpeed = 13,
            TurnSpeed = 140,
            ImpactRadius = 3.2,
            ColliderSize = 0.8,
            ProjectileScale = 1.0,
            Range = 10000.0,
        },
        [3] = {
            Damage = 55,
            FireInterval = 1.8,
            ProjectileSpeed = 15,
            TurnSpeed = 160,
            ImpactRadius = 3.5,
            ColliderSize = 0.9,
            ProjectileScale = 1.15,
            Range = 10000.0,
        },
    },
}

WeaponDefs.SatelliteBeam = {
    DisplayName = "Satellite Beam",
    AttackType = "InstantTargetDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 50,
            FireInterval = 4.0,
            Range = 10000.0,
            TargetCount = 1,
        },
        [2] = {
            Damage = 70,
            FireInterval = 3.5,
            Range = 10000.0,
            TargetCount = 1,
        },
        [3] = {
            Damage = 90,
            FireInterval = 3.0,
            Range = 10000.0,
            TargetCount = 1,
        },
    },
}

return WeaponDefs
