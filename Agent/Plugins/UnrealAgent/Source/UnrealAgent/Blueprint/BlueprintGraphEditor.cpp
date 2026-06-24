#include "Blueprint/BlueprintGraphEditor.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_FormatText.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Factories/BlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "SubobjectDataSubsystem.h"
#include "SubobjectDataHandle.h"
#include "SubobjectData.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformAtomics.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"

int32 FBlueprintGraphEditor::NodeIdCounter = 0;
const TCHAR* FBlueprintGraphEditor::NodeIdPrefix = TEXT("UA_ID:");

//-----------------------------------------------------------------------------
// 로드 / 그래프
//-----------------------------------------------------------------------------

UBlueprint* FBlueprintGraphEditor::LoadBlueprint(const FString& BlueprintPath, FString& OutError)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);

	if (!Blueprint && !BlueprintPath.StartsWith(TEXT("/")))
	{
		Blueprint = LoadObject<UBlueprint>(nullptr, *(TEXT("/Game/") + BlueprintPath));
	}

	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath);
		return nullptr;
	}

	const UPackage* Package = Blueprint->GetPackage();
	const FString PackageName = Package ? Package->GetName() : FString();
	if (PackageName.StartsWith(TEXT("/Engine/")) || PackageName.StartsWith(TEXT("/Script/")))
	{
		OutError = TEXT("Engine/Script blueprints cannot be modified.");
		return nullptr;
	}
	if (Package && Package->HasAnyPackageFlags(PKG_Cooked))
	{
		OutError = TEXT("Cooked blueprint cannot be modified.");
		return nullptr;
	}

	return Blueprint;
}

UEdGraph* FBlueprintGraphEditor::FindGraph(UBlueprint* Blueprint, const FString& GraphName, bool bFunctionGraph, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Null blueprint");
		return nullptr;
	}

	const TArray<TObjectPtr<UEdGraph>>& Graphs = bFunctionGraph ? Blueprint->FunctionGraphs : Blueprint->UbergraphPages;

	if (GraphName.IsEmpty())
	{
		if (Graphs.Num() > 0 && Graphs[0])
		{
			return Graphs[0];
		}
		OutError = bFunctionGraph ? TEXT("No function graphs found") : TEXT("No event graphs found");
		return nullptr;
	}

	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
	return nullptr;
}

//-----------------------------------------------------------------------------
// 노드 생성
//-----------------------------------------------------------------------------

