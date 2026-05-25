#pragma once

#include "Editor/UI/EditorCurveEditorWidget.h"
#include "Editor/UI/EditorWidget.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/ParticleSystemViewportClient.h"
#include "Particle/ParticleTypes.h"
#include "Render/Common/ComPtr.h"
#include "ImGui/imgui.h"

class UParticleEmitter;
class UParticleModule;
class UParticleLODLevel;
class UParticleSystem;
class UObject;
struct FProperty;
struct ID3D11ShaderResourceView;

class FEditorParticleSystemWidget : public FEditorWidget
{
public:
	~FEditorParticleSystemWidget() override;

	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void RenderEmbedded(float DeltaTime);
	void RenderDetachedDocumentChrome(bool& bCloseRequested);
	void RenderDocumentToolbarControls();
	void Shutdown();
	bool CanUndo() const;
	bool CanRedo() const;
	bool Undo();
	bool Redo();

	void OpenLayoutTest(const FString& InDocumentPath = "");
	const FString& GetDocumentPath() const { return DocumentPath; }
	bool IsDirty() const { return bDirty; }
	bool IsPreviewViewportVisible() const { return bPreviewViewportVisible; }
	bool HasValidPreviewViewportRect() const { return bPreviewViewportRectValid; }
	FSceneViewport* GetPreviewViewport() { return bPreviewViewportInitialized ? &PreviewViewport : nullptr; }
	const FSceneViewport* GetPreviewViewport() const { return bPreviewViewportInitialized ? &PreviewViewport : nullptr; }
	FParticleSystemViewportClient* GetPreviewClient() { return bPreviewViewportInitialized ? &PreviewClient : nullptr; }
	const FParticleSystemViewportClient* GetPreviewClient() const { return bPreviewViewportInitialized ? &PreviewClient : nullptr; }

private:
	enum class ECascadeToolbarIcon : int32
	{
		Save,
		Find,
		RestartSim,
		RestartLevel,
		Undo,
		Redo,
		Thumbnail,
		Bounds,
		OriginAxis,
		BackgroundColor,
		RegenLOD,
		LowestLOD,
		LowerLOD,
		AddLOD,
		HigherLOD,
		Menu,
		Count
	};

	static constexpr int32 CascadeToolbarIconCount = static_cast<int32>(ECascadeToolbarIcon::Count);

	struct FParticleEditorUndoEntry
	{
		FString Label;
		FString Snapshot;
		int32 CurrentLOD = 0;
		int32 SelectedEmitterIndex = 0;
		int32 SelectedModuleIndex = -1;
	};

