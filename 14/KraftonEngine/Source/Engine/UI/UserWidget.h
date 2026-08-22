#pragma once

#include "Object/Object.h"
#include "Core/Logging/Log.h"
#include "Source/Engine/UI/UserWidget.generated.h"
#include <functional>
#include <sol/sol.hpp>
#include <utility>

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>

class APlayerController;
class FWidgetEventListener;
namespace Rml { class ElementDocument; }

struct FWidgetLuaEventBinding
{
	FString ElementId;
	FString EventName;
	sol::protected_function Callback;
};

struct FWidgetNativeEventBinding
{
	FString ElementId;
	FString EventName;
	std::function<void()> Callback;
};

class FWidgetEventListener final : public Rml::EventListener
{
public:
	FWidgetEventListener(FString InElementId, FString InEventName, sol::protected_function InCallback)
		: ElementId(std::move(InElementId))
		, EventName(std::move(InEventName))
		, LuaCallback(std::move(InCallback))
	{
	}

	FWidgetEventListener(FString InElementId, FString InEventName, std::function<void()> InCallback)
		: ElementId(std::move(InElementId))
		, EventName(std::move(InEventName))
		, NativeCallback(std::move(InCallback))
	{
	}

	void ProcessEvent(Rml::Event& /*Event*/) override
	{
		if (NativeCallback)
		{
			NativeCallback();
			return;
		}

		if (!LuaCallback.valid())
		{
			return;
		}

		sol::protected_function_result Result = LuaCallback();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua] UI event callback error: %s", Err.what());
		}
	}

	const FString& GetElementId() const { return ElementId; }
	const FString& GetEventName() const { return EventName; }

private:
	FString ElementId;
	FString EventName;
	sol::protected_function LuaCallback;
	std::function<void()> NativeCallback;
};

UCLASS()
class UUserWidget : public UObject
{
public:
	GENERATED_BODY()
	UUserWidget() = default;
	~UUserWidget() override = default;

	void Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath);
	void AddToViewport(int32 InZOrder = 0);
	void RemoveFromParent();
	void BindClick(const FString& ElementId, sol::protected_function Callback);
	void BindClick(const FString& ElementId, std::function<void()> Callback);
	void BindEvent(const FString& ElementId, const FString& EventName, std::function<void()> Callback);
	void BindMouseOver(const FString& ElementId, std::function<void()> Callback);
	void RegisterEventListeners();
	void ClearEventListeners();
	void SetText(const FString& ElementId, const FString& Text);
	FString GetValue(const FString& ElementId) const;
	bool SetValue(const FString& ElementId, const FString& Value);
	bool Focus(const FString& ElementId, bool bFocusVisible = true);
	bool SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value);
	bool SetAttribute(const FString& ElementId, const FString& AttributeName, float Value);
	bool SetClass(const FString& ElementId, const FString& ClassName, bool bActive);
	bool Click(const FString& ElementId);

	APlayerController* GetOwningPlayer() const { return OwningPlayer; }
	const FString& GetDocumentPath() const { return DocumentPath; }
	int32 GetZOrder() const { return ZOrder; }
	bool IsInViewport() const { return bInViewport; }
	bool IsDocumentLoaded() const { return bDocumentLoaded; }
	Rml::ElementDocument* GetDocument() const { return Document; }

	// 메뉴/대화창처럼 사용자가 클릭/포인팅을 해야 하는 widget 은 true 로 설정.
	// UUIManager 가 viewport 에 올라온 widget 중 하나라도 이 값이 true 면 GameViewportClient
	// 에 알려 시스템 커서를 보이고 raw mouse / clip 을 해제하도록 한다. HUD 처럼 비대화형
	// 오버레이는 false 유지.
	void SetWantsMouse(bool bInWantsMouse) { bWantsMouse = bInWantsMouse; }
	bool WantsMouse() const { return bWantsMouse; }

	void MarkDocumentLoaded(Rml::ElementDocument* InDocument) { Document = InDocument; bDocumentLoaded = Document != nullptr; }
	void MarkRemovedFromViewport() { bInViewport = false; }
	void ClearDocument() { Document = nullptr; bDocumentLoaded = false; }

private:
	APlayerController* OwningPlayer = nullptr;
	Rml::ElementDocument* Document = nullptr;
	FString DocumentPath;
	TArray<FWidgetLuaEventBinding> PendingLuaEventBindings;
	TArray<FWidgetNativeEventBinding> PendingNativeEventBindings;
	TArray<FWidgetEventListener*> EventListeners;
	int32 ZOrder = 0;
	bool bInViewport = false;
	bool bDocumentLoaded = false;
	bool bWantsMouse = false;
};
