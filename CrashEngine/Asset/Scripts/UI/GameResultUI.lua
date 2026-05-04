---@class GameResultUI : ScriptComponent
local Script = {
    properties = {
        PanelWidth = { type = "float", default = 720.0, min = 360.0, max = 1200.0, speed = 10.0 },
        PanelHeight = { type = "float", default = 560.0, min = 260.0, max = 900.0, speed = 10.0 },
        ButtonWidth = { type = "float", default = 260.0, min = 120.0, max = 520.0, speed = 10.0 },
        ButtonHeight = { type = "float", default = 58.0, min = 32.0, max = 120.0, speed = 2.0 },
        Layer = { type = "int", default = 380, min = 0, max = 1000, speed = 1 },
    }
}

local GameManager = require("GameManager")

local function vec2(x, y)
    return { x = x, y = y }
end

local function formatTime(timeRemaining)
    local totalSeconds = math.max(0, math.floor((tonumber(timeRemaining) or 0.0) + 0.5))
    local minutes = math.floor(totalSeconds / 60)
    local seconds = totalSeconds % 60
    return string.format("%02d:%02d", minutes, seconds)
end

local function hideActorUI(actor)
    if actor == nil or not actor:IsValid() then
        return
    end

    local components = actor:GetComponents("UUIComponent")
    if components == nil then
        return
    end

    for _, component in ipairs(components) do
        if component ~= nil and component:IsValid() and component.SetUIVisibility ~= nil then
            component:SetUIVisibility(false)
        end
    end
end

local function configureUI(ui, anchor, position, size, layer, zOrder, tint, hitTest)
    if ui == nil or not ui:IsValid() then
        return false
    end

    ui:SetUIRenderSpace("ScreenSpace")
    ui:SetUIAnchor(anchor)
    ui:SetUIAnchoredPosition(position or vec2(0.0, 0.0))
    ui:SetUISizeDelta(size)
    ui:SetUIPivot(vec2(0.5, 0.5))
    ui:SetUILayer(layer)
    ui:SetUIZOrder(zOrder)
    ui:SetUIHitTestVisible(hitTest == true)

    if tint ~= nil then
        ui:SetUITint(tint[1], tint[2], tint[3], tint[4])
    end

    ui:SetUIVisibility(true)
    return true
end

local function resultTitle(resultType)
    if resultType == "GameClear" then
        return "GAME CLEAR"
    end

    return "GAME OVER"
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.actors = {}
    self.buttons = {}
    self.visible = false
    self.locked = false

    GameManager.RegisterResultUI(self)
end

function Script:AcquireActor(className)
    if self.pool == nil or not self.pool:IsValid() then
        return nil
    end

    local actor = self.pool:Acquire(className)
    if actor == nil or not actor:IsValid() then
        return nil
    end

    actor:SetVisible(true)
    table.insert(self.actors, actor)
    return actor
end

function Script:CreateImage(anchor, position, size, layer, zOrder, tint)
    local actor = self:AcquireActor("AUIActor")
    if actor == nil then
        return nil
    end

    local ui = actor:GetComponent("UTextureUIComponent")
    if ui == nil or not ui:IsValid() then
        ui = actor:GetComponent("UUIComponent")
    end

    if not configureUI(ui, anchor, position, size, layer, zOrder, tint, false) then
        return nil
    end

    return ui
end

function Script:CreateText(text, anchor, position, size, layer, zOrder, fontSize, tint, horizontal)
    local actor = self:AcquireActor("ATextUIActor")
    if actor == nil then
        return nil
    end

    local ui = actor:GetComponent("UTextUIComponent")
    if not configureUI(ui, anchor, position, size, layer, zOrder, tint or { 1.0, 1.0, 1.0, 1.0 }, false) then
        return nil
    end

    ui:SetUIText(text)
    ui:SetUIFont("Default")
    ui:SetUIFontSize(fontSize or 1.0)
    ui:SetUITextHorizontalAlignment(horizontal or "Center")
    ui:SetUITextVerticalAlignment("Center")
    return ui
end

function Script:CreateButton(label, anchor, position, action)
    local actor = self:AcquireActor("AButtonActor")
    if actor == nil then
        return nil
    end

    local layer = self.Layer or 380
    local button = actor:GetComponent("UUIButtonComponent")
    if not configureUI(
        button,
        anchor,
        position,
        vec2(self.ButtonWidth or 260.0, self.ButtonHeight or 58.0),
        layer + 4,
        0,
        { 0.10, 0.18, 0.28, 0.92 },
        true
    ) then
        return nil
    end

    button:SetUITexturePath("")
    button:SetButtonInteractable(true)

    self:CreateText(
        label,
        anchor,
        position,
        vec2((self.ButtonWidth or 260.0) - 20.0, self.ButtonHeight or 58.0),
        layer + 5,
        0,
        1.0,
        { 1.0, 1.0, 1.0, 1.0 },
        "Center"
    )

    table.insert(self.buttons, {
        component = button,
        lastClickCount = button:GetButtonClickCount() or 0,
        action = action,
    })

    return button
end

function Script:ReleaseActors()
    if self.pool == nil or not self.pool:IsValid() then
        self.actors = {}
        return
    end

    for _, actor in ipairs(self.actors) do
        if actor ~= nil and actor:IsValid() then
            hideActorUI(actor)
            actor:SetVisible(false)
            self.pool:Release(actor)
        end
    end

    self.actors = {}
