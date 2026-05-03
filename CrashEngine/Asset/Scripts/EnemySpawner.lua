local Vec = require("Core.Vector")

---@class EnemySpawner : ScriptComponent
local Script = {
    properties = {
        EnemyClassName = { type = "string", default = "AEnemyBaseActor" },
        TargetTagName = { type = "string", default = "Player" }, -- 검색할 대상의 태그입니다.
        MinSpawnRadius = { type = "float", default = 30.0 },
        MaxSpawnRadius = { type = "float", default = 50.0 },

        -- Flying EnemyWave
        FlyingEnemyClassName = { type = "string", default = "AFlyingWaveEnemyActor" },
        FlyingWaveInterval = { type = "float", default = 10.0 },
        FlyingWaveCount = { type = "int", default = 12 },
        FlyingWaveSpacing = { type = "float", default = 8.0 },
        FlyingWaveSpawnDistance = { type = "float", default = 80.0 },
        FlyingWaveSpeed = { type = "float", default = 60.0 },
        FlyingWaveLifeTime = { type = "float", default = 5.0 }
    }
}

-- 스폰 단계 설정 (C++의 SpawnTable과 동일)
local SpawnPhaseTable = {
    {
        TimeThreshold = 30.0, 
        Enemy = {DesiredCount = 30, SpawnRate = 5.0},
        FlyingWave = {Pattern = { 2, 3, 2 }, Interval = 15.0, Speed = 45.0}
    },

    {
        TimeThreshold = 60.0, 
        Enemy = {DesiredCount = 60, SpawnRate = 10.0}, FlyingWave = {Pattern = { 3, 4, 3 }, Interval = 12.0,  Speed = 55.0}
    },

    {
        TimeThreshold = 120.0,
        Enemy = {DesiredCount = 100, SpawnRate = 15.0}, FlyingWave = {Pattern = { 3, 4, 5, 4, 3 }, Interval = 10.0, Speed = 65.0}
    },

    {
        TimeThreshold = 1000000.0,
        Enemy = {DesiredCount = 150,SpawnRate = 20.0},
        FlyingWave = {Pattern = { 4, 5, 6, 5, 4 },Interval = 8.0, Speed = 75.0}
    }
}

function Script:BeginPlay()
    self.GameTime = 0.0
    self.SpawnBudget = 0.0
    self.FlyingWaveTimer = 0.0
    self.PoolManager = GetActorPoolManager()
    
    -- 랜덤 시드 초기화
math.randomseed(12345)
    
    if self.PoolManager:IsValid() then
        Log("EnemySpawner: PoolManager initialized for " .. self.EnemyClassName)
        -- 빈번하게 생성될 적 액터들을 위해 풀 워밍업 (선택 사항)
        self.PoolManager:Warmup(self.EnemyClassName, 20)
        self.PoolManager:Warmup(self.FlyingEnemyClassName, 20)
    end
end

