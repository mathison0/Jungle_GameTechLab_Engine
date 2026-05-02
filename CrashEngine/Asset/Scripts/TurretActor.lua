local TurretHead = require("Components.TurretHead")

local Script = {
    properties = {
        TargetTag = { type = "string", default = "Player" },
        LeftHead = { type = "string", default = "LeftHead" },
        RightHead = { type = "string", default = "RightHead" },
        TargetSearchRadius = { type = "float", default = 10000.0, min = 0.0, max = 100000.0, speed = 100.0 },
    }
}

function Script:BeginPlay()
    self.LeftHeadRoutine = self.StartCoroutine(function()
        TurretHead.RotateToward(self, self.LeftHead, self.TargetTag)
    end)

    self.RightHeadRoutine = self.StartCoroutine(function()
        TurretHead.RotateToward(self, self.RightHead, self.TargetTag)
    end)
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