UEdGraphNode* FBlueprintGraphEditor::CreateNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& NodeType,
	const TSharedPtr<FJsonObject>& NodeParams, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError)
{
	UEdGraphNode* NewNode = nullptr;

	const TSharedPtr<FJsonObject> Params = NodeParams.IsValid() ? NodeParams : MakeShared<FJsonObject>();

	if (NodeType.Equals(TEXT("CallFunction"), ESearchCase::IgnoreCase))
	{
		const FString FunctionName = Params->GetStringField(TEXT("function"));
		FString TargetClass;
		Params->TryGetStringField(TEXT("target_class"), TargetClass);
		NewNode = CreateCallFunctionNode(Graph, Blueprint, FunctionName, TargetClass, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
	{
		const FString EventName = Params->GetStringField(TEXT("event"));
		NewNode = CreateEventNode(Graph, Blueprint, EventName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("GetVariable"), ESearchCase::IgnoreCase))
	{
		FString VarTargetClass;
		Params->TryGetStringField(TEXT("target_class"), VarTargetClass);
		NewNode = CreateVariableNode(Graph, Blueprint, Params->GetStringField(TEXT("variable")), VarTargetClass, false, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("SetVariable"), ESearchCase::IgnoreCase))
	{
		FString VarTargetClass;
		Params->TryGetStringField(TEXT("target_class"), VarTargetClass);
		NewNode = CreateVariableNode(Graph, Blueprint, Params->GetStringField(TEXT("variable")), VarTargetClass, true, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("Branch"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("IfThenElse"), ESearchCase::IgnoreCase))
	{
		NewNode = CreateBranchNode(Graph, PosX, PosY);
	}
	else if (NodeType.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
	{
		NewNode = CreateSequenceNode(Graph, PosX, PosY);
	}
	else if (NodeType.Equals(TEXT("Cast"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("DynamicCast"), ESearchCase::IgnoreCase))
	{
		FString TargetClass;
		Params->TryGetStringField(TEXT("target_class"), TargetClass);
		NewNode = CreateCastNode(Graph, TargetClass, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("CustomEvent"), ESearchCase::IgnoreCase))
	{
		FString EventName;
		Params->TryGetStringField(TEXT("event"), EventName);
		NewNode = CreateCustomEventNode(Graph, EventName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("OverrideEvent"), ESearchCase::IgnoreCase)
		|| NodeType.Equals(TEXT("OverrideFunction"), ESearchCase::IgnoreCase)
		|| NodeType.Equals(TEXT("Override"), ESearchCase::IgnoreCase))
	{
		FString EventName;
		Params->TryGetStringField(TEXT("event"), EventName);
		NewNode = CreateOverrideEventNode(Graph, Blueprint, EventName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("SwitchEnum"), ESearchCase::IgnoreCase)
		|| NodeType.Equals(TEXT("SwitchOnEnum"), ESearchCase::IgnoreCase))
	{
		FString EnumName;
		Params->TryGetStringField(TEXT("enum"), EnumName);
		NewNode = CreateSwitchEnumNode(Graph, EnumName, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("BreakStruct"), ESearchCase::IgnoreCase)
		|| NodeType.Equals(TEXT("MakeStruct"), ESearchCase::IgnoreCase))
	{
		FString StructName;
		Params->TryGetStringField(TEXT("struct_type"), StructName);
		const bool bMake = NodeType.Equals(TEXT("MakeStruct"), ESearchCase::IgnoreCase);
		NewNode = CreateBreakMakeStructNode(Graph, StructName, bMake, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("FormatText"), ESearchCase::IgnoreCase))
	{
		FString FormatString;
		Params->TryGetStringField(TEXT("format"), FormatString);
		NewNode = CreateFormatTextNode(Graph, FormatString, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("CreateWidget"), ESearchCase::IgnoreCase))
	{
		FString WidgetClass;
		Params->TryGetStringField(TEXT("target_class"), WidgetClass);
		NewNode = CreateWidgetNode(Graph, WidgetClass, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("MacroInstance"), ESearchCase::IgnoreCase))
	{
		FString MacroName;
		Params->TryGetStringField(TEXT("macro"), MacroName);
		NewNode = CreateMacroInstanceNode(Graph, MacroName, PosX, PosY, OutError);
	}
	else if (IsStandardMacroNodeType(NodeType))
	{
		// ForLoop / ForEachLoop / WhileLoop 등은 노드 타입 이름을 매크로 이름으로 직접 사용.
		NewNode = CreateMacroInstanceNode(Graph, NodeType, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("FunctionResult"), ESearchCase::IgnoreCase) || NodeType.Equals(TEXT("Return"), ESearchCase::IgnoreCase))
	{
		NewNode = CreateFunctionResultNode(Graph, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("ComponentBoundEvent"), ESearchCase::IgnoreCase))
	{
		FString Component, DelegateProperty;
		Params->TryGetStringField(TEXT("component"), Component);
		Params->TryGetStringField(TEXT("delegate_property"), DelegateProperty);
		NewNode = CreateComponentBoundEventNode(Graph, Blueprint, Component, DelegateProperty, PosX, PosY, OutError);
	}
	else if (NodeType.Equals(TEXT("AddDelegate"), ESearchCase::IgnoreCase)
		|| NodeType.Equals(TEXT("RemoveDelegate"), ESearchCase::IgnoreCase)
		|| NodeType.Equals(TEXT("CreateDelegate"), ESearchCase::IgnoreCase))
	{
		FString DelegateProperty, DelegateOwner, BoundFunction;
		Params->TryGetStringField(TEXT("delegate_property"), DelegateProperty);
		Params->TryGetStringField(TEXT("delegate_owner"), DelegateOwner);
		Params->TryGetStringField(TEXT("bound_function"), BoundFunction);
		NewNode = CreateDelegateNode(Graph, Blueprint, NodeType, DelegateProperty, DelegateOwner, BoundFunction, PosX, PosY, OutError);
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown node_type '%s'. Supported: CallFunction, Event, OverrideEvent, VariableGet, VariableSet, Branch, Sequence, Cast, CustomEvent, FunctionResult, ComponentBoundEvent, AddDelegate, RemoveDelegate, CreateDelegate, SwitchEnum, BreakStruct, MakeStruct, FormatText, CreateWidget, MacroInstance, ForLoop, ForEachLoop, WhileLoop"), *NodeType);
		return nullptr;
	}

	if (!NewNode)
	{
		if (OutError.IsEmpty())
			OutError = FString::Printf(TEXT("Failed to create node of type '%s'"), *NodeType);
		return nullptr;
	}

	OutNodeId = GenerateNodeId(NodeType, Graph);
	SetNodeId(NewNode, OutNodeId);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return NewNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateCallFunctionNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& FunctionName, const FString& TargetClass, int32 PosX, int32 PosY, FString& OutError)
{
	if (FunctionName.IsEmpty())
	{
		OutError = TEXT("CallFunction requires node_params.function");
		return nullptr;
	}

	UClass* FunctionOwner = nullptr;
	if (!TargetClass.IsEmpty())
	{
		if (TargetClass.Equals(TEXT("KismetSystemLibrary"), ESearchCase::IgnoreCase))
			FunctionOwner = UKismetSystemLibrary::StaticClass();
		else if (TargetClass.Equals(TEXT("KismetMathLibrary"), ESearchCase::IgnoreCase))
			FunctionOwner = UKismetMathLibrary::StaticClass();
		else if (TargetClass.Equals(TEXT("GameplayStatics"), ESearchCase::IgnoreCase))
			FunctionOwner = UGameplayStatics::StaticClass();
		else
			// ResolveClass는 U-접두사/단축명까지 처리하므로 target_class='UserWidget' 같은 입력도 해석된다.
			FunctionOwner = ResolveClass(TargetClass);
	}

	UFunction* Function = nullptr;
	if (FunctionOwner)
		Function = FunctionOwner->FindFunctionByName(FName(*FunctionName));

	// self/상속 폴백: target_class 미지정 시 블루프린트 자기 클래스 계층에서 탐색.
	// 예) GetOwningPlayerState/GetOwningPlayer 등 부모 UUserWidget의 BlueprintCallable 함수.
	if (!Function && Blueprint)
	{
		UClass* SelfClass = Blueprint->GeneratedClass;
		if (!SelfClass) SelfClass = Blueprint->SkeletonGeneratedClass;
		if (!SelfClass) SelfClass = Blueprint->ParentClass;
		if (SelfClass)
			Function = SelfClass->FindFunctionByName(FName(*FunctionName));
	}

	// 전역 폴백 (공용 함수 라이브러리)
	if (!Function) Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName));
	if (!Function) Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName));
	if (!Function) Function = UGameplayStatics::StaticClass()->FindFunctionByName(FName(*FunctionName));

	if (!Function)
	{
		OutError = FString::Printf(TEXT("Function '%s' not found (target_class='%s')"), *FunctionName, *TargetClass);
		return nullptr;
	}

	// Array_Get/Array_Length/Array_Add 등 ArrayParm 메타 함수는 와일드카드 원소 타입 전파 로직
	// (PropagateArrayTypeInfo)을 가진 UK2Node_CallArrayFunction으로 생성해야 한다. plain
	// UK2Node_CallFunction으로 만들면 타입드 배열을 연결해도 와일드카드가 해석되지 않는다.
	if (Function->HasMetaData(TEXT("ArrayParm")))
	{
		FGraphNodeCreator<UK2Node_CallArrayFunction> NodeCreator(*Graph);
		UK2Node_CallArrayFunction* ArrayNode = NodeCreator.CreateNode();
		ArrayNode->SetFromFunction(Function);
		ArrayNode->NodePosX = PosX;
		ArrayNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return ArrayNode;
	}

	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* CallNode = NodeCreator.CreateNode();
	CallNode->SetFromFunction(Function);
	CallNode->NodePosX = PosX;
	CallNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return CallNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateEventNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& EventName, int32 PosX, int32 PosY, FString& OutError)
{
	UFunction* EventFunc = nullptr;

	if (EventName.Equals(TEXT("BeginPlay"), ESearchCase::IgnoreCase))
		EventFunc = AActor::StaticClass()->FindFunctionByName(FName(TEXT("ReceiveBeginPlay")));
	else if (EventName.Equals(TEXT("Tick"), ESearchCase::IgnoreCase))
		EventFunc = AActor::StaticClass()->FindFunctionByName(FName(TEXT("ReceiveTick")));
	else if (EventName.Equals(TEXT("EndPlay"), ESearchCase::IgnoreCase))
		EventFunc = AActor::StaticClass()->FindFunctionByName(FName(TEXT("ReceiveEndPlay")));
	else if (Blueprint && Blueprint->ParentClass)
		EventFunc = Blueprint->ParentClass->FindFunctionByName(FName(*EventName));

	if (!EventFunc)
	{
		OutError = FString::Printf(TEXT("Event '%s' not found on parent class"), *EventName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_Event> NodeCreator(*Graph);
	UK2Node_Event* EventNode = NodeCreator.CreateNode();
	EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
	EventNode->bOverrideFunction = true;
	EventNode->NodePosX = PosX;
	EventNode->NodePosY = PosY;
	NodeCreator.Finalize();

	return EventNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateVariableNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FString& TargetClass, bool bSetter, int32 PosX, int32 PosY, FString& OutError)
{
	if (VariableName.IsEmpty())
	{
		OutError = TEXT("Variable node requires node_params.variable");
		return nullptr;
	}

	const FName VarName(*VariableName);

	// ── 외부 멤버 경로 ──
	// target_class가 주어지면 외부 UObject의 멤버 프로퍼티(예: 데이터 에셋의 BlueprintReadOnly
	// UPROPERTY)를 읽고/쓴다. UE는 BlueprintReadOnly 프로퍼티를 getter 함수 없이도 Target 핀을
	// 가진 멤버 Get 노드로 노출하므로, SetExternalMember로 동일 노드를 생성한다.
	if (!TargetClass.IsEmpty())
	{
		UClass* OwnerClass = ResolveClass(TargetClass);
		if (!OwnerClass)
		{
			OutError = FString::Printf(TEXT("target_class '%s' could not be resolved to a UClass for variable '%s'."), *TargetClass, *VariableName);
			return nullptr;
		}

		FProperty* Prop = FindFProperty<FProperty>(OwnerClass, VarName);
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("Property '%s' not found on class '%s'."), *VariableName, *OwnerClass->GetName());
			return nullptr;
		}
		if (!Prop->HasAnyPropertyFlags(CPF_BlueprintVisible))
		{
			OutError = FString::Printf(TEXT("Property '%s' on '%s' is not Blueprint-visible (no BlueprintReadOnly/BlueprintReadWrite)."), *VariableName, *OwnerClass->GetName());
			return nullptr;
		}
		if (bSetter && Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
		{
			OutError = FString::Printf(TEXT("Property '%s' on '%s' is BlueprintReadOnly; cannot create a Set node. Use VariableGet instead."), *VariableName, *OwnerClass->GetName());
			return nullptr;
		}

		if (bSetter)
		{
			FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*Graph);
			UK2Node_VariableSet* SetNode = NodeCreator.CreateNode();
			SetNode->VariableReference.SetExternalMember(VarName, OwnerClass);
			SetNode->NodePosX = PosX;
			SetNode->NodePosY = PosY;
			NodeCreator.Finalize();
			return SetNode;
		}

		FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
		UK2Node_VariableGet* GetNode = NodeCreator.CreateNode();
		GetNode->VariableReference.SetExternalMember(VarName, OwnerClass);
		GetNode->NodePosX = PosX;
		GetNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return GetNode;
	}

	// ── Self 멤버 경로 (기존 동작) ──
	bool bFound = false;
	if (Blueprint)
	{
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName == VarName) { bFound = true; break; }
		}
		// NewVariables(사용자 선언 변수)에 없으면 컴파일된 클래스 프로퍼티에서 탐색.
		// UMG 위젯 트리 변수(bIsVariable인 ItemIcon 등), 상속 변수, 컴포넌트가 여기에 해당한다.
		// 위젯은 NewVariables가 아니라 (Skeleton)GeneratedClass의 FObjectProperty로 존재.
		if (!bFound)
		{
			UClass* VarClass = Blueprint->SkeletonGeneratedClass
				? Blueprint->SkeletonGeneratedClass.Get()
				: Blueprint->GeneratedClass.Get();
			if (VarClass && FindFProperty<FProperty>(VarClass, VarName))
				bFound = true;
		}
	}
	if (!bFound)
	{
		OutError = FString::Printf(TEXT("Variable '%s' not found in Blueprint (checked user variables + widget/inherited members). For an external object's property, pass target_class. For a UMG widget, enable 'Is Variable' on it first."), *VariableName);
		return nullptr;
	}

	if (bSetter)
	{
		FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*Graph);
		UK2Node_VariableSet* SetNode = NodeCreator.CreateNode();
		SetNode->VariableReference.SetSelfMember(VarName);
		SetNode->NodePosX = PosX;
		SetNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return SetNode;
	}

	FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
	UK2Node_VariableGet* GetNode = NodeCreator.CreateNode();
	GetNode->VariableReference.SetSelfMember(VarName);
	GetNode->NodePosX = PosX;
	GetNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return GetNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateBranchNode(UEdGraph* Graph, int32 PosX, int32 PosY)
{
	FGraphNodeCreator<UK2Node_IfThenElse> NodeCreator(*Graph);
	UK2Node_IfThenElse* BranchNode = NodeCreator.CreateNode();
	BranchNode->NodePosX = PosX;
	BranchNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return BranchNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateSequenceNode(UEdGraph* Graph, int32 PosX, int32 PosY)
{
	FGraphNodeCreator<UK2Node_ExecutionSequence> NodeCreator(*Graph);
	UK2Node_ExecutionSequence* SeqNode = NodeCreator.CreateNode();
	SeqNode->NodePosX = PosX;
	SeqNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return SeqNode;
}

UClass* FBlueprintGraphEditor::ResolveClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
		return nullptr;

	// 1) 경로 형태(/Script/... 또는 /Game/...): 직접 로드.
	if (ClassName.Contains(TEXT("/")))
	{
		// 클래스 오브젝트 경로 (예: /Script/Shooter.GachavivalCharacter, /Game/.../BP.BP_C)
		if (UClass* C = LoadObject<UClass>(nullptr, *ClassName))
			return C;
		// 블루프린트 에셋 경로 (예: /Game/.../BP_GachavivalCharacter) → GeneratedClass.
		if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassName))
			if (BP->GeneratedClass)
				return BP->GeneratedClass;
		// _C 접미사 없는 클래스 경로 보정: /Game/Dir/BP_X → /Game/Dir/BP_X.BP_X_C
		if (!ClassName.EndsWith(TEXT("_C")) && !ClassName.Contains(TEXT(".")))
		{
			int32 SlashIdx = INDEX_NONE;
			ClassName.FindLastChar(TEXT('/'), SlashIdx);
			const FString Leaf = (SlashIdx != INDEX_NONE) ? ClassName.RightChop(SlashIdx + 1) : ClassName;
			const FString GeneratedPath = FString::Printf(TEXT("%s.%s_C"), *ClassName, *Leaf);
			if (UClass* C = LoadObject<UClass>(nullptr, *GeneratedPath))
				return C;
		}
		return nullptr;
	}

	// 2) 단축명 (네이티브/로드된 클래스): TryFindTypeSlow가 접두사 유무를 모두 처리합니다.
	if (UClass* C = UClass::TryFindTypeSlow<UClass>(ClassName))
		return C;

	// 3) U/A/F 접두사 제거 후 재시도.
	FString Stripped = ClassName;
	if (Stripped.Len() > 1 && FChar::IsUpper(Stripped[1])
		&& (Stripped[0] == TEXT('U') || Stripped[0] == TEXT('A') || Stripped[0] == TEXT('F')))
	{
		Stripped.RightChopInline(1);
		if (UClass* C = UClass::TryFindTypeSlow<UClass>(Stripped))
			return C;
	}

	// 4) 블루프린트 생성 클래스 단축명 보정 (BP_X → BP_X_C).
	if (!ClassName.EndsWith(TEXT("_C")))
	{
		if (UClass* C = UClass::TryFindTypeSlow<UClass>(ClassName + TEXT("_C")))
			return C;
	}

	// 5) 최후 폴백.
	return FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
}

