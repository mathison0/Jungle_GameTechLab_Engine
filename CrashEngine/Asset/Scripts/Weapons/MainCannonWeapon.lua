local Co = require("LuaCoroutine")
local Audio = require("Core.Audio")
local WeaponDefs = require("WeaponDefs")

local MainCannonWeapon = {}
MainCannonWeapon.__index = MainCannonWeapon

local TARGET_TAG = "Enemy"
local DEFAULT_RANGE = 10000.0
local DEFAULT_SEARCH_INTERVAL = 0.2

function MainCannonWeapon.New(owner)
    local self = setmetatable({}, MainCannonWeapon)

    self.Owner = owner
    self.Id = "MainCannon"
    self.Level = 1
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]

    self.IsRunning = false

    self.Target = nil

    self.VisualComponent = nil
    self.MuzzleComponent = nil

    self.SearchCoroutine = nil
    self.TargetingCoroutine = nil
    self.FireCoroutine = nil

    return self
end

function MainCannonWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    self:RefreshComponents()

    Log("[MainCannon] Start")

    if self.Owner == nil or self.Owner.StartCoroutine == nil then
        Log("[MainCannon] StartCoroutine is nil")
        return
    end

    self.SearchCoroutine = self.Owner.StartCoroutine(function()
        self:SearchLoop()
    end)

    self.TargetingCoroutine = self.Owner.StartCoroutine(function()
        self:TargetingLoop()
    end)

    self.FireCoroutine = self.Owner.StartCoroutine(function()
        self:FireLoop()
    end)
end

function MainCannonWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
        if self.SearchCoroutine ~= nil then
            self.Owner.StopCoroutine(self.SearchCoroutine)
        end

        if self.TargetingCoroutine ~= nil then
            self.Owner.StopCoroutine(self.TargetingCoroutine)
        end

        if self.FireCoroutine ~= nil then
            self.Owner.StopCoroutine(self.FireCoroutine)
        end
    end

    self.SearchCoroutine = nil
    self.TargetingCoroutine = nil
    self.FireCoroutine = nil

    self.Target = nil
end

function MainCannonWeapon:RefreshComponents()
    if self.Owner == nil or self.Owner.GetComponentByName == nil then
        return
    end

    self.VisualComponent = self.Owner.GetComponentByName(
        "UStaticMeshComponent",
        "Visual_MainCannon_0"
    )

    self.MuzzleComponent = self.Owner.GetComponentByName(
        "USceneComponent",
        "Muzzle_MainCannon_0"
    )

    if self.VisualComponent == nil or not self.VisualComponent:IsValid() then
        Log("[MainCannon] Visual_MainCannon_0 not found")
    end

    if self.MuzzleComponent == nil or not self.MuzzleComponent:IsValid() then
        Log("[MainCannon] Muzzle_MainCannon_0 not found")
    end
end

function MainCannonWeapon:GetSearchOrigin()
    if self.MuzzleComponent ~= nil and self.MuzzleComponent:IsValid() then
        return self.MuzzleComponent:GetWorldLocation()
    end  

    if self.VisualComponent ~= nil and self.VisualComponent:IsValid() then
        return self.VisualComponent:GetWorldLocation()
    end

    local actor = nil
    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        actor = self.Owner.GetActor()
    end

    if actor ~= nil and actor:IsValid() then
        return actor:GetLocation()
    end

    return nil
end

function MainCannonWeapon:IsTargetValid()
    return self.Target ~= nil and self.Target:IsValid()
end

function MainCannonWeapon:SearchTarget()
    if self.Owner == nil or self.Owner.QueryActorByTagClosest == nil then
        return
    end

    local origin = self:GetSearchOrigin()
    if origin == nil then
        self:RefreshComponents()
        origin = self:GetSearchOrigin()
    end

    if origin == nil then
        return
    end

    local range = self.Data.Range or self.Data.TargetSearchRadius or DEFAULT_RANGE

    self.Target = self.Owner.QueryActorByTagClosest(
        TARGET_TAG,
        origin,
        range
    )
end

function MainCannonWeapon:SearchLoop()
    while self.IsRunning do
        self:SearchTarget()

        local interval =
            self.Data.TargetRefreshInterval or
            self.Data.SearchInterval or
            DEFAULT_SEARCH_INTERVAL

        Co.Wait(interval)
    end
end

function MainCannonWeapon:TargetingLoop()
    while self.IsRunning do
        Co.WaitNextFrame()

        if self.VisualComponent == nil or not self.VisualComponent:IsValid() then
            self:RefreshComponents()
        end

        if self.VisualComponent ~= nil and self.VisualComponent:IsValid() and self:IsTargetValid() then
            local myPos = self:GetSearchOrigin()
            local targetPos = self.Target:GetLocation()

            local z = myPos.z or myPos[3]
            if z ~= nil then
                targetPos[3] = z
                targetPos.z = z
            end

            self.VisualComponent:LookAt(targetPos)
        end
    end
end

function MainCannonWeapon:FireLoop()
    while self.IsRunning do
        if self:IsTargetValid() then
            if self.Owner ~= nil and self.Owner.FireLinearProjectile ~= nil then
                self:PlayFireSound()
                self.Owner.FireLinearProjectile("MainCannon", self.Data, 0)
            else
                Log("[MainCannon] FireLinearProjectile is nil")
            end
        end

        Co.Wait(self.Data.FireInterval)
    end
end

function MainCannonWeapon:PlayFireSound()
    local sound = self.Data.Sound
    if sound == nil then
        return
    end

    if type(sound) == "string" then
        Audio.Play(sound, Audio.Bus.SFX, 1.0)
        return
    end

    Audio.Play(
        sound.Fire or sound.Key,
        sound.Bus or Audio.Bus.SFX,
        sound.Volume or 1.0
    )
end

function MainCannonWeapon:Upgrade()
    if self.Level >= WeaponDefs.MainCannon.MaxNormalLevel then
        Log("MainCannon is max normal level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]

    -- WeaponInventory가 visual layout을 갱신한 뒤에도 이름은 같지만,
    -- 컴포넌트 재생성/숨김 가능성 때문에 다음 루프에서 다시 찾게 한다.
    self:RefreshComponents()

    Log(
        "MainCannon upgraded. level = " ..
        tostring(self.Level) ..
        ", interval = " ..
        tostring(self.Data.FireInterval)
    )

    return true
end

function MainCannonWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.MainCannon.MaxNormalLevel
end

function MainCannonWeapon:CanEvolve()
    return self.Level == WeaponDefs.MainCannon.MaxNormalLevel
end

function MainCannonWeapon:Evolve()
    if not self:CanEvolve() then
        return false
    end

    self.Level = 3
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]

    self:RefreshComponents()

    Log(
        "MainCannon evolved. level = " ..
        tostring(self.Level) ..
        ", interval = " ..
        tostring(self.Data.FireInterval)
    )

    return true
end

return MainCannonWeapon
