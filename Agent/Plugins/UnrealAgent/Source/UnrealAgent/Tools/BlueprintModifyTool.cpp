#include "Tools/BlueprintModifyTool.h"
#include "Blueprint/BlueprintGraphEditor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Dom/JsonObject.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(BlueprintModifyTool)

FString FBlueprintModifyTool::ToolDescription() const
{
	return TEXT(
		"Edit a Blueprint's node graph natively (operations Python cannot do).\n"
		"After any modifying op the blueprint is compiled and marked dirty.\n"
		"All parameters are flat (no nested objects). Fill only the fields an operation needs:\n"
		"\n"
		"- add_node: node_type + node fields, [pos_x], [pos_y], [graph_name], [is_function_graph]\n"
		"    node_type = CallFunction | Event | VariableGet | VariableSet | Branch | Sequence\n"
		"    CallFunction: function, [target_class] (KismetSystemLibrary|KismetMathLibrary|GameplayStatics|<Class>)\n"
		"    Event: event (BeginPlay|Tick|EndPlay|<ParentFunc>)\n"
		"    VariableGet/VariableSet: variable\n"
		"    Sequence: [num_outputs]\n"
		"    Returns the created node_id (use it for connect_pins/set_pin_value).\n"
		"- add_component: component_type (e.g. StaticMeshComponent), [component_name], [static_mesh asset path]\n"
		"    Adds a component to the Blueprint's component hierarchy. Use this instead of Python\n"
		"    SubobjectDataSubsystem for components. For a static mesh, pass static_mesh to set it in one call.\n"
		"- connect_pins: source_node, source_pin, target_node, target_pin (empty pin name = exec pin auto)\n"
		"- disconnect_pins: source_node, source_pin, target_node, target_pin\n"
		"- set_pin_value: node_id, pin, value\n"
		"- delete_node: node_id\n"
		"- list_nodes: [graph_name], [is_function_graph] (read-only; returns node/pin JSON)\n"
		"\n"
		"Node IDs returned by add_node/list_nodes are stable handles. Existing nodes can also\n"
		"be referenced by their NodeGuid string.");
}

FMcpResponse FBlueprintModifyTool::Execute()
{
	if (Operation.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'operation'"));
	if (BlueprintPath.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'blueprint_path'"));

	FString Error;
	UBlueprint* Blueprint = FBlueprintGraphEditor::LoadBlueprint(BlueprintPath, Error);
	if (!Blueprint)
		return FMcpResponse::Failure(Error);

	// ── add_component: 그래프가 필요 없는 연산 (컴포넌트 계층 수정) ──
	if (Operation.Equals(TEXT("add_component"), ESearchCase::IgnoreCase))
	{
		if (ComponentType.IsEmpty())
			return FMcpResponse::Failure(TEXT("add_component requires 'component_type'"));

		if (!FBlueprintGraphEditor::AddComponent(Blueprint, ComponentType, ComponentName, StaticMesh, Error))
			return FMcpResponse::Failure(Error);

		bool bComp = false;
		const FString Msg = FBlueprintGraphEditor::CompileAndSave(Blueprint, bComp);
		const FString Out = FString::Printf(TEXT("Added %s%s.\n"), *ComponentType,
			ComponentName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" '%s'"), *ComponentName)) + Msg;
		return bComp ? FMcpResponse::Success(Out) : FMcpResponse::Failure(Out);
	}

	UEdGraph* Graph = FBlueprintGraphEditor::FindGraph(Blueprint, GraphName, bIsFunctionGraph, Error);
	if (!Graph)
		return FMcpResponse::Failure(Error);

	// ── list_nodes: 읽기 전용 ──
	if (Operation.Equals(TEXT("list_nodes"), ESearchCase::IgnoreCase))
	{
		return FMcpResponse::Success(FBlueprintGraphEditor::ListNodes(Graph));
	}

	FString ResultSummary;

	if (Operation.Equals(TEXT("add_node"), ESearchCase::IgnoreCase))
	{
		if (NodeType.IsEmpty())
			return FMcpResponse::Failure(TEXT("add_node requires 'node_type'"));

		// 선언된 flat 필드로부터 노드 파라미터를 조립합니다.
		const TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
		if (!Function.IsEmpty())    NodeParams->SetStringField(TEXT("function"), Function);
		if (!TargetClass.IsEmpty()) NodeParams->SetStringField(TEXT("target_class"), TargetClass);
		if (!Event.IsEmpty())       NodeParams->SetStringField(TEXT("event"), Event);
		if (!Variable.IsEmpty())    NodeParams->SetStringField(TEXT("variable"), Variable);
		if (NumOutputs > 0)         NodeParams->SetNumberField(TEXT("num_outputs"), NumOutputs);

		FString OutNodeId;
		UEdGraphNode* Node = FBlueprintGraphEditor::CreateNode(Graph, Blueprint, NodeType, NodeParams, PosX, PosY, OutNodeId, Error);
		if (!Node) return FMcpResponse::Failure(Error);
		ResultSummary = FString::Printf(TEXT("Added %s node. node_id=%s"), *NodeType, *OutNodeId);
	}
	else if (Operation.Equals(TEXT("connect_pins"), ESearchCase::IgnoreCase))
	{
		if (!FBlueprintGraphEditor::ConnectPins(Graph, SourceNode, SourcePin, TargetNode, TargetPin, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = TEXT("Pins connected.");
	}
	else if (Operation.Equals(TEXT("disconnect_pins"), ESearchCase::IgnoreCase))
	{
		if (!FBlueprintGraphEditor::DisconnectPins(Graph, SourceNode, SourcePin, TargetNode, TargetPin, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = TEXT("Pins disconnected.");
	}
	else if (Operation.Equals(TEXT("set_pin_value"), ESearchCase::IgnoreCase))
	{
		if (!FBlueprintGraphEditor::SetPinDefaultValue(Graph, NodeId, Pin, Value, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = TEXT("Pin default value set.");
	}
	else if (Operation.Equals(TEXT("delete_node"), ESearchCase::IgnoreCase))
	{
		if (!FBlueprintGraphEditor::DeleteNode(Graph, NodeId, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = TEXT("Node deleted.");
	}
	else
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Unknown operation '%s'"), *Operation));
	}

	// 컴파일 + 저장 마크
	bool bCompiled = false;
	const FString CompileMsg = FBlueprintGraphEditor::CompileAndSave(Blueprint, bCompiled);

	const FString Full = ResultSummary + TEXT("\n") + CompileMsg;
	return bCompiled ? FMcpResponse::Success(Full) : FMcpResponse::Failure(Full);
}
