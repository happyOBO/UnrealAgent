#include "Blueprint/WidgetTreeEditor.h"
#include "Blueprint/BlueprintGraphEditor.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

UWidgetBlueprint* FWidgetTreeEditor::LoadWidgetBlueprint(const FString& BlueprintPath, FString& OutError)
{
	// 엔진/Script/cooked 가드를 LoadBlueprint에서 재사용한 뒤 위젯 BP로 캐스팅합니다.
	UBlueprint* Blueprint = FBlueprintGraphEditor::LoadBlueprint(BlueprintPath, OutError);
	if (!Blueprint)
		return nullptr;

	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint);
	if (!WidgetBP)
	{
		OutError = FString::Printf(TEXT("'%s' is not a Widget Blueprint."), *BlueprintPath);
		return nullptr;
	}
	if (!WidgetBP->WidgetTree)
	{
		OutError = TEXT("Widget Blueprint has no WidgetTree.");
		return nullptr;
	}
	return WidgetBP;
}

UClass* FWidgetTreeEditor::ResolveWidgetClass(const FString& TypeName)
{
	if (TypeName.IsEmpty())
		return nullptr;

	FString Name = TypeName;
	if (Name.StartsWith(TEXT("U")) && Name.Len() > 1 && FChar::IsUpper(Name[1]))
		Name.RightChopInline(1);

	UClass* Found = FindFirstObject<UClass>(*Name, EFindFirstObjectOptions::None);
	if (!Found)
		Found = FindFirstObject<UClass>(*TypeName, EFindFirstObjectOptions::None);

	if (Found && Found->IsChildOf(UWidget::StaticClass()) && !Found->HasAnyClassFlags(CLASS_Abstract))
		return Found;
	return nullptr;
}

UWidget* FWidgetTreeEditor::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName)
{
	if (!WidgetBP || !WidgetBP->WidgetTree || WidgetName.IsEmpty())
		return nullptr;
	return WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
}

bool FWidgetTreeEditor::AddWidget(UWidgetBlueprint* WidgetBP, const FString& WidgetType, const FString& WidgetName,
	const FString& ParentName, FString& OutError)
{
	if (!WidgetBP) { OutError = TEXT("Null widget blueprint"); return false; }

	UClass* WidgetClass = ResolveWidgetClass(WidgetType);
	if (!WidgetClass)
	{
		OutError = FString::Printf(TEXT("Unknown or non-widget class '%s'. Use a UWidget subclass name like Button, TextBlock, VerticalBox."), *WidgetType);
		return false;
	}

	UWidgetTree* Tree = WidgetBP->WidgetTree;

	// 이름 중복 방지.
	if (!WidgetName.IsEmpty() && Tree->FindWidget(FName(*WidgetName)))
	{
		OutError = FString::Printf(TEXT("A widget named '%s' already exists."), *WidgetName);
		return false;
	}

	Tree->Modify();

	const FName NewName = WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName);
	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(WidgetClass, NewName);
	if (!NewWidget)
	{
		OutError = FString::Printf(TEXT("Failed to construct widget of type '%s'"), *WidgetType);
		return false;
	}

	if (ParentName.IsEmpty())
	{
		// 루트가 비어 있으면 루트로 설정, 아니면 부모를 명시하라고 안내.
		if (Tree->RootWidget == nullptr)
		{
			Tree->RootWidget = NewWidget;
		}
		else
		{
			OutError = TEXT("Root widget already exists. Specify 'parent' (a panel widget name) to attach the new widget.");
			return false;
		}
	}
	else
	{
		UWidget* ParentWidget = Tree->FindWidget(FName(*ParentName));
		if (!ParentWidget)
		{
			OutError = FString::Printf(TEXT("Parent widget '%s' not found."), *ParentName);
			return false;
		}
		UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			OutError = FString::Printf(TEXT("Parent '%s' (%s) is not a panel widget and cannot have children."), *ParentName, *ParentWidget->GetClass()->GetName());
			return false;
		}
		ParentPanel->AddChild(NewWidget);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	return true;
}

bool FWidgetTreeEditor::RemoveWidget(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError)
{
	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName);
	if (!Widget) { OutError = FString::Printf(TEXT("Widget '%s' not found."), *WidgetName); return false; }

	WidgetBP->WidgetTree->Modify();
	WidgetBP->WidgetTree->RemoveWidget(Widget);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	return true;
}