end

function Script:Hide()
    self.visible = false
    self.locked = false
    self.buttons = {}
    self:ReleaseActors()
end

function Script:GetRecords(source)
    if source ~= nil and source.Records ~= nil then
        return source.Records
    end

    if type(ScoreBoard_LoadRecords) == "function" then
        return ScoreBoard_LoadRecords()
    end

    return {}
end

function Script:CreateFrame(title)
    local layer = self.Layer or 380
    self:CreateImage(vec2(0.5, 0.5), vec2(0.0, 0.0), vec2(4000.0, 4000.0), layer, 0, { 0.0, 0.0, 0.0, 0.56 })
    self:CreateImage(
        vec2(0.5, 0.5),
        vec2(0.0, 0.0),
        vec2(self.PanelWidth or 720.0, self.PanelHeight or 560.0),
        layer + 1,
        0,
        { 0.035, 0.045, 0.070, 0.96 }
    )
    self:CreateText(title, vec2(0.5, 0.245), vec2(0.0, 0.0), vec2(620.0, 58.0), layer + 2, 0, 1.45, { 1.0, 0.92, 0.62, 1.0 })
end

function Script:CreateScoreRows(records, startY, layer)
    self:CreateText("RANK   RESULT       TIME    KILLS", vec2(0.5, startY), vec2(0.0, 0.0), vec2(620.0, 26.0), layer, 0, 0.68, { 0.72, 0.78, 0.88, 1.0 })

    local rowY = startY + 0.036
    local maxRows = math.min(10, #(records or {}))
    if maxRows <= 0 then
        self:CreateText("NO RECORDS", vec2(0.5, rowY + 0.03), vec2(0.0, 0.0), vec2(620.0, 30.0), layer, 0, 0.8, { 0.9, 0.9, 0.9, 1.0 })
        return
    end

    for index = 1, maxRows do
        local record = records[index]
        local rank = record.Rank or index
        local result = record.ResultType == "GameClear" and "CLEAR" or "OVER "
        local text = string.format("#%02d    %-6s      %s    %d", rank, result, formatTime(record.RemainingTime), record.KillCount or 0)
        local tint = { 0.96, 0.96, 0.96, 1.0 }
        if record.ResultType == "GameClear" then
            tint = { 0.70, 1.0, 0.78, 1.0 }
        end
        self:CreateText(text, vec2(0.5, rowY + (index - 1) * 0.035), vec2(0.0, 0.0), vec2(620.0, 28.0), layer, index, 0.74, tint)
    end
end

function Script:ShowResult(record)
    self:Hide()

    self.visible = true
    self.locked = false
    self.buttons = {}

    local layer = self.Layer or 380
    local resultType = record ~= nil and record.ResultType or "GameOver"
    local title = resultTitle(resultType)
    self:CreateFrame(title)

    local remainingTime = record ~= nil and record.RemainingTime or 0.0
    local killCount = record ~= nil and record.KillCount or 0
    local rank = record ~= nil and record.Rank or 0
    local rankText = rank > 0 and ("RANK #" .. tostring(rank)) or "RANK --"

    self:CreateText("TIME LEFT  " .. formatTime(remainingTime), vec2(0.5, 0.335), vec2(0.0, 0.0), vec2(520.0, 34.0), layer + 2, 0, 0.88, { 1.0, 1.0, 1.0, 1.0 })
    self:CreateText("KILLS  " .. tostring(killCount), vec2(0.5, 0.382), vec2(0.0, 0.0), vec2(520.0, 34.0), layer + 2, 0, 0.88, { 1.0, 1.0, 1.0, 1.0 })
    self:CreateText(rankText, vec2(0.5, 0.428), vec2(0.0, 0.0), vec2(520.0, 34.0), layer + 2, 0, 0.94, { 1.0, 0.88, 0.48, 1.0 })

    self:CreateScoreRows(self:GetRecords(record), 0.488, layer + 2)
    self:CreateButton("MAIN MENU", vec2(0.5, 0.790), vec2(0.0, 0.0), "MainMenu")
end

function Script:ShowScoreBoard()
    self:Hide()

    self.visible = true
    self.locked = false
    self.buttons = {}

    local layer = self.Layer or 380
    self:CreateFrame("SCORE BOARD")
    self:CreateScoreRows(self:GetRecords(nil), 0.345, layer + 2)
    self:CreateButton("CLOSE", vec2(0.5, 0.790), vec2(0.0, 0.0), "Close")
end

function Script:Tick(deltaTime)
    if not self.visible or self.locked then
        return
    end

    for _, entry in ipairs(self.buttons or {}) do
        local button = entry.component
        if button ~= nil and button:IsValid() then
            local clickCount = button:GetButtonClickCount() or 0
            if clickCount > (entry.lastClickCount or 0) then
                entry.lastClickCount = clickCount
                self.locked = true

                if entry.action == "MainMenu" then
                    GameManager.ReturnToMainMenu()
                else
                    self:Hide()
                end
                return
            end
        end
    end
end

function Script:EndPlay()
    GameManager.UnregisterResultUI(self)
    self:Hide()
end

return Script
