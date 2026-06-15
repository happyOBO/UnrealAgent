#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "BlueprintModifyTool.generated.h"

/**
 * 블루프린트 노드 그래프를 편집하는 MCP 도구입니다.
 *
 * Python `unreal` API로 불가능한 이벤트 그래프 편집(노드 추가, 핀 연결, 기본값 설정)을
 * 네이티브로 수행합니다. 모든 연산 파라미터를 flat 스칼라로 선언하여 MCP inputSchema에
 * 노출하므로, 모델이 평탄한 인자를 정확히 전달합니다(중첩 객체 직렬화 불일치 방지).
 * 연산별로 필요한 필드만 채우면 됩니다.
 */
USTRUCT(meta=(McpTool="blueprint_modify"))
struct FBlueprintModifyTool : public FMcpTool
{
	GENERATED_BODY()

	/** 수행할 연산: add_node | add_component | connect_pins | disconnect_pins | set_pin_value | delete_node | list_nodes */
	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: add_node, add_component, connect_pins, disconnect_pins, set_pin_value, delete_node, list_nodes"))
	FString Operation;

	/** 대상 블루프린트 경로 (예: /Game/BP_Door) */
	UPROPERTY(meta=(ToolParam="blueprint_path", Required,
		Description="Blueprint asset path, e.g. /Game/Blueprints/BP_Door"))
	FString BlueprintPath;

	// ── 그래프 선택 (옵션, 모든 연산) ──

	UPROPERTY(meta=(ToolParam="graph_name",
		Description="Target graph name. Empty = first event graph (or first function graph)."))
	FString GraphName;

	UPROPERTY(meta=(ToolParam="is_function_graph",
		Description="If true, target a function graph instead of the event graph."))
	bool bIsFunctionGraph = false;

	// ── add_node ──

	UPROPERTY(meta=(ToolParam="node_type",
		Description="[add_node] CallFunction | Event | VariableGet | VariableSet | Branch | Sequence"))
	FString NodeType;

	UPROPERTY(meta=(ToolParam="function",
		Description="[add_node CallFunction] Function name, e.g. PrintString"))
	FString Function;

	UPROPERTY(meta=(ToolParam="target_class",
		Description="[add_node CallFunction] Owning class: KismetSystemLibrary | KismetMathLibrary | GameplayStatics | <ClassName>"))
	FString TargetClass;

	UPROPERTY(meta=(ToolParam="event",
		Description="[add_node Event] Event name: BeginPlay | Tick | EndPlay | <ParentFunction>"))
	FString Event;

	UPROPERTY(meta=(ToolParam="variable",
		Description="[add_node VariableGet/VariableSet] Blueprint variable name"))
	FString Variable;

	UPROPERTY(meta=(ToolParam="num_outputs",
		Description="[add_node Sequence] Number of output exec pins"))
	int32 NumOutputs = 0;

	UPROPERTY(meta=(ToolParam="pos_x", Description="[add_node] Node X position"))
	int32 PosX = 0;

	UPROPERTY(meta=(ToolParam="pos_y", Description="[add_node] Node Y position"))
	int32 PosY = 0;

	// ── connect_pins / disconnect_pins ──

	UPROPERTY(meta=(ToolParam="source_node",
		Description="[connect_pins/disconnect_pins] Source node id"))
	FString SourceNode;

	UPROPERTY(meta=(ToolParam="source_pin",
		Description="[connect_pins/disconnect_pins] Source pin name. Empty = auto-detect output exec pin."))
	FString SourcePin;

	UPROPERTY(meta=(ToolParam="target_node",
		Description="[connect_pins/disconnect_pins] Target node id"))
	FString TargetNode;

	UPROPERTY(meta=(ToolParam="target_pin",
		Description="[connect_pins/disconnect_pins] Target pin name. Empty = auto-detect input exec pin."))
	FString TargetPin;

	// ── add_component ──

	UPROPERTY(meta=(ToolParam="component_type",
		Description="[add_component] UActorComponent subclass name, e.g. StaticMeshComponent"))
	FString ComponentType;

	UPROPERTY(meta=(ToolParam="component_name",
		Description="[add_component] Name for the new component (optional)"))
	FString ComponentName;

	UPROPERTY(meta=(ToolParam="static_mesh",
		Description="[add_component] Static mesh asset path to assign (only for StaticMeshComponent; optional)"))
	FString StaticMesh;

	// ── set_pin_value / delete_node ──

	UPROPERTY(meta=(ToolParam="node_id",
		Description="[set_pin_value/delete_node] Target node id"))
	FString NodeId;

	UPROPERTY(meta=(ToolParam="pin",
		Description="[set_pin_value] Input pin name"))
	FString Pin;

	UPROPERTY(meta=(ToolParam="value",
		Description="[set_pin_value] New default value (string; parsed by pin type)"))
	FString Value;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