namespace
{
	// 점(.)으로 구분된 프로퍼티 경로를 따라 중첩 struct를 내려가, 말단 프로퍼티에 값을 ImportText 한다.
	// 예) "Font.Size" → 위젯의 FSlateFontInfo Font 안의 Size; "Anchors" → 평면 단일 토큰.
	// 중간 토큰은 FStructProperty여야 한다(struct 체인만 지원). OwnerObject는 ImportText의 컨텍스트.
	bool ApplyPropertyPath(UStruct* OwnerStruct, void* Container, UObject* OwnerObject,
		const FString& Path, const FString& Value, FString& OutError)
	{
		TArray<FString> Tokens;
		Path.ParseIntoArray(Tokens, TEXT("."), /*CullEmpty*/true);
		if (Tokens.Num() == 0) { OutError = TEXT("Empty property path."); return false; }

		UStruct* CurrentStruct = OwnerStruct;
		void* CurrentContainer = Container;

		// 중간 토큰: struct 프로퍼티를 따라 내려간다.
		for (int32 i = 0; i < Tokens.Num() - 1; ++i)
		{
			FProperty* Prop = FindFProperty<FProperty>(CurrentStruct, FName(*Tokens[i]));
			if (!Prop)
			{
				OutError = FString::Printf(TEXT("Property '%s' not found on '%s'."), *Tokens[i], *CurrentStruct->GetName());
				return false;
			}
			FStructProperty* StructProp = CastField<FStructProperty>(Prop);
			if (!StructProp)
			{
				OutError = FString::Printf(TEXT("Property '%s' is not a struct; cannot descend into '%s'."), *Tokens[i], *Path);
				return false;
			}
			CurrentContainer = StructProp->ContainerPtrToValuePtr<void>(CurrentContainer);
			CurrentStruct = StructProp->Struct;
		}

		// 말단 토큰: 값 설정.
		const FString& Leaf = Tokens.Last();
		FProperty* LeafProp = FindFProperty<FProperty>(CurrentStruct, FName(*Leaf));
		if (!LeafProp)
		{
			OutError = FString::Printf(TEXT("Property '%s' not found on '%s'."), *Leaf, *CurrentStruct->GetName());
			return false;
		}
		void* ValuePtr = LeafProp->ContainerPtrToValuePtr<void>(CurrentContainer);
		const TCHAR* Result = LeafProp->ImportText_Direct(*Value, ValuePtr, OwnerObject, PPF_None);
		if (Result == nullptr)
		{
			OutError = FString::Printf(TEXT("Failed to parse value '%s' for property '%s'."), *Value, *Path);
			return false;
		}
		return true;
	}
}

bool FWidgetTreeEditor::SetWidgetProperty(UWidgetBlueprint* WidgetBP, const FString& WidgetName, const FString& PropertyName,
	const FString& Value, FString& OutError)
{
	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName);
	if (!Widget) { OutError = FString::Printf(TEXT("Widget '%s' not found."), *WidgetName); return false; }

	// "Slot." 접두사: 대상은 위젯이 아니라 Widget->Slot(UPanelSlot*, 부모 패널 종류에 따라 Canvas/VerticalBox 등).
	if (PropertyName.StartsWith(TEXT("Slot.")))
	{
		UPanelSlot* Slot = Widget->Slot;
		if (!Slot)
		{
			OutError = FString::Printf(TEXT("Widget '%s' has no Slot (not parented to a panel)."), *WidgetName);
			return false;
		}
		const FString SlotPath = PropertyName.RightChop(5); // "Slot." 제거
		Slot->Modify();
		if (!ApplyPropertyPath(Slot->GetClass(), Slot, Slot, SlotPath, Value, OutError))
			return false;
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
		return true;
	}

	// 평면 또는 struct 중첩 경로(예: Font.Size) — 위젯을 컨테이너로 워킹.
	Widget->Modify();
	if (!ApplyPropertyPath(Widget->GetClass(), Widget, Widget, PropertyName, Value, OutError))
		return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
	return true;
}

bool FWidgetTreeEditor::ReparentWidget(UWidgetBlueprint* WidgetBP, const FString& WidgetName, const FString& NewParentName, FString& OutError)
{
	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName);
	if (!Widget) { OutError = FString::Printf(TEXT("Widget '%s' not found."), *WidgetName); return false; }

	UWidget* NewParent = FindWidgetByName(WidgetBP, NewParentName);
	if (!NewParent) { OutError = FString::Printf(TEXT("New parent '%s' not found."), *NewParentName); return false; }

	UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParent);
	if (!NewParentPanel)
	{
		OutError = FString::Printf(TEXT("New parent '%s' is not a panel widget."), *NewParentName);
		return false;
	}

	WidgetBP->WidgetTree->Modify();
	Widget->RemoveFromParent();
	NewParentPanel->AddChild(Widget);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	return true;
}

namespace
{
	// 위젯과 그 자식을 재귀적으로 JSON 오브젝트로 직렬화합니다.
	TSharedPtr<FJsonObject> SerializeWidget(UWidget* Widget)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!Widget) return Obj;

		Obj->SetStringField(TEXT("name"), Widget->GetName());
		Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			TArray<TSharedPtr<FJsonValue>> Children;
			const int32 Count = Panel->GetChildrenCount();
			for (int32 i = 0; i < Count; ++i)
			{
				if (UWidget* Child = Panel->GetChildAt(i))
					Children.Add(MakeShared<FJsonValueObject>(SerializeWidget(Child)));
			}
			Obj->SetArrayField(TEXT("children"), Children);
		}
		return Obj;
	}
}

FString FWidgetTreeEditor::ListWidgets(UWidgetBlueprint* WidgetBP)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	if (WidgetBP && WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
	{
		Root = SerializeWidget(WidgetBP->WidgetTree->RootWidget);
	}
	else
	{
		Root->SetStringField(TEXT("root"), TEXT("(empty)"));
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}
