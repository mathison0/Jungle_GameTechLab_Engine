#include "UI/UserWidget.h"

#include "Object/Reflection/ObjectFactory.h"
#include "UI/UIManager.h"



void UUserWidget::Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath)
{
	OwningPlayer = InOwningPlayer;
	DocumentPath = InDocumentPath;
}

void UUserWidget::AddToViewport(int32 InZOrder)
{
	ZOrder = InZOrder;
	bInViewport = true;
	UUIManager::Get().AddToViewport(this, InZOrder);
}

void UUserWidget::RemoveFromParent()
{
	UUIManager::Get().RemoveFromViewport(this);
	bInViewport = false;
}

void UUserWidget::BindClick(const FString& ElementId, sol::protected_function Callback)
{
	PendingLuaEventBindings.push_back({ ElementId, "click", std::move(Callback) });
	if (IsDocumentLoaded())
	{
		RegisterEventListeners();
	}
}

void UUserWidget::BindClick(const FString& ElementId, std::function<void()> Callback)
{
	BindEvent(ElementId, "click", std::move(Callback));
}

void UUserWidget::BindEvent(const FString& ElementId, const FString& EventName, std::function<void()> Callback)
{
	PendingNativeEventBindings.push_back({ ElementId, EventName, std::move(Callback) });
	if (IsDocumentLoaded())
	{
		RegisterEventListeners();
	}
}

void UUserWidget::BindMouseOver(const FString& ElementId, std::function<void()> Callback)
{
	BindEvent(ElementId, "mouseover", std::move(Callback));
}

void UUserWidget::RegisterEventListeners()
{
	if (!Document)
	{
		return;
	}

	ClearEventListeners();

	for (const FWidgetLuaEventBinding& Binding : PendingLuaEventBindings)
	{
		Rml::Element* Element = Document->GetElementById(Binding.ElementId);
		if (!Element)
		{
			UE_LOG("[RmlUi] Event target not found: %s", Binding.ElementId.c_str());
			continue;
		}

		auto* Listener = new FWidgetEventListener(Binding.ElementId, Binding.EventName, Binding.Callback);
		Element->AddEventListener(Binding.EventName.c_str(), Listener);
		EventListeners.push_back(Listener);
	}

	for (const FWidgetNativeEventBinding& Binding : PendingNativeEventBindings)
	{
		Rml::Element* Element = Document->GetElementById(Binding.ElementId);
		if (!Element)
		{
			UE_LOG("[RmlUi] Event target not found: %s", Binding.ElementId.c_str());
			continue;
		}

		auto* Listener = new FWidgetEventListener(Binding.ElementId, Binding.EventName, Binding.Callback);
		Element->AddEventListener(Binding.EventName.c_str(), Listener);
		EventListeners.push_back(Listener);
	}
}

void UUserWidget::ClearEventListeners()
{
	if (Document)
	{
		for (FWidgetEventListener* Listener : EventListeners)
		{
			if (!Listener)
			{
				continue;
			}

			Rml::Element* Element = Document->GetElementById(Listener->GetElementId());
			if (Element)
			{
				Element->RemoveEventListener(Listener->GetEventName().c_str(), Listener);
			}
		}
	}

	for (FWidgetEventListener* Listener : EventListeners)
	{
		delete Listener;
	}
	EventListeners.clear();
}

void UUserWidget::SetText(const FString& ElementId, const FString& Text)
{
	if (!Document)
	{
		return;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Text target not found: %s", ElementId.c_str());
		return;
	}

	Element->SetInnerRML(Text.c_str());
}

FString UUserWidget::GetValue(const FString& ElementId) const
{
	if (!Document)
	{
		return {};
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Value target not found: %s", ElementId.c_str());
		return {};
	}

	Rml::ElementFormControl* FormControl = rmlui_dynamic_cast<Rml::ElementFormControl*>(Element);
	if (!FormControl)
	{
		UE_LOG("[RmlUi] Value target is not a form control: %s", ElementId.c_str());
		return {};
	}

	return FString(FormControl->GetValue().c_str());
}

bool UUserWidget::SetValue(const FString& ElementId, const FString& Value)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Value target not found: %s", ElementId.c_str());
		return false;
	}

	Rml::ElementFormControl* FormControl = rmlui_dynamic_cast<Rml::ElementFormControl*>(Element);
	if (!FormControl)
	{
		UE_LOG("[RmlUi] Value target is not a form control: %s", ElementId.c_str());
		return false;
	}

	FormControl->SetValue(Value.c_str());
	return true;
}

bool UUserWidget::Focus(const FString& ElementId, bool bFocusVisible)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Focus target not found: %s", ElementId.c_str());
		return false;
	}

	return Element->Focus(bFocusVisible);
}

bool UUserWidget::SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Property target not found: %s", ElementId.c_str());
		return false;
	}

	return Element->SetProperty(PropertyName.c_str(), Value.c_str());
}

bool UUserWidget::SetAttribute(const FString& ElementId, const FString& AttributeName, float Value)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Attribute target not found: %s", ElementId.c_str());
		return false;
	}

	Element->SetAttribute(AttributeName.c_str(), Value);
	return true;
}

bool UUserWidget::SetClass(const FString& ElementId, const FString& ClassName, bool bActive)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Class target not found: %s", ElementId.c_str());
		return false;
	}

	Element->SetClass(ClassName.c_str(), bActive);
	return true;
}

bool UUserWidget::Click(const FString& ElementId)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Click target not found: %s", ElementId.c_str());
		return false;
	}

	Element->Click();
	return true;
}
