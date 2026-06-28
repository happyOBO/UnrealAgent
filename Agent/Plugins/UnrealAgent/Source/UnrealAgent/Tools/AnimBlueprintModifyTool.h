#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "AnimBlueprintModifyTool.generated.h"

/**
 * Animation Blueprint(스테이트 머신)을 편집하는 MCP 도구입니다.
 *
 * Python `unreal` API로 불가능한 스테이트 머신 골격 구성을 네이티브로 수행합니다.
 * 최소 기능: 스테이트 머신/스테이트/전이 생성, 스테이트 애님 연결, 진입 스테이트 설정.
 */
USTRUCT(meta=(McpTool="anim_blueprint_modify"))
struct FAnimBlueprintModifyTool : public FMcpTool
{
	GENERATED_BODY()

	/** state machine ops, AnimGraph 노드 ops, 또는 읽기 전용 조회 */
	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: get_anim_graph, create_state_machine, add_state, add_transition, set_state_animation, set_entry_state, add_slot_node, add_layered_blend_per_bone, add_aim_offset_node, add_modify_bone, connect_anim_nodes, expose_pin, add_variable_get, add_graph_node, connect, set_pin_default, delete_node, set_transition_auto_rule"))
	FString Operation;

	/** 대상 Animation Blueprint 경로 (예: /Game/Characters/ABP_Hero) */
	UPROPERTY(meta=(ToolParam="blueprint_path", Required,
		Description="Animation Blueprint asset path"))
	FString BlueprintPath;

	// ── 연산 파라미터 (flat, 연산별로 필요한 것만 채움) ──

	UPROPERTY(meta=(ToolParam="state_machine",
		Description="State machine name (all operations)"))
	FString StateMachine;

	UPROPERTY(meta=(ToolParam="state_name",
		Description="[add_state/set_state_animation/set_entry_state] State name"))
	FString StateName;

	UPROPERTY(meta=(ToolParam="from_state",
		Description="[add_transition] Source state name"))
	FString FromState;

	UPROPERTY(meta=(ToolParam="to_state",
		Description="[add_transition] Target state name"))
	FString ToState;

	UPROPERTY(meta=(ToolParam="anim_sequence",
		Description="[set_state_animation] Anim sequence asset path"))
	FString AnimSequence;

	UPROPERTY(meta=(ToolParam="pos_x", Description="[create_state_machine/add_state/add_slot_node/add_layered_blend_per_bone] X position"))
	int32 PosX = 0;

	UPROPERTY(meta=(ToolParam="pos_y", Description="[create_state_machine/add_state/add_slot_node/add_layered_blend_per_bone] Y position"))
	int32 PosY = 0;

	// ── AnimGraph 노드 ops 파라미터 ──

	UPROPERTY(meta=(ToolParam="slot_name",
		Description="[add_slot_node] Montage slot name to play (e.g. DefaultSlot)"))
	FString SlotName;

	UPROPERTY(meta=(ToolParam="bones",
		Description="[add_layered_blend_per_bone] Comma-separated bone names for the blend layer (e.g. spine_01,spine_02)"))
	FString Bones;

	UPROPERTY(meta=(ToolParam="aim_offset_path",
		Description="[add_aim_offset_node] AimOffset (UAimOffsetBlendSpace) asset path to play"))
	FString AimOffsetPath;

	UPROPERTY(meta=(ToolParam="bone",
		Description="[add_modify_bone] Bone to modify (e.g. spine_01). The node's Rotation pin is shown by default."))
	FString Bone;

	UPROPERTY(meta=(ToolParam="rotation_mode",
		Description="[add_modify_bone] Ignore|Replace|Add (default Add)"))
	FString RotationMode;

	UPROPERTY(meta=(ToolParam="rotation_space",
		Description="[add_modify_bone] World|Component|Parent|Bone (default Component)"))
	FString RotationSpace;

	UPROPERTY(meta=(ToolParam="from_node_id",
		Description="[connect_anim_nodes] Source node id (NodeGuid returned by add_*_node)"))
	FString FromNodeId;

	UPROPERTY(meta=(ToolParam="from_pin",
		Description="[connect_anim_nodes] Source output pin name (optional; default first output pin)"))
	FString FromPin;

	UPROPERTY(meta=(ToolParam="to_node_id",
		Description="[connect_anim_nodes] Target node id, or 'output'/'result' for the Output Pose"))
	FString ToNodeId;

	UPROPERTY(meta=(ToolParam="to_pin",
		Description="[connect_anim_nodes/connect] Target input pin name (optional; default first input pin)"))
	FString ToPin;

	// ── 핀 노출 / 서브그래프 범용 편집 ops 파라미터 ──
	// 범용 편집 ops(add_graph_node/connect/set_pin_default/add_variable_get)는 from_state+to_state가
	// 둘 다 주어지면 그 전이의 조건 그래프를, 아니면 메인 AnimGraph를 편집 대상으로 합니다.

	UPROPERTY(meta=(ToolParam="node_id",
		Description="[expose_pin/set_pin_default] Target node id (NodeGuid or UA_ID)"))
	FString NodeId;

	UPROPERTY(meta=(ToolParam="pin_name",
		Description="[expose_pin] property to expose as a pin (AimOffset: X=Yaw, Y=Pitch); [set_pin_default] input pin name"))
	FString PinName;

	UPROPERTY(meta=(ToolParam="expose",
		Description="[expose_pin] true to expose the property as a pin, false to hide (default true)"))
	bool bExpose = true;

	UPROPERTY(meta=(ToolParam="variable",
		Description="[add_variable_get] AnimInstance member variable to read (e.g. AimYaw)"))
	FString Variable;

	UPROPERTY(meta=(ToolParam="node_type",
		Description="[add_graph_node] node type: CallFunction|VariableGet|Branch|Cast|... (same as blueprint_modify add_node)"))
	FString NodeType;

	UPROPERTY(meta=(ToolParam="node_params",
		Description="[add_graph_node] JSON object string of node params (e.g. {\"function\":\"Greater_DoubleDouble\",\"target_class\":\"KismetMathLibrary\"})"))
	FString NodeParams;

	UPROPERTY(meta=(ToolParam="value",
		Description="[set_pin_default] default value to set on the input pin"))
	FString Value;

	UPROPERTY(meta=(ToolParam="enable",
		Description="[set_transition_auto_rule] true to enable 'transition when sequence finishes' (default true)"))
	bool bEnable = true;

	UPROPERTY(meta=(ToolParam="trigger_time",
		Description="[set_transition_auto_rule] automatic rule trigger time in seconds; negative = leave unchanged (default -1)"))
	float TriggerTime = -1.f;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
