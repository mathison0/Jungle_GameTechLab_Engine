---@class LevelUpChoice : ScriptComponent
local Script = {
    properties = {
        PanelWidth = { type = "float", default = 620.0, min = 320.0, max = 1200.0, speed = 10.0 },
        PanelHeight = { type = "float", default = 430.0, min = 240.0, max = 900.0, speed = 10.0 },
        OptionWidth = { type = "float", default = 520.0, min = 240.0, max = 1000.0, speed = 10.0 },
        OptionHeight = { type = "float", default = 78.0, min = 44.0, max = 160.0, speed = 2.0 },
    }
}

local GameManager = require("GameManager")

local function vec2(x, y)
    return { x = x, y = y }
end

local function configureUI(ui, anchor, size, layer, zOrder, tint, hitTest)
    if ui == nil or not ui:IsValid() then
        return
    end

    ui:SetUIRenderSpace("ScreenSpace")
    ui:SetUIAnchor(anchor)
    ui:SetUIAnchoredPosition(vec2(0.0, 0.0))
    ui:SetUISizeDelta(size)
    ui:SetUIPivot(vec2(0.5, 0.5))
    ui:SetUILayer(layer)
    ui:SetUIZOrder(zOrder)
    ui:SetUIHitTestVisible(hitTest == true)

    if tint ~= nil then
        ui:SetUITint(tint[1], tint[2], tint[3], tint[4])
    end

    ui:SetUIVisibility(true)
end

local function describeOption(option)
    if option == nil then
        return ""
    end

    local name = option.DisplayName or option.WeaponId or "Unknown"
    if option.Type == "AddWeapon" then
        return name .. "    New Weapon"
    end

    local currentLevel = option.CurrentLevel or 1
    local nextLevel = option.NextLevel or (currentLevel + 1)
    return name .. "    Upgrade Lv." .. tostring(currentLevel) .. " -> Lv." .. tostring(nextLevel)
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

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.actors = {}
    self.optionButtons = {}
    self.visible = false
    self.choiceLocked = false
    self.GemRainScript = self.GetComponentByName("UScriptComponent", "GemRainScript")

    self:SetGemRainEnabled(false)
    GameManager.RegisterLevelUpUI(self)
end

function Script:SetGemRainEnabled(enabled)
    if self.GemRainScript ~= nil and self.GemRainScript:IsValid() then
        self.GemRainScript:CallScript("SetEnabled", enabled == true)
    end
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

function Script:CreateImage(anchor, size, layer, zOrder, tint)
    local actor = self:AcquireActor("AUIActor")
    if actor == nil then
        return nil
    end

    local ui = actor:GetComponent("UTextureUIComponent")
    configureUI(ui, anchor, size, layer, zOrder, tint, false)
    return ui
end

function Script:CreateText(text, anchor, size, layer, zOrder, fontSize, tint)
    local actor = self:AcquireActor("ATextUIActor")
    if actor == nil then
        return nil
    end

    local ui = actor:GetComponent("UTextUIComponent")
    configureUI(ui, anchor, size, layer, zOrder, tint or { 1.0, 1.0, 1.0, 1.0 }, false)

    if ui ~= nil and ui:IsValid() then
        ui:SetUIText(text)
        ui:SetUIFont("Default")
        ui:SetUIFontSize(fontSize or 1.0)
    end

    return ui
end

function Script:CreateButton(option, index, anchor)
    local actor = self:AcquireActor("AButtonActor")
    if actor == nil then
        return
    end

    local button = actor:GetComponent("UUIButtonComponent")
    configureUI(
        button,
        anchor,
        vec2(self.OptionWidth or 520.0, self.OptionHeight or 78.0),
        320,
        index * 10,
        nil,
        true
    )

    if button ~= nil and button:IsValid() then
        button:SetButtonInteractable(true)
        button:SetUITexturePath("")
    end

    self:CreateText(
        describeOption(option),
        anchor,
        vec2((self.OptionWidth or 520.0) - 44.0, self.OptionHeight or 78.0),
        321,
        index * 10,
        0.92,
        { 1.0, 1.0, 1.0, 1.0 }
    )

    self.optionButtons[index] = {
        component = button,
        lastClickCount = button ~= nil and button:IsValid() and button:GetButtonClickCount() or 0,
    }
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

function Script:Show(options)
    self:Hide()

    self.visible = true
    self.choiceLocked = false
    self.options = options or {}
    self.optionButtons = {}

    self:SetGemRainEnabled(true)

    self:CreateImage(vec2(0.5, 0.5), vec2(4000.0, 4000.0), 300, 0, { 0.0, 0.0, 0.0, 0.48 })
    self:CreateImage(
        vec2(0.5, 0.5),
        vec2(self.PanelWidth or 620.0, self.PanelHeight or 430.0),
        310,
        0,
        { 0.055, 0.065, 0.095, 0.92 }
    )
    self:CreateText("LEVEL UP", vec2(0.5, 0.345), vec2(520.0, 56.0), 321, 0, 1.3, { 0.95, 0.92, 0.62, 1.0 })

    local startY = 0.46
    local spacing = 0.115
    for index = 1, math.min(3, #self.options) do
        self:CreateButton(self.options[index], index, vec2(0.5, startY + spacing * (index - 1)))
    end
end

function Script:Hide()
    self.visible = false
    self.choiceLocked = false
    self.options = nil
    self.optionButtons = {}
    self:SetGemRainEnabled(false)
    self:ReleaseActors()
end

function Script:Tick(deltaTime)
    if not self.visible or self.choiceLocked then
        return
    end

    for index, entry in pairs(self.optionButtons) do
        local button = entry.component
        if button ~= nil and button:IsValid() then
            local clickCount = button:GetButtonClickCount()
            if clickCount ~= entry.lastClickCount then
                self.choiceLocked = true
                for _, other in pairs(self.optionButtons) do
                    if other.component ~= nil and other.component:IsValid() then
                        other.component:SetButtonInteractable(false)
                    end
                end
                GameManager.ConfirmLevelUpChoice(index)
                return
            end
        end
    end
end

function Script:EndPlay()
    self:Hide()
end

return Script
