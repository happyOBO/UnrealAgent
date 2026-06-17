#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "MontageModifyTool.generated.h"

/**
 * AnimMontage 에셋을 편집하는 MCP 도구입니다.
 *
 * Python `unreal` API로 불가능한 슬롯 트랙(SlotAnimTracks) 조작을 네이티브로 수행합니다.
 * 기능: 몽타주 생성, 슬롯 추가/이름 변경, 세그먼트 추가, 구성 조회.
 */
USTRUCT(meta=(McpTool="montage_modify"))
struct FMontageModifyTool : public FMcpTool
{
	GENERATED_BODY()

	/** create | get_info | rename_slot | add_slot | add_segment */
	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: create, get_info, rename_slot, add_slot, add_segment"))
	FString Operation;

	/** 대상 몽타주 경로 (예: /Game/Characters/AM_Attack) */
	UPROPERTY(meta=(ToolParam="montage_path", Required,
		Description="AnimMontage asset path"))
	FString MontagePath;

	UPROPERTY(meta=(ToolParam="skeleton",
		Description="[create] Skeleton asset path (required unless anim_sequence given)"))
	FString Skeleton;

	UPROPERTY(meta=(ToolParam="anim_sequence",
		Description="[create/add_segment] AnimSequence asset path"))
	FString AnimSequence;

	UPROPERTY(meta=(ToolParam="slot_name",
		Description="[rename_slot/add_slot/add_segment] Slot name (for rename: the current name)"))
	FString SlotName;

	UPROPERTY(meta=(ToolParam="new_slot_name",
		Description="[rename_slot] New slot name"))
	FString NewSlotName;

	UPROPERTY(meta=(ToolParam="start_time",
		Description="[add_segment] Segment start position in seconds (default 0)"))
	float StartTime = 0.0f;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
