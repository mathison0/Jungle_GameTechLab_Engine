local ChaseAI = require("AI.ChaseAI")
local GameManager = require("GameManager")

local Script = {
    properties = ChaseAI.properties
}
function Script:BeginPlay()
    -- GameManager가 이번 세션에서 완전히 초기화되었는지 확인 후 플레이어 참조 가져오기
    if GameManager.Initialized and GameManager.PlayerScript and type(GameManager.PlayerScript.GetActor) == "function" then
        self.target = GameManager.PlayerScript:GetActor()
    end

    -- 만약 GameManager를 통해 못 찾았다면 직접 태그로 검색 (세션 초기화 지연 대응)
    if not self.target or not self.target:IsValid() then
        self.target = self:QueryActorByTagClosest("Player", {x=0, y=0, z=0}, 1000000.0)
    end

    self.AttackTimer = 0

    self.ai = self.StartCoroutine(function()
        ChaseAI.ChaseTargetCoroutine(self, "Player", self.GetRootComponent())
    end)
end

function Script:Tick(deltaTime)
    if self.AttackTimer > 0 then
        self.AttackTimer = self.AttackTimer - deltaTime
    end

    -- 연속 충돌(Overlap Stay) 처리: 이미 겹쳐있는 상태에서도 주기적으로 데미지 적용
    if self.AttackTimer <= 0 then
        if self.target and self.target:IsValid() then
            local root = self:GetRootComponent()
            if root and root:IsValid() and root:IsOverlappingActor(self.target) then
                self:ApplyAttack()
            end
        end
    end
end

function Script:ApplyAttack()
    GameManager.PlayerGetDamage(self.Damage or 10.0)
    self.AttackTimer = self.AttackInterval or 1.0
    Log("[EnemyScript] Player hit! Damage: " .. tostring(self.Damage or 10.0))
end

local function HandleAttack(self, other)
    if otherActor:GetTag() == "Player" then
        Log("Collision with player")
    end

    if self.AttackTimer > 0 then
        return
    end

    if not other or not other:IsValid() then return end

    local otherActor = other:GetOwner()
    if not otherActor or not otherActor:IsValid() then return end

    if otherActor:GetTag() == "Player" then
        self:ApplyAttack()
    end
end

function Script:OnOverlapBegin(other)
    HandleAttack(self, other)
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
