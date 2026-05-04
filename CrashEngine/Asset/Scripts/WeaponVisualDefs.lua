local WeaponVisualDefs = {}

WeaponVisualDefs.MainCannon = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_MainCannon_0",
                Mesh = "Models/Tank/Tank_head.obj",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.4, 0.4, 0.4 },
                MuzzleName = "Muzzle_MainCannon_0",
                MuzzleLocation = { 1.5, 0.0, 0.0 },
            },
        },
    },

    [2] = {
        Visuals = {
            {
                Name = "Visual_MainCannon_0",
                Mesh = "Models/Tank/Tank_head.obj",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.45, 0.45, 0.45 },
                MuzzleName = "Muzzle_MainCannon_0",
                MuzzleLocation = { 1.7, 0.0, 0.0 },
            },
        },
    },

    [3] = {
        Visuals = {
            {
                Name = "Visual_MainCannon_0",
                Mesh = "Models/Tank/Tank_head.obj",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.5, 0.5, 0.5 },
                MuzzleName = "Muzzle_MainCannon_0",
                MuzzleLocation = { 2.0, 0.0, 0.0 },
            },
        },
    },
}

WeaponVisualDefs.MachineTurret = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_MachineTurret_0",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.5, 0.3, 0.3 },
                MuzzleName = "Muzzle_MachineTurret_0",
                MuzzleLocation = { 0.8, 0.0, 0.0 },
            },
        },
    },

    [2] = {
        Visuals = {
            {
                Name = "Visual_MachineTurret_0",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 2.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.5, 0.3, 0.3 },
                MuzzleName = "Muzzle_MachineTurret_0",
                MuzzleLocation = { 0.8, 0.0, 0.0 },
            },
            {
                Name = "Visual_MachineTurret_1",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, -2.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.5, 0.3, 0.3 },
                MuzzleName = "Muzzle_MachineTurret_1",
                MuzzleLocation = { 0.8, 0.0, 0.0 },
            },
        },
    },

    [3] = {
        Visuals = {
            {
                Name = "Visual_MachineTurret_0",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { 2.0, 2.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.35, 0.35 },
                MuzzleName = "Muzzle_MachineTurret_0",
                MuzzleLocation = { 0.9, 0.0, 0.0 },
            },
            {
                Name = "Visual_MachineTurret_1",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { -2.0, 2.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.35, 0.35 },
                MuzzleName = "Muzzle_MachineTurret_1",
                MuzzleLocation = { 0.9, 0.0, 0.0 },
            },
            {
                Name = "Visual_MachineTurret_2",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { 2.0, -2.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.35, 0.35 },
                MuzzleName = "Muzzle_MachineTurret_2",
                MuzzleLocation = { 0.9, 0.0, 0.0 },
            },
            {
                Name = "Visual_MachineTurret_3",
                Mesh = "Models/Drone/Drone.OBJ",
                Parent = "RootComponent",
                Location = { -2.0, -2.0, 2.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.35, 0.35 },
                MuzzleName = "Muzzle_MachineTurret_3",
                MuzzleLocation = { 0.9, 0.0, 0.0 },
            },
        },
    },
}

WeaponVisualDefs.VehicleRush = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_VehicleRush_0",
                Mesh = "Models/Roadroller/roadroller.obj",
                Parent = "World",
                Location = { 0.0, 0.0, -1000.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 5.5, 2.4, 0.8 },
            },
        },
    },

    [2] = {
        Visuals = {
            {
                Name = "Visual_VehicleRush_0",
                Mesh = "Models/Roadroller/roadroller.obj",
                Parent = "World",
                Location = { 0.0, 0.0, -1000.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 5.0, 5.0, 5.0 },
            },
            {
                Name = "Visual_VehicleRush_1",
                Mesh = "Models/Roadroller/roadroller.obj",
                Parent = "World",
                Location = { 0.0, 0.0, -1000.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 5.0, 5.0, 5.0 },
            },
            {
                Name = "Visual_VehicleRush_2",
                Mesh = "Models/Roadroller/roadroller.obj",
                Parent = "World",
                Location = { 0.0, 0.0, -1000.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 5.0, 5.0, 5.0 },
            },
        },
    },

    [3] = {
        Visuals = {
            {
                Name = "Visual_VehicleRush_0",
                Mesh = "Models/Thomas/thomas.obj",
                Parent = "World",
                Location = { 0.0, 0.0, -2000.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 1.0, 1.0, 1.0 },
            },
        },
    },
}

WeaponVisualDefs.Aura = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_Aura_0",
                Mesh = "Models/_Basic/Sphere.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 0.1 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 3.0, 3.0, 3.0 },
            },
        },
    },

    [2] = {
        Visuals = {
            {
                Name = "Visual_Aura_0",
                Mesh = "Models/_Basic/Sphere.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 0.1 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 4.0, 4.0, 4.0 },
            },
        },
    },

    [3] = {
        Visuals = {
            {
                Name = "Visual_Aura_0",
                Mesh = "Models/_Basic/Sphere.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 0.1 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 5.5, 5.5, 5.5 },
            },
        },
    },
}

WeaponVisualDefs.HomingMissile = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_HomingMissile_0",
                Mesh = "Models/_Basic/Cube.OBJ",
                Parent = "RootComponent",
                Location = { -0.2, 0.9, 0.8 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.25, 0.25 },
                MuzzleName = "Muzzle_HomingMissile_0",
                MuzzleLocation = { 0.8, 0.0, 0.0 },
            },
        },
    },

    [2] = {
        Visuals = {
            {
                Name = "Visual_HomingMissile_0",
                Mesh = "Models/_Basic/Cube.OBJ",
                Parent = "RootComponent",
                Location = { -0.2, 0.9, 0.8 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.7, 0.3, 0.3 },
                MuzzleName = "Muzzle_HomingMissile_0",
                MuzzleLocation = { 0.9, 0.0, 0.0 },
            },
        },
    },

    [3] = {
        Visuals = {
            {
                Name = "Visual_HomingMissile_0",
                Mesh = "Models/_Basic/Cube.OBJ",
                Parent = "RootComponent",
                Location = { -0.2, 0.9, 0.8 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.8, 0.35, 0.35 },
                MuzzleName = "Muzzle_HomingMissile_0",
                MuzzleLocation = { 1.0, 0.0, 0.0 },
            },
        },
    },
}

WeaponVisualDefs.SatelliteBeam = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_SatelliteBeam_0",
                Mesh = "Models/_Basic/Sphere.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.2 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.4, 0.4, 0.4 },
            },
        },
    },

    [2] = {
        Visuals = {
            {
                Name = "Visual_SatelliteBeam_0",
                Mesh = "Models/_Basic/Sphere.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.25 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.5, 0.5, 0.5 },
            },
        },
    },

    [3] = {
        Visuals = {
            {
                Name = "Visual_SatelliteBeam_0",
                Mesh = "Models/_Basic/Sphere.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.3 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.6, 0.6 },
            },
        },
    },
}

return WeaponVisualDefs
