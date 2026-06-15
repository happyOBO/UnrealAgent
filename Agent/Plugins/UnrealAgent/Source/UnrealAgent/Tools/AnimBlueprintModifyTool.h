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

	/** create_state_machine | add_state | add_transition | set_state_animation | set_entry_state */
	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: create_state_machine, add_state, add_transition, set_state_animation, set_entry_state"))
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

	UPROPERTY(meta=(ToolParam="pos_x", Description="[create_state_machine/add_state] X position"))
	int32 PosX = 0;

	UPROPERTY(meta=(ToolParam="pos_y", Description="[create_state_machine/add_state] Y position"))
	int32 PosY = 0;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
