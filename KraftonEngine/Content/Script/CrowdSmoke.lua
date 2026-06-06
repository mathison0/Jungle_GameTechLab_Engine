local manager = nil
local elapsed = 0.0
local bPrintedInitialCount = false
local bAppliedRadialDamage = false
local bPrintedAfterDamage = false
local bStartedStressTest = false

local ENABLE_STRESS_TEST = false

local function PrintCounts(label)
    if manager == nil then
        print("[CrowdSmoke] " .. label .. " manager=nil")
        return
    end

    print("[CrowdSmoke] " .. label
        .. " alive=" .. manager:GetAliveCount()
        .. " ally=" .. manager:GetTeamAliveCount(EUnitTeam.Ally)
        .. " enemy=" .. manager:GetTeamAliveCount(EUnitTeam.Enemy)
        .. " allyMelee=" .. manager:GetTeamCombatTypeAliveCount(EUnitTeam.Ally, EUnitCombatType.Melee)
        .. " allyRanged=" .. manager:GetTeamCombatTypeAliveCount(EUnitTeam.Ally, EUnitCombatType.Ranged)
        .. " enemyMelee=" .. manager:GetTeamCombatTypeAliveCount(EUnitTeam.Enemy, EUnitCombatType.Melee)
        .. " enemyRanged=" .. manager:GetTeamCombatTypeAliveCount(EUnitTeam.Enemy, EUnitCombatType.Ranged)
        .. " render=" .. manager:GetRenderDataCount())
end

function BeginPlay()
    manager = Crowd.GetOrCreateManager(obj)
    if manager == nil then
        print("[CrowdSmoke] failed to get manager")
        return
    end

    manager:ClearUnits()
    manager:SetDebugDrawEnabled(true)

    manager:SpawnUnits(EUnitTeam.Ally, EUnitCombatType.Melee, Vector.new(-2.0, 10.0, 0.0), 50, 3.0)
    manager:SpawnUnits(EUnitTeam.Ally, EUnitCombatType.Ranged, Vector.new(2.0, 10.0, 0.0), 50, 3.0)
    manager:SpawnUnits(EUnitTeam.Enemy, EUnitCombatType.Melee, Vector.new(-2.0, -10.0, 0.0), 50, 3.0)
    manager:SpawnUnits(EUnitTeam.Enemy, EUnitCombatType.Ranged, Vector.new(2.0, -10.0, 0.0), 50, 3.0)

    print("[CrowdSmoke] requested 50 melee + 50 ranged per team")
end

function Tick(dt)
    if manager == nil then
        return
    end

    elapsed = elapsed + dt

    if elapsed > 1.0 and not bPrintedInitialCount then
        bPrintedInitialCount = true
        PrintCounts("initial")
    end

    --if elapsed > 4.0 and not bAppliedRadialDamage then
        bAppliedRadialDamage = true
     --   manager:ApplyRadialDamage(Vector.new(0.0, 0.0, 0.0), 6.0, 1000.0, EUnitTeam.Enemy)
     --   print("[CrowdSmoke] radial damage requested")
    --end

    if elapsed > 5.0 and not bPrintedAfterDamage then
        bPrintedAfterDamage = true
        PrintCounts("after radial damage")
    end

    if ENABLE_STRESS_TEST and elapsed > 8.0 and not bStartedStressTest then
        bStartedStressTest = true
        manager:ClearUnits()
        manager:SpawnUnits(EUnitTeam.Ally, EUnitCombatType.Melee, Vector.new(-14.0, -2.0, 0.0), 250, 6.0)
        manager:SpawnUnits(EUnitTeam.Ally, EUnitCombatType.Ranged, Vector.new(-14.0, 2.0, 0.0), 250, 6.0)
        manager:SpawnUnits(EUnitTeam.Enemy, EUnitCombatType.Melee, Vector.new(14.0, -2.0, 0.0), 250, 6.0)
        manager:SpawnUnits(EUnitTeam.Enemy, EUnitCombatType.Ranged, Vector.new(14.0, 2.0, 0.0), 250, 6.0)
        print("[CrowdSmoke] stress requested 500 allies + 500 enemies")
    end

    if ENABLE_STRESS_TEST and elapsed > 9.0 and bStartedStressTest then
        PrintCounts("stress")
        bStartedStressTest = false
    end
end