FMulticastDelegateProperty* FBlueprintGraphEditor::FindMulticastDelegateProperty(UClass* Owner, const FString& PropertyName)
{
	if (!Owner || PropertyName.IsEmpty())
		return nullptr;

	const FName PropName(*PropertyName);
	for (TFieldIterator<FMulticastDelegateProperty> It(Owner); It; ++It)
	{
		if (It->GetFName() == PropName)
			return *It;
	}
	return nullptr;
}

UEdGraphNode* FBlueprintGraphEditor::CreateCastNode(UEdGraph* Graph, const FString& TargetClass, int32 PosX, int32 PosY, FString& OutError)
{
	UClass* CastTo = ResolveClass(TargetClass);
	if (!CastTo)
	{
		OutError = FString::Printf(TEXT("Cast requires a valid target_class. '%s' not found."), *TargetClass);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*Graph);
	UK2Node_DynamicCast* CastNode = NodeCreator.CreateNode();
	CastNode->TargetType = CastTo;
	CastNode->SetPurity(false);
	CastNode->NodePosX = PosX;
	CastNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return CastNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateCustomEventNode(UEdGraph* Graph, const FString& EventName, int32 PosX, int32 PosY, FString& OutError)
{
	if (EventName.IsEmpty())
	{
		OutError = TEXT("CustomEvent requires 'event' (the custom event name)");
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*Graph);
	UK2Node_CustomEvent* EventNode = NodeCreator.CreateNode();
	EventNode->CustomFunctionName = FName(*EventName);
	EventNode->bIsEditable = true;
	EventNode->NodePosX = PosX;
	EventNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return EventNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateFunctionResultNode(UEdGraph* Graph, int32 PosX, int32 PosY, FString& OutError)
{
	// FunctionResult는 함수 그래프에만 의미가 있습니다.
	bool bHasEntry = false;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Cast<UK2Node_FunctionEntry>(Node)) { bHasEntry = true; break; }
	}
	if (!bHasEntry)
	{
		OutError = TEXT("FunctionResult can only be added to a function graph (no FunctionEntry found). Use is_function_graph=true and a function graph_name.");
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*Graph);
	UK2Node_FunctionResult* ResultNode = NodeCreator.CreateNode();
	ResultNode->NodePosX = PosX;
	ResultNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return ResultNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateComponentBoundEventNode(UEdGraph* Graph, UBlueprint* Blueprint,
	const FString& ComponentName, const FString& DelegateProperty, int32 PosX, int32 PosY, FString& OutError)
{
	if (ComponentName.IsEmpty() || DelegateProperty.IsEmpty())
	{
		OutError = TEXT("ComponentBoundEvent requires 'component' and 'delegate_property' (e.g. component=BtnOk, delegate_property=OnClicked)");
		return nullptr;
	}
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		OutError = TEXT("Blueprint has no generated class (compile it first)");
		return nullptr;
	}

	// 컴포넌트/위젯은 BP 생성 클래스의 ObjectProperty로 노출됩니다.
	UClass* OwnerClass = Blueprint->GeneratedClass;
	FObjectProperty* ComponentProp = FindFProperty<FObjectProperty>(OwnerClass, FName(*ComponentName));
	if (!ComponentProp)
	{
		OutError = FString::Printf(TEXT("Component/widget property '%s' not found on the Blueprint class"), *ComponentName);
		return nullptr;
	}

	FMulticastDelegateProperty* DelegateProp = FindMulticastDelegateProperty(ComponentProp->PropertyClass, DelegateProperty);
	if (!DelegateProp)
	{
		OutError = FString::Printf(TEXT("Multicast delegate '%s' not found on '%s'"), *DelegateProperty, *ComponentProp->PropertyClass->GetName());
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*Graph);
	UK2Node_ComponentBoundEvent* BoundNode = NodeCreator.CreateNode();
	BoundNode->InitializeComponentBoundEventParams(ComponentProp, DelegateProp);
	BoundNode->NodePosX = PosX;
	BoundNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return BoundNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateDelegateNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& NodeType,
	const FString& DelegateProperty, const FString& DelegateOwner, const FString& BoundFunction, int32 PosX, int32 PosY, FString& OutError)
{
	if (NodeType.Equals(TEXT("CreateDelegate"), ESearchCase::IgnoreCase))
	{
		FGraphNodeCreator<UK2Node_CreateDelegate> NodeCreator(*Graph);
		UK2Node_CreateDelegate* DelegateNode = NodeCreator.CreateNode();
		DelegateNode->NodePosX = PosX;
		DelegateNode->NodePosY = PosY;
		NodeCreator.Finalize();
		if (!BoundFunction.IsEmpty())
			DelegateNode->SetFunction(FName(*BoundFunction));
		return DelegateNode;
	}

	// AddDelegate / RemoveDelegate: 멀티캐스트 델리게이트 프로퍼티에 바인딩/해제.
	if (DelegateProperty.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s requires 'delegate_property'"), *NodeType);
		return nullptr;
	}

	// 소유 클래스: delegate_owner 지정 시 해석, 아니면 BP 자기 자신.
	UClass* OwnerClass = nullptr;
	if (DelegateOwner.IsEmpty())
		OwnerClass = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	else
		OwnerClass = ResolveClass(DelegateOwner);
	if (!OwnerClass)
	{
		OutError = TEXT("Could not resolve delegate owner class (compile the Blueprint or pass delegate_owner)");
		return nullptr;
	}

	FMulticastDelegateProperty* DelegateProp = FindMulticastDelegateProperty(OwnerClass, DelegateProperty);
	if (!DelegateProp)
	{
		OutError = FString::Printf(TEXT("Multicast delegate '%s' not found on '%s'"), *DelegateProperty, *OwnerClass->GetName());
		return nullptr;
	}

	const bool bSelfContext = DelegateOwner.IsEmpty();

	if (NodeType.Equals(TEXT("AddDelegate"), ESearchCase::IgnoreCase))
	{
		FGraphNodeCreator<UK2Node_AddDelegate> NodeCreator(*Graph);
		UK2Node_AddDelegate* AddNode = NodeCreator.CreateNode();
		AddNode->SetFromProperty(DelegateProp, bSelfContext, OwnerClass);
		AddNode->NodePosX = PosX;
		AddNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return AddNode;
	}

	FGraphNodeCreator<UK2Node_RemoveDelegate> NodeCreator(*Graph);
	UK2Node_RemoveDelegate* RemoveNode = NodeCreator.CreateNode();
	RemoveNode->SetFromProperty(DelegateProp, bSelfContext, OwnerClass);
	RemoveNode->NodePosX = PosX;
	RemoveNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return RemoveNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateSwitchEnumNode(UEdGraph* Graph, const FString& EnumName, int32 PosX, int32 PosY, FString& OutError)
{
	UEnum* Enum = ResolveEnum(EnumName);
	if (!Enum)
	{
		OutError = FString::Printf(TEXT("SwitchEnum requires a valid 'enum'. '%s' not found."), *EnumName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_SwitchEnum> NodeCreator(*Graph);
	UK2Node_SwitchEnum* SwitchNode = NodeCreator.CreateNode();
	SwitchNode->SetEnum(Enum);
	SwitchNode->NodePosX = PosX;
	SwitchNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return SwitchNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateBreakMakeStructNode(UEdGraph* Graph, const FString& StructName, bool bMake, int32 PosX, int32 PosY, FString& OutError)
{
	UScriptStruct* Struct = ResolveScriptStruct(StructName);
	if (!Struct)
	{
		OutError = FString::Printf(TEXT("%sStruct requires a valid 'struct_type'. '%s' not found."),
			bMake ? TEXT("Make") : TEXT("Break"), *StructName);
		return nullptr;
	}

	// StructType은 Finalize(AllocateDefaultPins) 전에 설정해야 멤버별 핀이 생성된다.
	if (bMake)
	{
		FGraphNodeCreator<UK2Node_MakeStruct> NodeCreator(*Graph);
		UK2Node_MakeStruct* MakeNode = NodeCreator.CreateNode();
		MakeNode->StructType = Struct;
		MakeNode->NodePosX = PosX;
		MakeNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return MakeNode;
	}

	FGraphNodeCreator<UK2Node_BreakStruct> NodeCreator(*Graph);
	UK2Node_BreakStruct* BreakNode = NodeCreator.CreateNode();
	BreakNode->StructType = Struct;
	BreakNode->NodePosX = PosX;
	BreakNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return BreakNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateFormatTextNode(UEdGraph* Graph, const FString& FormatString, int32 PosX, int32 PosY, FString& OutError)
{
	FGraphNodeCreator<UK2Node_FormatText> NodeCreator(*Graph);
	UK2Node_FormatText* FormatNode = NodeCreator.CreateNode();
	FormatNode->NodePosX = PosX;
	FormatNode->NodePosY = PosY;
	NodeCreator.Finalize();

	// Format 핀의 기본 텍스트를 설정하면 {0}/{Name} 토큰을 파싱해 인자 핀이 자동 생성된다.
	if (!FormatString.IsEmpty())
	{
		if (UEdGraphPin* FormatPin = FormatNode->FindPin(TEXT("Format")))
		{
			FormatPin->DefaultTextValue = FText::FromString(FormatString);
			// PinDefaultValueChanged가 SynchronizeArgumentsWithFormatText를 호출해 인자 핀을 동기화한다.
			FormatNode->PinDefaultValueChanged(FormatPin);
		}
	}
	return FormatNode;
}

bool FBlueprintGraphEditor::IsStandardMacroNodeType(const FString& NodeType)
{
	static const TCHAR* KnownMacros[] = {
		TEXT("ForLoop"), TEXT("ForLoopWithBreak"), TEXT("ForEachLoop"), TEXT("ForEachLoopWithBreak"),
		TEXT("WhileLoop"), TEXT("DoOnce"), TEXT("DoN"), TEXT("Gate"), TEXT("FlipFlop"), TEXT("MultiGate"), TEXT("IsValid")
	};
	for (const TCHAR* Name : KnownMacros)
	{
		if (NodeType.Equals(Name, ESearchCase::IgnoreCase))
			return true;
	}
	return false;
}

UEdGraph* FBlueprintGraphEditor::FindStandardMacroGraph(const FString& MacroName)
{
	if (MacroName.IsEmpty())
		return nullptr;

	UBlueprint* MacroBP = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
	if (!MacroBP)
		return nullptr;

	for (UEdGraph* MacroGraph : MacroBP->MacroGraphs)
	{
		if (MacroGraph && MacroGraph->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
			return MacroGraph;
	}
	return nullptr;
}

UEdGraphNode* FBlueprintGraphEditor::CreateMacroInstanceNode(UEdGraph* Graph, const FString& MacroName, int32 PosX, int32 PosY, FString& OutError)
{
	UEdGraph* MacroGraph = FindStandardMacroGraph(MacroName);
	if (!MacroGraph)
	{
		OutError = FString::Printf(TEXT("Standard macro '%s' not found (try ForLoop|ForLoopWithBreak|ForEachLoop|ForEachLoopWithBreak|WhileLoop|DoOnce|DoN|Gate|FlipFlop|MultiGate|IsValid)"), *MacroName);
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_MacroInstance> NodeCreator(*Graph);
	UK2Node_MacroInstance* MacroNode = NodeCreator.CreateNode();
	MacroNode->SetMacroGraph(MacroGraph);
	MacroNode->NodePosX = PosX;
	MacroNode->NodePosY = PosY;
	NodeCreator.Finalize();
	return MacroNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateWidgetNode(UEdGraph* Graph, const FString& WidgetClassName, int32 PosX, int32 PosY, FString& OutError)
{
	// UK2Node_CreateWidget은 UMGEditor의 Private 헤더라 모듈 외부에서 include할 수 없습니다.
	// 동등한 공개 API인 UWidgetBlueprintLibrary::Create를 CallFunction 노드로 배치합니다.
	// (WidgetType 핀이 DeterminesOutputType이라 클래스를 지정하면 반환 타입이 위젯 클래스로 좁혀집니다.)
	UFunction* CreateFunc = UWidgetBlueprintLibrary::StaticClass()->FindFunctionByName(FName(TEXT("Create")));
	if (!CreateFunc)
	{
		OutError = TEXT("UWidgetBlueprintLibrary::Create not found");
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* WidgetNode = NodeCreator.CreateNode();
	WidgetNode->SetFromFunction(CreateFunc);
	WidgetNode->NodePosX = PosX;
	WidgetNode->NodePosY = PosY;
	NodeCreator.Finalize();

	// 위젯 클래스 지정(옵션): WidgetType 입력 핀 기본값을 설정하면 반환 타입이 갱신됩니다.
	if (!WidgetClassName.IsEmpty())
	{
		UClass* WidgetClass = ResolveClass(WidgetClassName);
		if (!WidgetClass)
		{
			OutError = FString::Printf(TEXT("CreateWidget target_class '%s' not found"), *WidgetClassName);
			return nullptr;
		}
		if (UEdGraphPin* ClassPin = WidgetNode->FindPin(TEXT("WidgetType")))
		{
			ClassPin->DefaultObject = WidgetClass;
			WidgetNode->PinDefaultValueChanged(ClassPin);
		}
	}
	return WidgetNode;
}

UEdGraphNode* FBlueprintGraphEditor::CreateOverrideEventNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& EventName, int32 PosX, int32 PosY, FString& OutError)
{
	if (EventName.IsEmpty())
	{
		OutError = TEXT("OverrideEvent requires 'event' (the parent function/event name, e.g. OnMouseButtonDown)");
		return nullptr;
	}
	if (!Blueprint || !Blueprint->ParentClass)
	{
		OutError = TEXT("Blueprint has no parent class");
		return nullptr;
	}

	UFunction* OverrideFunc = Blueprint->ParentClass->FindFunctionByName(FName(*EventName));
	if (!OverrideFunc)
	{
		OutError = FString::Printf(TEXT("Overridable function/event '%s' not found on parent class '%s'"), *EventName, *Blueprint->ParentClass->GetName());
		return nullptr;
	}
	if (!UEdGraphSchema_K2::CanKismetOverrideFunction(OverrideFunc))
	{
		OutError = FString::Printf(TEXT("Function '%s' cannot be overridden in Blueprint"), *EventName);
		return nullptr;
	}

	// 반환값/출력 인자가 없으면 이벤트 그래프에 빨간 이벤트 노드로 배치 가능.
	if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(OverrideFunc))
	{
		FGraphNodeCreator<UK2Node_Event> NodeCreator(*Graph);
		UK2Node_Event* EventNode = NodeCreator.CreateNode();
		EventNode->EventReference.SetFromField<UFunction>(OverrideFunc, false);
		EventNode->bOverrideFunction = true;
		EventNode->NodePosX = PosX;
		EventNode->NodePosY = PosY;
		NodeCreator.Finalize();
		return EventNode;
	}

	// 반환/출력값이 있어 이벤트로 배치 불가 → 함수 오버라이드 그래프를 생성하고 진입 노드를 반환.
	const FName FuncFName(*EventName);
	for (UEdGraph* FG : Blueprint->FunctionGraphs)
	{
		if (FG && FG->GetFName() == FuncFName)
		{
			for (UEdGraphNode* N : FG->Nodes)
				if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(N))
					return Entry;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FuncFName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated*/false, OverrideFunc->GetOwnerClass());

	for (UEdGraphNode* N : NewGraph->Nodes)
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(N))
			return Entry;

	OutError = TEXT("Override function graph created but no entry node found");
	return nullptr;
}

//-----------------------------------------------------------------------------
// 함수 시그니처 편집
//-----------------------------------------------------------------------------

bool FBlueprintGraphEditor::MakePinType(const FString& TypeStr, FEdGraphPinType& OutPinType, FString& OutError)
{
	OutPinType = FEdGraphPinType();
	OutPinType.ContainerType = EPinContainerType::None;

	const FString T = TypeStr.TrimStartAndEnd();
	if (T.IsEmpty()) { OutError = TEXT("Empty param_type"); return false; }

	if (T.Equals(TEXT("bool"), ESearchCase::IgnoreCase) || T.Equals(TEXT("boolean"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (T.Equals(TEXT("int"), ESearchCase::IgnoreCase) || T.Equals(TEXT("int32"), ESearchCase::IgnoreCase) || T.Equals(TEXT("integer"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (T.Equals(TEXT("int64"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (T.Equals(TEXT("float"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (T.Equals(TEXT("double"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (T.Equals(TEXT("string"), ESearchCase::IgnoreCase) || T.Equals(TEXT("fstring"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (T.Equals(TEXT("name"), ESearchCase::IgnoreCase) || T.Equals(TEXT("fname"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (T.Equals(TEXT("text"), ESearchCase::IgnoreCase) || T.Equals(TEXT("ftext"), ESearchCase::IgnoreCase))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else
	{
		// 클래스(object) → 스트럭트 → enum 순으로 해석 시도.
		if (UClass* AsClass = ResolveClass(T))
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = AsClass;
		}
		else if (UScriptStruct* AsStruct = ResolveScriptStruct(T))
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			OutPinType.PinSubCategoryObject = AsStruct;
		}
		else if (UEnum* AsEnum = ResolveEnum(T))
		{
			// 블루프린트 enum 파라미터는 PC_Byte + enum 서브카테고리로 표현됩니다.
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			OutPinType.PinSubCategoryObject = AsEnum;
		}
		else
		{
			OutError = FString::Printf(TEXT("Unknown param_type '%s'. Use bool|int|int64|float|double|string|name|text|<ClassName>|<StructName>|<EnumName>"), *TypeStr);
			return false;
		}
	}
	return true;
}

UScriptStruct* FBlueprintGraphEditor::ResolveScriptStruct(const FString& StructName)
{
	if (StructName.IsEmpty())
		return nullptr;

	if (UScriptStruct* S = FindFirstObject<UScriptStruct>(*StructName, EFindFirstObjectOptions::None))
		return S;

	// F 접두사 제거 후 재시도 (리플렉션 이름은 접두사가 없음: FVector2D → Vector2D).
	if (StructName.Len() > 1 && StructName[0] == TEXT('F') && FChar::IsUpper(StructName[1]))
	{
		const FString Stripped = StructName.RightChop(1);
		if (UScriptStruct* S = FindFirstObject<UScriptStruct>(*Stripped, EFindFirstObjectOptions::None))
			return S;
	}
	return nullptr;
}

UEnum* FBlueprintGraphEditor::ResolveEnum(const FString& EnumName)
{
	if (EnumName.IsEmpty())
		return nullptr;

	// 단축명(접두사 유무 모두 처리) 우선, 실패 시 전역 검색 폴백.
	if (UEnum* E = UClass::TryFindTypeSlow<UEnum>(EnumName))
		return E;
	return FindFirstObject<UEnum>(*EnumName, EFindFirstObjectOptions::None);
}

bool FBlueprintGraphEditor::AddFunctionParam(UEdGraph* FuncGraph, const FString& ParamName, const FString& ParamType,
	bool bIsOutput, FString& OutError)
{
	if (!FuncGraph) { OutError = TEXT("Null graph"); return false; }
	if (ParamName.IsEmpty()) { OutError = TEXT("param_name is required"); return false; }

	FEdGraphPinType PinType;
	if (!MakePinType(ParamType, PinType, OutError))
		return false;

	UK2Node_EditablePinBase* TargetNode = nullptr;

	if (!bIsOutput)
	{
		// 입력 파라미터 → FunctionEntry의 출력 핀.
		for (UEdGraphNode* Node : FuncGraph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node)) { TargetNode = Entry; break; }
		}
		if (!TargetNode)
		{
			OutError = TEXT("No FunctionEntry node found. add_function_param requires a function graph (is_function_graph=true).");
			return false;
		}
	}
	else
	{
		// 출력(반환) → FunctionResult의 입력 핀. 없으면 생성.
		for (UEdGraphNode* Node : FuncGraph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node)) { TargetNode = Result; break; }
		}
		if (!TargetNode)
		{
			UEdGraphNode* Created = CreateFunctionResultNode(FuncGraph, 600, 0, OutError);
			if (!Created) return false;
			TargetNode = Cast<UK2Node_EditablePinBase>(Created);
		}
	}

	if (!TargetNode)
	{
		OutError = TEXT("Could not resolve target node for parameter");
		return false;
	}

	const EEdGraphPinDirection Direction = bIsOutput ? EGPD_Input : EGPD_Output;
	UEdGraphPin* NewPin = TargetNode->CreateUserDefinedPin(FName(*ParamName), PinType, Direction, /*bUseUniqueName*/true);
	if (!NewPin)
	{
		OutError = FString::Printf(TEXT("Failed to create user-defined pin '%s'"), *ParamName);
		return false;
	}

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(FuncGraph))
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool FBlueprintGraphEditor::AddCustomEventParam(UBlueprint* Blueprint, const FString& EventName,
	const FString& ParamName, const FString& ParamType, FString& OutError)
{
	if (!Blueprint) { OutError = TEXT("Null blueprint"); return false; }
	if (EventName.IsEmpty()) { OutError = TEXT("custom_event name is required"); return false; }
	if (ParamName.IsEmpty()) { OutError = TEXT("param_name is required"); return false; }

	FEdGraphPinType PinType;
	if (!MakePinType(ParamType, PinType, OutError))
		return false;

	// 모든 그래프(이벤트 그래프 페이지 포함)에서 이름이 일치하는 CustomEvent를 탐색.
	UK2Node_CustomEvent* Target = nullptr;
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	const FName WantName(*EventName);
	for (UEdGraph* G : AllGraphs)
	{
		if (!G) continue;
		for (UEdGraphNode* Node : G->Nodes)
		{
			if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
			{
				if (CE->CustomFunctionName == WantName) { Target = CE; break; }
			}
		}
		if (Target) break;
	}
	if (!Target)
	{
		OutError = FString::Printf(TEXT("CustomEvent '%s' not found in blueprint. Add it first with add_node node_type=CustomEvent."), *EventName);
		return false;
	}

	// CustomEvent의 파라미터는 이벤트 노드의 출력 핀입니다.
	UEdGraphPin* NewPin = Target->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output, /*bUseUniqueName*/true);
	if (!NewPin)
	{
		OutError = FString::Printf(TEXT("Failed to create user-defined pin '%s' on CustomEvent '%s'"), *ParamName, *EventName);
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

//-----------------------------------------------------------------------------
// 핀 연결 / 해제 / 기본값
//-----------------------------------------------------------------------------

bool FBlueprintGraphEditor::ConnectPins(UEdGraph* Graph, const FString& SourceNodeId, const FString& SourcePinName,
	const FString& TargetNodeId, const FString& TargetPinName, FString& OutError)
{
	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!SourceNode) { OutError = FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeId); return false; }
	if (!TargetNode) { OutError = FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeId); return false; }

	UEdGraphPin* SourcePin = SourcePinName.IsEmpty()
		? GetExecPin(SourceNode, true)
		: FindPinByName(SourceNode, SourcePinName, EGPD_Output);
	if (!SourcePin && !SourcePinName.IsEmpty())
		SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Input);
	if (!SourcePin) { OutError = FString::Printf(TEXT("Source pin '%s' not found on '%s'"), *SourcePinName, *SourceNodeId); return false; }

	UEdGraphPin* TargetPin = TargetPinName.IsEmpty()
		? GetExecPin(TargetNode, false)
		: FindPinByName(TargetNode, TargetPinName, EGPD_Input);
	if (!TargetPin && !TargetPinName.IsEmpty())
		TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Output);
	if (!TargetPin) { OutError = FString::Printf(TEXT("Target pin '%s' not found on '%s'"), *TargetPinName, *TargetNodeId); return false; }

	if (const UEdGraphSchema* Schema = Graph->GetSchema())
	{
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			OutError = FString::Printf(TEXT("Cannot connect pins: %s"), *Response.Message.ToString());
			return false;
		}

		// TryCreateConnection은 링크 생성 + 양쪽 노드의 PinConnectionListChanged 콜백 호출 +
		// 필요 시 변환 노드 삽입을 모두 수행합니다. 원시 MakeLinkTo는 콜백을 건너뛰어
		// Cast/와일드카드 노드가 핀 타입을 재해석하지 못합니다(Object 핀이 wildcard로 남는 버그).
		if (!Schema->TryCreateConnection(SourcePin, TargetPin))
		{
			OutError = FString::Printf(TEXT("Failed to connect '%s.%s' -> '%s.%s'"),
				*SourceNodeId, *SourcePin->PinName.ToString(), *TargetNodeId, *TargetPin->PinName.ToString());
			return false;
		}
	}
	else
	{
		SourcePin->MakeLinkTo(TargetPin);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));
	return true;
}

bool FBlueprintGraphEditor::DisconnectPins(UEdGraph* Graph, const FString& SourceNodeId, const FString& SourcePinName,
	const FString& TargetNodeId, const FString& TargetPinName, FString& OutError)
{
	UEdGraphNode* SourceNode = FindNodeById(Graph, SourceNodeId);
	UEdGraphNode* TargetNode = FindNodeById(Graph, TargetNodeId);
	if (!SourceNode) { OutError = FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeId); return false; }
	if (!TargetNode) { OutError = FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeId); return false; }

	UEdGraphPin* SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_MAX);
	UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_MAX);
	if (!SourcePin) { OutError = FString::Printf(TEXT("Pin '%s' not found on '%s'"), *SourcePinName, *SourceNodeId); return false; }
	if (!TargetPin) { OutError = FString::Printf(TEXT("Pin '%s' not found on '%s'"), *TargetPinName, *TargetNodeId); return false; }

	SourcePin->BreakLinkTo(TargetPin);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));
	return true;
}

bool FBlueprintGraphEditor::SetPinDefaultValue(UEdGraph* Graph, const FString& NodeId, const FString& PinName, const FString& Value, FString& OutError)
{
	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) { OutError = FString::Printf(TEXT("Node '%s' not found"), *NodeId); return false; }

	UEdGraphPin* Pin = FindPinByName(Node, PinName, EGPD_Input);
	if (!Pin) { OutError = FString::Printf(TEXT("Input pin '%s' not found on '%s'"), *PinName, *NodeId); return false; }

	if (const UEdGraphSchema* Schema = Graph->GetSchema())
		Schema->TrySetDefaultValue(*Pin, Value);
	else
		Pin->DefaultValue = Value;

	FBlueprintEditorUtils::MarkBlueprintAsModified(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));
	return true;
}

bool FBlueprintGraphEditor::DeleteNode(UEdGraph* Graph, const FString& NodeId, FString& OutError)
{
	UEdGraphNode* Node = FindNodeById(Graph, NodeId);
	if (!Node) { OutError = FString::Printf(TEXT("Node '%s' not found"), *NodeId); return false; }

	Node->BreakAllNodeLinks();
	Graph->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));
	return true;
}

//-----------------------------------------------------------------------------
// 핀 / 노드 조회
//-----------------------------------------------------------------------------

UEdGraphPin* FBlueprintGraphEditor::FindPinByName(UEdGraphNode* Node, const FString& PinName, int32 Direction)
{
	if (!Node) return nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			if (Direction == EGPD_MAX || Pin->Direction == static_cast<EEdGraphPinDirection>(Direction))
				return Pin;
		}
	}
	return nullptr;
}

UEdGraphPin* FBlueprintGraphEditor::GetExecPin(UEdGraphNode* Node, bool bOutput)
{
	if (!Node) return nullptr;
	const EEdGraphPinDirection Direction = bOutput ? EGPD_Output : EGPD_Input;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			return Pin;
	}
	return nullptr;
}

UEdGraphNode* FBlueprintGraphEditor::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
	if (!Graph || NodeId.IsEmpty()) return nullptr;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && GetNodeId(Node) == NodeId)
			return Node;
	}

	FGuid ParsedGuid;
	if (FGuid::Parse(NodeId, ParsedGuid))
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == ParsedGuid)
				return Node;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FBlueprintGraphEditor::SerializeNodeInfo(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
	if (!Node) return NodeObj;

	const FString Id = GetNodeId(Node);
	NodeObj->SetStringField(TEXT("node_id"), Id.IsEmpty() ? Node->NodeGuid.ToString() : Id);
	NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
	NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);

	TArray<TSharedPtr<FJsonValue>> Pins;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
		PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
		PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
		PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
		if (!Pin->DefaultValue.IsEmpty())
			PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
		PinObj->SetNumberField(TEXT("connections"), Pin->LinkedTo.Num());
		Pins.Add(MakeShared<FJsonValueObject>(PinObj));
	}
	NodeObj->SetArrayField(TEXT("pins"), Pins);
	return NodeObj;
}

