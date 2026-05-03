local DamageSystem = require("Core.DamageSystem")
local Co = require("LuaCoroutine")

local TestScript = {
    properties = {
        ExplosionRadius = 5.0,
        ExplosionDamage = 50
    }
}

function TestScript:BeginPlay()
    self.actor = self.GetActor()
    print("--- Damage System Test Started ---")
    
    -- 2초마다 주변 적에게 폭발 데미지를 입히는 코루틴 시작
    self.testRoutine = self.StartCoroutine(function()
        while true do
            Co.Wait(2.0)
            self:PerformTestExplosion()
        end
    end)
end

function TestScript:PerformTestExplosion()
    local world = self.GetWorld()
    local myPos = self.actor:GetLocation()
    
    print(string.format("\n[Test] Triggering Explosion at (%.2f, %.2f) with Radius %.2f", 
        myPos.X, myPos.Y, self.ExplosionRadius))
    
    -- 1. 먼저 OverlapCircle로 어떤 적들이 감지되는지 확인 (디버그용)
    local targets = world:OverlapCircle(myPos, self.ExplosionRadius, "Enemy")
    print("[Test] Detected " .. #targets .. " enemies in range.")
    
    for i, target in ipairs(targets) do
        print(string.format("  - Target[%d]: %s (UUID: %d)", i, target:GetName(), target:GetUUID()))
    end
    
    -- 2. DamageSystem을 통해 실제 데미지 적용
    -- 이 함수 내부에서 world:OverlapCircle을 다시 호출하여 데미지를 분배합니다.
    DamageSystem.ApplyRadialDamage(world, myPos, self.ExplosionRadius, self.ExplosionDamage, "Enemy", self.actor)
end

function TestScript:EndPlay()
    self.StopAllCoroutines()
end

return TestScript