-- 현재 게임 시간에 맞는 스폰 페이즈 반환
function Script:GetPhase(Time)
    for _, phase in ipairs(SpawnPhaseTable) do
        if Time < phase.TimeThreshold then
            return phase
        end
    end

    return SpawnPhaseTable[#SpawnPhaseTable]
end

-- 플레이어 주변의 랜덤한 스폰 위치 계산
function Script:GetSpawnPosition(PlayerPos)
    local Angle = math.random() * math.pi * 2.0
    local Distance = self.MinSpawnRadius + (math.random() * (self.MaxSpawnRadius - self.MinSpawnRadius))
    
    return {
        x = PlayerPos.x + math.cos(Angle) * Distance,
        y = PlayerPos.y + math.sin(Angle) * Distance,
        z = 0.0
    }
end

function Script:Tick(DeltaTime)
    if not self.PoolManager:IsValid() then return end

    self.GameTime = self.GameTime + DeltaTime
    self.FlyingWaveTimer = self.FlyingWaveTimer + DeltaTime

    local Phase = self:GetPhase(self.GameTime)
    local DesiredEnemyCount = Phase.Enemy.DesiredCount
    local SpawnRate = Phase.Enemy.SpawnRate
    
    -- 현재 활성화된 적 수 확인
    local CurrentEnemyCount = self.PoolManager:GetActiveCount(self.EnemyClassName)
    
    -- 스폰 예산 누적
    self.SpawnBudget = self.SpawnBudget + (SpawnRate * DeltaTime)
    
    -- 플레이어 위치 찾기 (태그 기반 검색)
    local PlayerActor = self:QueryActorByTagClosest(self.TargetTagName, {X=0, Y=0, Z=0}, 100000.0)
    local PlayerPos = {X = 0, Y = 0, Z = 0}
    
    if PlayerActor:IsValid() then
        PlayerPos = PlayerActor:GetLocation()
    else
        -- 플레이어를 못 찾으면 스포너 자신의 위치를 기준으로 삼음 (Fallback)
        --local Owner = self:GetActor()
        --if Owner:IsValid() then
        --    PlayerPos = Owner:GetLocation()
        --end

    end
    
    -- 버짓이 1.0 이상 쌓였고 목표 개수 미만이면 루프를 돌며 스폰
    while self.SpawnBudget >= 1.0 and CurrentEnemyCount < DesiredEnemyCount do
        local NewEnemy = self.PoolManager:Acquire(self.EnemyClassName)
        if NewEnemy:IsValid() then
            local SpawnPos = self:GetSpawnPosition(PlayerPos)
            NewEnemy:SetLocation(SpawnPos)
            
            CurrentEnemyCount = CurrentEnemyCount + 1
            self.SpawnBudget = self.SpawnBudget - 1.0
        else
            -- 풀에서 액터를 가져오지 못하면 중단
            break
        end
    end

    if self.FlyingWaveTimer >= Phase.FlyingWave.Interval then
        self.FlyingWaveTimer = 0.0
        self:SpawnFlyingWave(PlayerPos, Phase.FlyingWave)
    end
end

function Script:SpawnFlyingWave(PlayerPos, WaveData)
    local Angle = math.random() * math.pi * 2.0

    local Pattern = WaveData.Pattern
    local Speed = WaveData.Speed
    local Spacing = self.FlyingWaveSpacing

    local MoveDir = {
        x = math.cos(Angle),
        y = math.sin(Angle),
        z = 0.0
    }

    -- 진행 방향 기준 좌우 방향
    local Perp = {
        x = -MoveDir.y,
        y = MoveDir.x,
        z = 0.0
    }

    -- 플레이어 바깥쪽에서 생성
    local CenterSpawnPos = {
        x = PlayerPos.x - MoveDir.x * self.FlyingWaveSpawnDistance,
        y = PlayerPos.y - MoveDir.y * self.FlyingWaveSpawnDistance,
        z = PlayerPos.z
    }

    local RowSpacing = Spacing * 0.866 -- sqrt(3) / 2
    local Rows = #Pattern
    local HalfRows = (Rows - 1) * 0.5

    for row = 0, Rows - 1 do
        local Columns = Pattern[row + 1]
        local HalfCols = (Columns - 1) * 0.5

        -- 진행 방향 기준 앞뒤 배치
        local RowOffset = (row - HalfRows) * RowSpacing

        -- 벌집 배치를 위한 반 칸 밀기
        local Stagger = 0.0
        if row % 2 == 1 then
            Stagger = Spacing * 0.5
        end

        for col = 0, Columns - 1 do
            -- 진행 방향 기준 좌우 배치
            local ColOffset = (col - HalfCols) * Spacing + Stagger

            local SpawnPos = {
                x = CenterSpawnPos.x
                    + MoveDir.x * RowOffset
                    + Perp.x * ColOffset,

                y = CenterSpawnPos.y
                    + MoveDir.y * RowOffset
                    + Perp.y * ColOffset,

                z = CenterSpawnPos.z
            }

            local Enemy = self.PoolManager:Acquire(self.FlyingEnemyClassName)

            if Enemy:IsValid() then
                Enemy:SetLocation(SpawnPos)
                Enemy:InitFlyingWave(
                    MoveDir,
                    Speed,
                    self.FlyingWaveLifeTime
                )
            end
        end
    end
end

function Script:EndPlay()
    Log("EnemySpawner EndPlay")
end

return Script