FString FBlueprintGraphEditor::ListNodes(UEdGraph* Graph)
{
	TArray<TSharedPtr<FJsonValue>> NodeArray;
	if (Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
				NodeArray.Add(MakeShared<FJsonValueObject>(SerializeNodeInfo(Node)));
		}
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(NodeArray, Writer);
	return Out;
}

//-----------------------------------------------------------------------------
// 컴포넌트 추가
//-----------------------------------------------------------------------------

namespace
{
	// 컴포넌트 타입 문자열을 UActorComponent 파생 UClass로 해석합니다. 실패 시 nullptr.
	UClass* ResolveComponentClass(const FString& TypeName)
	{
		if (TypeName.IsEmpty())
			return nullptr;

		// "U" 접두사가 붙어 와도 허용 (UClass 오브젝트명은 접두사 없음).
		FString Name = TypeName;
		if (Name.StartsWith(TEXT("U")) && Name.Len() > 1 && FChar::IsUpper(Name[1]))
			Name.RightChopInline(1);

		UClass* Found = FindFirstObject<UClass>(*Name, EFindFirstObjectOptions::None);
		if (Found && Found->IsChildOf(UActorComponent::StaticClass()) && !Found->HasAnyClassFlags(CLASS_Abstract))
			return Found;

		return nullptr;
	}
}

