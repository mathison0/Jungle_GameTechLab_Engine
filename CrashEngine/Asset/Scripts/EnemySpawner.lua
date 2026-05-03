local Vec = require("Core.Vector")

---@class EnemySpawner : ScriptComponent
local Script = {
    properties = {
        EnemyClassName = { type = "string", default = "AEnemyBaseActor" },
        TargetTagName = { type = "string", default = "Player" }, -- 검색할 대상의 태그입니다.
        MinSpawnRadius = { type = "float", default = 30.0 },
        MaxSpawnRadius = { type = "float", default = 50.0 }
    }
}

-- 스폰 단계 설정 (C++의 SpawnTable과 동일)
local SpawnTable = {
    { TimeThreshold = 30.0, DesiredCount = 30, SpawnRate = 5.0 },
    { TimeThreshold = 60.0, DesiredCount = 60, SpawnRate = 10.0 },
    { TimeThreshold = 120.0, DesiredCount = 100, SpawnRate = 15.0 },
    { TimeThreshold = 1000000.0, DesiredCount = 150, SpawnRate = 20.0 }
}

function Script:BeginPlay()
    self.GameTime = 0.0
    self.SpawnBudget = 0.0
    self.PoolManager = GetActorPoolManager()
    
    -- 랜덤 시드 초기화
    math.randomseed(os.time())
    
    if self.PoolManager:IsValid() then
        Log("EnemySpawner: PoolManager initialized for " .. self.EnemyClassName)
        -- 빈번하게 생성될 적 액터들을 위해 풀 워밍업 (선택 사항)
        self.PoolManager:Warmup(self.EnemyClassName, 20)
    end
end

-- 현재 게임 시간에 맞는 스폰 페이즈 반환
function Script:GetPhase(Time)
    for _, phase in ipairs(SpawnTable) do
        if Time < phase.TimeThreshold then
            return phase
        end
    end
    return SpawnTable[#SpawnTable]
end

-- 플레이어 주변의 랜덤한 스폰 위치 계산
function Script:GetSpawnPosition(PlayerPos)
    local Angle = math.random() * math.pi * 2.0
    local Distance = self.MinSpawnRadius + (math.random() * (self.MaxSpawnRadius - self.MinSpawnRadius))
    
    return {
        X = PlayerPos.X + math.cos(Angle) * Distance,
        Y = PlayerPos.Y + math.sin(Angle) * Distance,
        Z = 0.0
    }
end

function Script:Tick(DeltaTime)
    if not self.PoolManager:IsValid() then return end

    self.GameTime = self.GameTime + DeltaTime
    
    local Phase = self:GetPhase(self.GameTime)
    local DesiredEnemyCount = Phase.DesiredCount
    local SpawnRate = Phase.SpawnRate
    
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
        local Owner = self:GetActor()
        if Owner:IsValid() then
            PlayerPos = Owner:GetLocation()
        end
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
end

function Script:EndPlay()
    Log("EnemySpawner EndPlay")
end

return Script
