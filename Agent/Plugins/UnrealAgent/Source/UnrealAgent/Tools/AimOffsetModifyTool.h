#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "AimOffsetModifyTool.generated.h"

/**
 * AimOffset(UAimOffsetBlendSpace) 에셋을 편집하는 MCP 도구입니다.
 *
 * Python `unreal` API로 불가능한 BlendSpace 샘플 조작을 네이티브로 수행합니다.
 * 기능: aim offset 에셋 생성, Yaw/Pitch 샘플 추가, 구성 조회.
 */
USTRUCT(meta=(McpTool="aim_offset_modify"))
struct FAimOffsetModifyTool : public FMcpTool
{
	GENERATED_BODY()

	/** create | add_sample | get_info */
	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: create, add_sample, get_info"))
	FString Operation;

	/** 대상 AimOffset 경로 (예: /Game/Characters/AO_Aim) */
	UPROPERTY(meta=(ToolParam="aim_offset_path", Required,
		Description="AimOffset (UAimOffsetBlendSpace) asset path"))
	FString AimOffsetPath;

	UPROPERTY(meta=(ToolParam="skeleton",
		Description="[create] Skeleton asset path (required for create)"))
	FString Skeleton;

	UPROPERTY(meta=(ToolParam="anim_sequence",
		Description="[add_sample] Additive AnimSequence asset path for this aim pose"))
	FString AnimSequence;

	UPROPERTY(meta=(ToolParam="yaw",
		Description="[add_sample] Yaw (X axis) value of the sample, degrees"))
	float Yaw = 0.0f;

	UPROPERTY(meta=(ToolParam="pitch",
		Description="[add_sample] Pitch (Y axis) value of the sample, degrees"))
	float Pitch = 0.0f;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