bool FBlueprintGraphEditor::AddComponent(UBlueprint* Blueprint, const FString& ComponentType, const FString& ComponentName,
	const FString& StaticMeshPath, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Null blueprint");
		return false;
	}

	// 1) 컴포넌트 클래스 해석 (검증 먼저, 실패 시 즉시 명확한 에러).
	UClass* ComponentClass = ResolveComponentClass(ComponentType);
	if (!ComponentClass)
	{
		OutError = FString::Printf(TEXT("Unknown or non-component class '%s'. Use a UActorComponent subclass name like StaticMeshComponent."), *ComponentType);
		return false;
	}

	// 2) 메시 경로가 있으면 컴포넌트 추가 전에 먼저 로드 검증 (부분 상태 방지).
	UStaticMesh* Mesh = nullptr;
	if (!StaticMeshPath.IsEmpty())
	{
		if (!ComponentClass->IsChildOf(UStaticMeshComponent::StaticClass()))
		{
			OutError = FString::Printf(TEXT("static_mesh given but '%s' is not a StaticMeshComponent."), *ComponentType);
			return false;
		}
		Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *StaticMeshPath));
		if (!Mesh)
		{
			OutError = FString::Printf(TEXT("Static mesh not found: %s"), *StaticMeshPath);
			return false;
		}
	}

	// 3) 서브시스템 + 루트 핸들 확보.
	USubobjectDataSubsystem* Subsystem = USubobjectDataSubsystem::Get();
	if (!Subsystem)
	{
		OutError = TEXT("SubobjectDataSubsystem unavailable");
		return false;
	}

	TArray<FSubobjectDataHandle> Handles;
	Subsystem->K2_GatherSubobjectDataForBlueprint(Blueprint, Handles);
	if (Handles.Num() == 0)
	{
		OutError = TEXT("Failed to gather blueprint subobjects");
		return false;
	}

	// 4) 컴포넌트 추가.
	FAddNewSubobjectParams Params;
	Params.ParentHandle = Handles[0];
	Params.NewClass = ComponentClass;
	Params.BlueprintContext = Blueprint;

	FText FailReason;
	FSubobjectDataHandle NewHandle = Subsystem->AddNewSubobject(Params, FailReason);
	if (!NewHandle.IsValid())
	{
		OutError = FString::Printf(TEXT("AddNewSubobject failed: %s"), *FailReason.ToString());
		return false;
	}

	// 5) 이름 지정 (옵션).
	if (!ComponentName.IsEmpty())
		Subsystem->RenameSubobject(NewHandle, FText::FromString(ComponentName));

	// 6) 메시 설정 (옵션). 컴포넌트 템플릿은 public const 접근자로 얻어 const_cast 후 설정합니다.
	if (Mesh)
	{
		const FSubobjectData* Data = NewHandle.GetData();
		const UStaticMeshComponent* ConstSMC = Data ? Data->GetObjectForBlueprint<UStaticMeshComponent>(Blueprint) : nullptr;
		if (UStaticMeshComponent* SMC = const_cast<UStaticMeshComponent*>(ConstSMC))
			SMC->SetStaticMesh(Mesh);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

//-----------------------------------------------------------------------------
// 변수 / 블루프린트 생성
//-----------------------------------------------------------------------------

bool FBlueprintGraphEditor::AddVariable(UBlueprint* Blueprint, const FString& VarName, const FString& VarType,
	const FString& DefaultValue, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Null blueprint");
		return false;
	}
	if (VarName.IsEmpty())
	{
		OutError = TEXT("add_variable requires 'variable_name'");
		return false;
	}

	const FName VarFName(*VarName);

	// 이름 중복 검사 (이미 있으면 AddMemberVariable이 조용히 실패하므로 먼저 명확히 거른다).
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarFName)
		{
			OutError = FString::Printf(TEXT("Variable '%s' already exists"), *VarName);
			return false;
		}
	}

	FEdGraphPinType PinType;
	if (!MakePinType(VarType, PinType, OutError))
		return false;

	if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarFName, PinType, DefaultValue))
	{
		OutError = FString::Printf(TEXT("Failed to add variable '%s' (%s)"), *VarName, *VarType);
		return false;
	}

	return true;
}

