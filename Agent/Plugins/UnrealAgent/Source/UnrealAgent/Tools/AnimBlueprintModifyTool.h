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
		Description="One of: get_anim_graph, create_state_machine, add_state, add_transition, set_state_animation, set_entry_state, add_slot_node, add_layered_blend_per_bone, connect_anim_nodes"))
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
		Description="[connect_anim_nodes] Target input pin name (optional; default first input pin)"))
	FString ToPin;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
