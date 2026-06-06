bEnabled = true
Team = EUnitTeam.Enemy
CombatType = EUnitCombatType.Melee
SpawnInterval = 2.0
SpawnCountPerWave = 5
MaxAliveUnits = 50
SpawnRadius = 3.0
InitialSpawnCount = 10
bDespawnOwnedUnitsOnDisable = false
bDespawnOwnedUnitsOnEndPlay = false
bDebugLog = false

local manager = nil
local ownedHandles = {}
local spawnTimer = 0.0
local bLastEnabled = bEnabled

local function ClampNonNegativeNumber(value)
    value = tonumber(value) or 0.0
    if value < 0.0 then
        return 0.0
    end
    return value
end

local function ClampNonNegativeInteger(value)
    return math.floor(ClampNonNegativeNumber(value))
end

local function GetSpawnCenter()
    if obj ~= nil then
        return obj.Location
    end

    return Vector.Zero()
end

local function MakeSpawnPosition()
    local center = GetSpawnCenter()
    local radius = ClampNonNegativeNumber(SpawnRadius)
    if radius <= 0.0 then
        return center
    end

    local angle = math.random() * math.pi * 2.0
    local distance = math.sqrt(math.random()) * radius
    return center + Vector.new(math.cos(angle) * distance, math.sin(angle) * distance, 0.0)
end

local function PruneOwnedHandles()
    if manager == nil then
        ownedHandles = {}
        return 0
    end

    local writeIndex = 1
    for readIndex = 1, #ownedHandles do
        local handle = ownedHandles[readIndex]
        if handle ~= nil and handle:IsValid() and manager:IsUnitAlive(handle) then
            ownedHandles[writeIndex] = handle
            writeIndex = writeIndex + 1
        end
    end

    for index = writeIndex, #ownedHandles do
        ownedHandles[index] = nil
    end

    return writeIndex - 1
end

local function DespawnOwnedUnits(reason)
    if manager == nil then
        ownedHandles = {}
        return
    end

    local despawned = 0
    for index = 1, #ownedHandles do
        local handle = ownedHandles[index]
        if handle ~= nil and handle:IsValid() and manager:IsUnitAlive(handle) then
            manager:DespawnUnit(handle)
            despawned = despawned + 1
        end
    end

    ownedHandles = {}

    if bDebugLog and despawned > 0 then
        print("[UnitSpawner] despawned=" .. despawned .. " reason=" .. reason)
    end
end

local function SpawnOneUnit()
    if manager == nil then
        return false
    end

    local handle = manager:SpawnUnit(Team, CombatType, MakeSpawnPosition())
    if handle == nil or not handle:IsValid() then
        return false
    end

    table.insert(ownedHandles, handle)
    return true
end

local function SpawnUpTo(requestedCount)
    if manager == nil then
        return 0
    end

    local aliveCount = PruneOwnedHandles()
    local maxAlive = ClampNonNegativeInteger(MaxAliveUnits)
    local remaining = maxAlive - aliveCount
    if remaining <= 0 then
        return 0
    end

    local count = math.min(ClampNonNegativeInteger(requestedCount), remaining)
    local spawned = 0
    for _ = 1, count do
        if SpawnOneUnit() then
            spawned = spawned + 1
        end
    end

    if bDebugLog and spawned > 0 then
        print("[UnitSpawner] spawned=" .. spawned .. " ownedAlive=" .. PruneOwnedHandles())
    end

    return spawned
end

local function SpawnWave()
    return SpawnUpTo(SpawnCountPerWave)
end

function BeginPlay()
    manager = Crowd.GetOrCreateManager(obj)
    ownedHandles = {}
    spawnTimer = 0.0
    bLastEnabled = bEnabled

    if manager == nil then
        if bDebugLog then
            print("[UnitSpawner] failed to get crowd manager")
        end
        return
    end

    SpawnUpTo(InitialSpawnCount)
end

function EndPlay()
    if bDespawnOwnedUnitsOnEndPlay then
        DespawnOwnedUnits("endplay")
    else
        ownedHandles = {}
    end

    manager = nil
end

function Tick(dt)
    if manager == nil then
        return
    end

    PruneOwnedHandles()

    if not bEnabled then
        if bLastEnabled and bDespawnOwnedUnitsOnDisable then
            DespawnOwnedUnits("disabled")
        end

        bLastEnabled = false
        spawnTimer = 0.0
        return
    end

    bLastEnabled = true

    local interval = ClampNonNegativeNumber(SpawnInterval)
    if interval <= 0.0 then
        SpawnWave()
        return
    end

    spawnTimer = spawnTimer + dt
    while spawnTimer >= interval do
        spawnTimer = spawnTimer - interval
        SpawnWave()

        if PruneOwnedHandles() >= ClampNonNegativeInteger(MaxAliveUnits) then
            break
        end
    end
end