UBlueprint* FBlueprintGraphEditor::CreateBlueprint(const FString& PackagePath, const FString& BlueprintName,
	const FString& ParentClassName, FString& OutError)
{
	if (PackagePath.IsEmpty() || BlueprintName.IsEmpty())
	{
		OutError = TEXT("create_blueprint requires 'package_path' and 'blueprint_name'");
		return nullptr;
	}

	// 부모 클래스 해석 (네이티브 단축명 / 경로 / BP 경로 모두 지원).
	UClass* ParentClass = ResolveClass(ParentClassName);
	if (!ParentClass)
	{
		OutError = FString::Printf(TEXT("Parent class not found: '%s'"), *ParentClassName);
		return nullptr;
	}

	// 패키지 경로 정규화: 선행 '/' 없으면 /Game/ 보정, 엔진/스크립트 경로 차단.
	FString Folder = PackagePath;
	if (!Folder.StartsWith(TEXT("/")))
		Folder = TEXT("/Game/") + Folder;
	Folder.RemoveFromEnd(TEXT("/"));
	if (Folder.StartsWith(TEXT("/Engine/")) || Folder.StartsWith(TEXT("/Script/")))
	{
		OutError = TEXT("Cannot create blueprints under /Engine or /Script.");
		return nullptr;
	}

	const FString FullPath = Folder / BlueprintName;

	// 중복 에셋 검사.
	if (StaticLoadObject(UBlueprint::StaticClass(), nullptr, *FullPath))
	{
		OutError = FString::Printf(TEXT("Blueprint already exists: %s"), *FullPath);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
		return nullptr;
	}

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
		UBlueprint::StaticClass(), Package, FName(*BlueprintName),
		RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));

	if (!NewBlueprint)
	{
		OutError = FString::Printf(TEXT("Factory failed to create Blueprint with parent '%s'"), *ParentClass->GetName());
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(NewBlueprint);
	NewBlueprint->MarkPackageDirty();
	return NewBlueprint;
}

