---@class TankScript : ScriptComponent
local Script = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0 },
        RotateSpeed = { type = "float", default = 1.0 },
        BaseFriction = { type = "float", default = 0.1 },
        DriftFactor = { type = "float", default = 0.3 },
        DriftSmoothness = { type = "float", default = 0.1 }
    }
}
local Query = require("Query")
local Targeting = require("AI.TargetingAI")
local WeaponInventory = require("WeaponInventory")
local LevelSystem = require("LevelSystem")
local Vec = require("Core.Vector")

function Script:BeginPlay()
    Log("TankScript BeginPlay")

    Log("SearchHeadMainGun")
    self.HeadMainGun = self.GetComponentByName("UStaticMeshComponent", "HeadMainGun")
    if self.HeadMainGun == nil or not self.HeadMainGun:IsValid() then
        Log("Invalid HeadMainGun")
    else
        Log("StarCoroutineSearch")
        self.Search = self.StartCoroutine(function()
            Query.SearchActorClosestByTagCoroutine(self, self.HeadMainGun, "Enemy")
        end)

        Log("StarCoroutineTargeting")
        self.Targeting = self.StartCoroutine(function()
            Targeting.TargetingCoroutine(self, self.HeadMainGun, true)
        end)
    end

    self.WeaponInventory = WeaponInventory.New(self)
    self.LevelSystem = LevelSystem.New(self, self.WeaponInventory)

    self.WeaponInventory:AddWeapon("MainCannon")

    -- Test only. In actual gameplay, enemy kills should call AddExp.
    self.LevelSystem:AddExp(30)

    self.Velocity = Vec.Zero()
end

function Script:AddExp(amount)
    if self.LevelSystem ~= nil then
        self.LevelSystem:AddExp(amount)
    end
end

function Script:OpenChest()
    if self.WeaponInventory ~= nil then
        return self.WeaponInventory:OpenChest()
    end

    return false
end

function Script:Tick(deltaTime)
    local actor = self:GetActor()
    if actor == nil or not actor:IsValid() then
        return
    end

    local InputH = Input.GetAxis("Horizontal")
    local InputV = Input.GetAxis("Vertical")
    local bDriftButton = Input.GetKey(0x10) -- VK_SHIFT (보통 Shift 키)

    -- 1. 전진/후진에 따른 회전 방향 처리
    local RotateSpeedSign = 1.0
    if InputV < 0.0 then
        RotateSpeedSign = -1.0
    end

    -- 2. 드리프트 상태에 따른 보간 계수 설정
    local DriftSmoothness = 0.8
    if bDriftButton then
        DriftSmoothness = self.DriftSmoothness or 0.1
    end

    -- 3. 드리프트 중 조향 민감도 보정
    local CurrentRotateSpeed = self.RotateSpeed or 1.0
    if bDriftButton then
        CurrentRotateSpeed = CurrentRotateSpeed * 1.5
    end

    -- 4. 궤도 속도 기반 전진 및 회전 속도 계산 (탱크 조향 방식)
    local TrackWidth = 0.8
    local LeftTrackSpeed = (self.MoveSpeed or 1.0) * InputV + CurrentRotateSpeed * InputH * RotateSpeedSign
    local RightTrackSpeed = (self.MoveSpeed or 1.0) * InputV - CurrentRotateSpeed * InputH * RotateSpeedSign

    local TargetForwardSpeed = (LeftTrackSpeed + RightTrackSpeed) * 0.5
    local TargetAngularSpeed = (LeftTrackSpeed - RightTrackSpeed) / TrackWidth

    -- 5. 관성 및 드리프트 처리를 위한 속도 계산
    local Forward = actor:GetForward()
    local IntendedVelocity = Vec.Mul(Forward, TargetForwardSpeed)

    if self.Velocity == nil then 
        self.Velocity = Vec.Zero() 
    end
    self.Velocity = Vec.Lerp(self.Velocity, IntendedVelocity, DriftSmoothness * deltaTime * 10.0)

    -- 6. 위치 및 회전 반영
    local Location = actor:GetLocation()
    local NewLocation = Vec.Add(Location, Vec.Mul(self.Velocity, deltaTime))
    actor:SetLocation(NewLocation)

    local Rotation = actor:GetRotation()
    Rotation.z = Rotation.z + (TargetAngularSpeed * deltaTime)
    actor:SetRotation(Rotation)
end

function Script:EndPlay()
    if self.WeaponInventory ~= nil then
        for _, weapon in pairs(self.WeaponInventory.Weapons) do
            if weapon.Stop ~= nil then
                weapon:Stop()
            end
        end
    end

    self.StopAllCoroutines()
end

return Script
