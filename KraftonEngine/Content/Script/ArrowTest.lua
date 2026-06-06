local arrowParticle = nil

local ARROW_FORWARD_SPEED = 850.0
local ARROW_UP_SPEED = 360.0
local ARROW_COUNT = 5
local SPREAD_STEP = 0.08
local MUZZLE_FORWARD_OFFSET = 0.0
local MUZZLE_UP_OFFSET = 0.0
local SPREAD_KEY = string.byte("B")

local function cache_arrow_particle()
    if arrowParticle == nil then
        arrowParticle = obj:GetParticleSystem()
    end

    return arrowParticle
end

local function get_fire_basis()
    local forward = obj.Forward:Normalized()
    local right = obj.Right:Normalized()
    local up = Vector.Up()
    local location = obj.Location + forward * MUZZLE_FORWARD_OFFSET + up * MUZZLE_UP_OFFSET

    return location, forward, right, up
end

local function make_arc_velocity(direction, up, upScale)
    local forwardVelocity = direction:Normalized() * ARROW_FORWARD_SPEED
    local upwardVelocity = up * (ARROW_UP_SPEED * upScale)

    return forwardVelocity + upwardVelocity
end

local function fire_arrow(location, velocity)
    local particle = cache_arrow_particle()
    if particle == nil then
        print("[ArrowTest] ParticleSystemComponent not found")
        return
    end

    particle:Activate()
    particle:SetEmitterSpawningEnabled(false)
    particle:EmitBurst(location, velocity)
end

local function fire_arrow_spread(count)
    local particle = cache_arrow_particle()
    if particle == nil then
        print("[ArrowTest] ParticleSystemComponent not found")
        return
    end

    local location, forward, right, up = get_fire_basis()
    local center = (count - 1) * 0.5
    local spawns = {}

    for i = 1, count do
        local offset = i - 1 - center
        local direction = (forward + right * (offset * SPREAD_STEP)):Normalized()
        local upScale = 1.0 + math.abs(offset) * 0.04

        spawns[i] = {
            Location = location + right * (offset * 12.0),
            Velocity = make_arc_velocity(direction, up, upScale)
        }
    end

    particle:Activate()
    particle:SetEmitterSpawningEnabled(false)
    local spawned = particle:EmitBurst(spawns)
    print("[ArrowTest] spawned arrows: " .. spawned)
end

function BeginPlay()
    local particle = cache_arrow_particle()
    if particle ~= nil then
        particle:SetEmitterSpawningEnabled(false)
        particle:Activate()
    end

    print("[ArrowTest] Space: single arrow, B: spread arrows")
end

function EndPlay()
    local particle = cache_arrow_particle()
    if particle ~= nil then
        particle:SetEmitterSpawningEnabled(false)
    end
end

function Tick(dt)
    if Input.GetKeyDown(Key.Space) then
        local location, forward, right, up = get_fire_basis()
        fire_arrow(location, make_arc_velocity(forward, up, 1.0))
    end

    if Input.GetKeyDown(SPREAD_KEY) then
        fire_arrow_spread(ARROW_COUNT)
    end
end