//-----------------------------------------------------------------------------
// 컴파일 / 저장
//-----------------------------------------------------------------------------

FString FBlueprintGraphEditor::CompileAndSave(UBlueprint* Blueprint, bool& bOutSuccess)
{
	bOutSuccess = false;
	if (!Blueprint) return TEXT("No blueprint");

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// 컴파일 메시지 수집
	FString Messages;
	auto ProcessGraph = [&](UEdGraph* Graph)
	{
		if (!Graph) return;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->bHasCompilerMessage && !Node->ErrorMsg.IsEmpty())
			{
				const TCHAR* Sev = Node->ErrorType == EMessageSeverity::Error ? TEXT("Error") : TEXT("Warning");
				Messages += FString::Printf(TEXT("\n[%s] %s: %s"), Sev,
					*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Node->ErrorMsg);
			}
		}
	};
	for (UEdGraph* G : Blueprint->UbergraphPages) ProcessGraph(G);
	for (UEdGraph* G : Blueprint->FunctionGraphs) ProcessGraph(G);

	Blueprint->MarkPackageDirty();

	switch (Blueprint->Status)
	{
	case BS_UpToDate:
		bOutSuccess = true;
		return TEXT("Compiled: UpToDate") + Messages;
	case BS_UpToDateWithWarnings:
		bOutSuccess = true;
		return TEXT("Compiled with warnings") + Messages;
	case BS_Error:
		bOutSuccess = false;
		return TEXT("Compilation FAILED") + Messages;
	default:
		bOutSuccess = false;
		return FString::Printf(TEXT("Compile status %d"), static_cast<int32>(Blueprint->Status)) + Messages;
	}
}

//-----------------------------------------------------------------------------
// 노드 ID
//-----------------------------------------------------------------------------

FString FBlueprintGraphEditor::GenerateNodeId(const FString& NodeType, UEdGraph* Graph)
{
	int32 Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
	FString NodeId = FString::Printf(TEXT("%s_%d"), *NodeType, Counter);
	while (Graph && FindNodeById(Graph, NodeId) != nullptr)
	{
		Counter = FPlatformAtomics::InterlockedIncrement(&NodeIdCounter);
		NodeId = FString::Printf(TEXT("%s_%d"), *NodeType, Counter);
	}
	return NodeId;
}

void FBlueprintGraphEditor::SetNodeId(UEdGraphNode* Node, const FString& NodeId)
{
	if (Node)
	{
		Node->NodeComment = NodeIdPrefix + NodeId;
		Node->bCommentBubbleVisible = false;
	}
}

FString FBlueprintGraphEditor::GetNodeId(UEdGraphNode* Node)
{
	if (Node && Node->NodeComment.StartsWith(NodeIdPrefix))
		return Node->NodeComment.RightChop(FCString::Strlen(NodeIdPrefix));
	return FString();
}