	void EnsurePreviewViewport();
	void ShutdownPreviewViewport();
	void LoadCascadeToolbarIcons();
	ID3D11ShaderResourceView* GetCascadeToolbarIcon(ECascadeToolbarIcon Icon) const;
	void DrawMainLayout();
	void DrawViewportPanel(const ImVec2& Size);
	void DrawViewportMenuBar(const ImVec2& CanvasMin);
	void DrawEmittersPanel(const ImVec2& Size);
	void DrawEmitterContextMenu();
	void AddDefaultEmitter();
	void AddDefaultEmitterAt(int32 InsertIndex);
	void DeleteSelectedEmitter();
	void DeleteEmitter(int32 EmitterIndex);
	void AddModuleToEmitter(int32 EmitterIndex, UParticleModule* Module);
	void DeleteModule(int32 EmitterIndex, int32 ModuleIndex);
	void ChangeEmitterRenderMode(int32 EmitterIndex, EParticleEmitterRenderMode RenderMode);
	void BeginRenameEmitter(int32 EmitterIndex);
	void RenameEmitter(int32 EmitterIndex, const FString& NewName);
	bool ApplyEmitterName(int32 EmitterIndex, const FString& NewName, bool bCaptureUndo, bool bWarnOnEmpty);
	void DrawEmitterRenamePopup();
	void SelectParticleSystem();
	void SelectEmitter(int32 EmitterIndex);
	void SelectModule(int32 EmitterIndex, int32 ModuleIndex);
	void OpenEmitterContextMenu(int32 EmitterIndex, int32 ModuleIndex);
	void ClearEmitterContext();
	void ShowCenterToast(const FString& Message);
	void DrawCenterToast(const ImVec2& AreaMin, const ImVec2& AreaSize);
	void CaptureUndoSnapshot(const char* Label);
	FString CaptureParticleSnapshot() const;
	bool RestoreParticleSnapshot(const FString& Snapshot, int32 InCurrentLOD, int32 InSelectedEmitterIndex, int32 InSelectedModuleIndex);
	void ClearUndoHistory();
	void PushUndoEntry(TArray<FParticleEditorUndoEntry>& Stack, const FParticleEditorUndoEntry& Entry, bool bSkipDuplicate);
	void ClampSelectionToParticleSystem();
	void ResetPendingReorders();
	void ApplyPendingReorders();
	void ReorderEmitter(int32 SourceIndex, int32 InsertIndex);
	void ReorderModule(int32 SourceEmitterIndex, int32 SourceModuleIndex, int32 TargetEmitterIndex, int32 InsertIndex);
	void DrawEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex, float ColumnHeight);
	void DrawEmitterModuleRow(UParticleModule* Module, int32 EmitterIndex, int32 ModuleIndex, bool bRequired, float RowHeight);
	void DrawDetailsPanel(const ImVec2& Size);
	UParticleLODLevel* GetEmitterLODLevel(UParticleEmitter* Emitter) const;
	UParticleLODLevel* GetSelectedLODLevel() const;
	UParticleModule* GetSelectedModule() const;
	UParticleEmitter* GetSelectedEmitter() const;
	void DrawEmitterDetails(UParticleEmitter* Emitter, int32 EmitterIndex);
	void DrawParticleSystemDetails(UParticleSystem* ParticleSystem);
	void DrawParticleModuleDetails(UParticleModule* Module, UParticleEmitter* OwnerEmitter);
	bool DrawParticleModuleProperty(UParticleModule* Module, const FProperty& Property);
	bool DrawParticlePropertyValue(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget, const char* Label);
	bool DrawParticleStructPropertyValue(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget, const char* Label);
	void NotifyParticleModulePropertyChanged(UParticleModule* Module, UParticleEmitter* OwnerEmitter, const FProperty& Property);
	void DrawCurveEditorPanel(const ImVec2& Size);

	FEditorCurveEditorWidget CurveEditorWidget;
	FSceneViewport PreviewViewport;
	FParticleSystemViewportClient PreviewClient;
	UParticleSystem* ParticleSystemAsset = nullptr;
	FName PreviewWorldHandle = FName::None;
	FString SelectedCurveAssetPath;
	FString DocumentPath;
	bool bDirty = true;
	bool bShowThumbnail = false;
	bool bShowBounds = true;
	bool bShowOriginAxis = true;
	bool bPreviewPaused = true;
	bool bPreviewRealtime = true;
	bool bPreviewLoop = true;
	bool bPreviewViewportInitialized = false;
	bool bPreviewViewportVisible = false;
	bool bPreviewViewportRectValid = false;
	int32 CurrentLOD = 0;
	int32 PreviewAnimSpeedIndex = 0;
	int32 SelectedEmitterIndex = 0;
	int32 SelectedModuleIndex = -1;
	int32 ContextEmitterIndex = -1;
	int32 ContextModuleIndex = -1;
	int32 RenameEmitterIndex = -1;
	int32 DetailEmitterNameEditIndex = -1;
	int32 PendingEmitterMoveSource = -1;
	int32 PendingEmitterMoveInsertIndex = -1;
	int32 PendingModuleMoveEmitterIndex = -1;
	int32 PendingModuleMoveTargetEmitterIndex = -1;
	int32 PendingModuleMoveSource = -1;
	int32 PendingModuleMoveInsertIndex = -1;
	char RenameEmitterBuffer[128] = {};
	char DetailEmitterNameEditBuffer[128] = {};
	FString CenterToastMessage;
	TArray<FParticleEditorUndoEntry> UndoHistory;
	TArray<FParticleEditorUndoEntry> RedoHistory;
	TComPtr<ID3D11ShaderResourceView> CascadeToolbarIcons[CascadeToolbarIconCount];
	bool bOpenEmitterContextMenu = false;
	bool bOpenRenameEmitterPopup = false;
	bool bRestoringParticleSnapshot = false;
	bool bPropertyEditUndoCaptured = false;
	bool bEmitterNameEditUndoCaptured = false;
	bool bCascadeToolbarIconsLoadAttempted = false;
	float TopAreaHeight = 0.0f;
	float TopLeftWidth = 0.0f;
	float BottomLeftWidth = 0.0f;
	float LastDeltaTime = 0.0f;
	float CenterToastRemainingTime = 0.0f;
};
