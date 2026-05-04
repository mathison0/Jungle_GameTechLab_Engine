---@class TankScript : ScriptComponent
local Script = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0 },
        RotateSpeed = { type = "float", default = 1.0 },
        BaseFriction = { type = "float", default = 0.1 },
        DriftFactor = { type = "float", default = 0.3 },
        DriftSmoothness = { type = "float", default = 0.1 },
        PickupExp = { type = "float", default = 1.0, min = 0.0, max = 1000.0, speed = 1.0 },
        PickupAttractSpeed = { type = "float", default = 20.0, min = 0.0, max = 1000.0, speed = 1.0 },
    }
}
local Vec = require("Core.Vector")
local GameManager = require("GameManager")

local KEY_SHIFT = 0x10

function Script:BeginPlay()
    self.Velocity = Vec.Zero()
    
    -- 1. 플레이어 태그 설정
    local actor = self:GetActor()
    if actor then
        actor:SetTag("Player")
    end

    -- 2. GameManager에 자신(Lua 스크립트 테이블)을 등록
    -- 이를 통해 GameManager는 플레이어의 Lua 메서드(EquipWeaponVisual 등)에 접근할 수 있게 됩니다.
    GameManager.RegisterPlayer(self)

    self.RootCollider = self.GetRootComponent()
    if self.RootCollider == nil or not self.RootCollider:IsValid() then
        Log("Invalid RootCollider")
    end

    self.PickupSensor = self.GetComponentByName("UCircleCollider2DComponent", "PickupSensor")
    if self.PickupSensor == nil or not self.PickupSensor:IsValid() then
        Log("Invalid PickupSensor")
    end
end

function Script:UpdateMovement(deltaTime)
    local actor = self.GetActor()
    if actor == nil or not actor:IsValid() then
        return
    end

    local inputH = Input.GetAxis("Horizontal")
    local inputV = Input.GetAxis("Vertical")
    local driftButton = Input.GetKey(KEY_SHIFT)

    local rotateSpeedSign = 1.0
    if inputV < 0.0 then
        rotateSpeedSign = -1.0
    end

    local driftSmoothness = 0.8
    if driftButton then
        driftSmoothness = self.DriftSmoothness or 0.1
    end

    local currentRotateSpeed = self.RotateSpeed or 1.0
    if driftButton then
        currentRotateSpeed = currentRotateSpeed * 1.5
    end

    local trackWidth = 0.8
    
    -- GameManager의 이동 속도 배율 적용
    local finalMoveSpeed = (self.MoveSpeed or 1.0) * (GameManager.Stats.MoveSpeedMult or 1.0)
    
    local leftTrackSpeed = finalMoveSpeed * inputV + currentRotateSpeed * inputH * rotateSpeedSign
    local rightTrackSpeed = finalMoveSpeed * inputV - currentRotateSpeed * inputH * rotateSpeedSign

    local targetForwardSpeed = (leftTrackSpeed + rightTrackSpeed) * 0.5
    local targetAngularSpeed = (leftTrackSpeed - rightTrackSpeed) / trackWidth

    local forward = actor:GetForward()
    local intendedVelocity = Vec.Mul(forward, targetForwardSpeed)

    if self.Velocity == nil then
        self.Velocity = Vec.Zero()
    end

    local lerpAlpha = math.min(1.0, driftSmoothness * deltaTime * 10.0)
    self.Velocity = Vec.Lerp(self.Velocity, intendedVelocity, lerpAlpha)

    local location = actor:GetLocation()
    local newLocation = Vec.Add(location, Vec.Mul(self.Velocity, deltaTime))
    actor:SetLocation(newLocation)

    local rotation = actor:GetRotation()
    rotation.z = rotation.z + targetAngularSpeed * deltaTime
    actor:SetRotation(rotation)
end

function Script:AddExp(amount)
    GameManager.OnPickupExp(amount)
end

function Script:OpenChest()
    return GameManager.OnPickupChest()
end

function Script:AttractPickup(pickup, deltaTime)
    if pickup == nil or not pickup:IsValid() then
        return
    end

    local owner = self.GetActor()
    if owner == nil or not owner:IsValid() then
        return
    end

    local pickupPos = pickup:GetLocation()
    local targetPos = owner:GetLocation()
    local toTarget = Vec.Sub(targetPos, pickupPos)
    local distance = Vec.Length(toTarget)
    if distance <= 0.0001 then
        return
    end

    -- 자석 속도와 범위 배율 적용 가능
    local attractSpeed = (self.PickupAttractSpeed or 20.0)
    local step = math.min(distance, attractSpeed * deltaTime)
    pickup:SetLocation(Vec.Add(pickupPos, Vec.Mul(toTarget, step / distance)))
end

function Script:CollectPickup(pickup, poolManager)
    -- 보석의 경우 경험치 지급 (추후 pickup 종류에 따라 분기 가능)
    self:AddExp(self.PickupExp or 1)

    if poolManager ~= nil and poolManager:IsValid() then
        poolManager:Release(pickup)
    end
    pickup:SetVisible(false)
end

function Script:UpdatePickups(deltaTime)
    if self.PickupSensor == nil or not self.PickupSensor:IsValid() then
        return
    end
    if self.RootCollider == nil or not self.RootCollider:IsValid() then
        return
    end

    local world = self.GetWorld()
    if world == nil or not world:IsValid() then
        return
    end

    local pickups = world:GetActorsByTag("Pickup")
    if pickups == nil then
        return
    end

    local poolManager = GetActorPoolManager()
    for _, pickup in ipairs(pickups) do
        if pickup:IsValid() and pickup:IsVisible() then
            if self.PickupSensor:IsOverlappingActor(pickup) then
                self:AttractPickup(pickup, deltaTime)
            end

            if self.RootCollider:IsOverlappingActor(pickup) then
                self:CollectPickup(pickup, poolManager)
            end
        end
    end
end

function Script:Tick(deltaTime)
    self:UpdateMovement(deltaTime)
    self:UpdatePickups(deltaTime)
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
