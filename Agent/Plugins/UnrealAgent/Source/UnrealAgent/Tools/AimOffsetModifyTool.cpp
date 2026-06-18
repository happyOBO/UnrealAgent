#include "Tools/AimOffsetModifyTool.h"
#include "Anim/AimOffsetEditor.h"
#include "Animation/AimOffsetBlendSpace.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AimOffsetModifyTool)

FString FAimOffsetModifyTool::ToolDescription() const
{
	return TEXT(
		"Create/edit an AimOffset (UAimOffsetBlendSpace) asset natively (BlendSpace samples Python cannot touch).\n"
		"Parameters are flat. The asset is marked dirty after each op.\n"
		"\n"
		"- create: aim_offset_path, skeleton\n"
		"    Creates a new AimOffset with the default Yaw/Pitch axes for the given skeleton.\n"
		"- add_sample: aim_offset_path, anim_sequence (additive pose), yaw, pitch\n"
		"    Adds an aim pose sample at the (yaw, pitch) grid coordinate; the axis range is expanded if needed.\n"
		"- get_info: aim_offset_path -> lists axes and samples (JSON-like)\n"
		"\n"
		"To use the AimOffset in an Animation Blueprint, add a node with anim_blueprint_modify add_aim_offset_node\n"
		"and wire its base pose with connect_anim_nodes. Axis range customization is not supported here (editor only).");
}

FMcpResponse FAimOffsetModifyTool::Execute()
{
	if (Operation.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'operation'"));
	if (AimOffsetPath.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'aim_offset_path'"));

	FString Error;

	// create는 로드가 아니라 생성이므로 별도 처리합니다.
	if (Operation.Equals(TEXT("create"), ESearchCase::IgnoreCase))
	{
		UAimOffsetBlendSpace* Created = FAimOffsetEditor::CreateAimOffset(AimOffsetPath, Skeleton, Error);
		if (!Created)
			return FMcpResponse::Failure(Error);
		return FMcpResponse::Success(FString::Printf(TEXT("Created aim offset '%s'.\n%s"),
			*AimOffsetPath, *FAimOffsetEditor::GetInfo(Created)));
	}

	UAimOffsetBlendSpace* AimOffset = FAimOffsetEditor::LoadAimOffset(AimOffsetPath, Error);
	if (!AimOffset)
		return FMcpResponse::Failure(Error);

	FString Summary;

	if (Operation.Equals(TEXT("get_info"), ESearchCase::IgnoreCase))
	{
		return FMcpResponse::Success(FAimOffsetEditor::GetInfo(AimOffset));
	}
	else if (Operation.Equals(TEXT("add_sample"), ESearchCase::IgnoreCase))
	{
		if (AnimSequence.IsEmpty()) return FMcpResponse::Failure(TEXT("add_sample requires 'anim_sequence'"));
		if (!FAimOffsetEditor::AddSample(AimOffset, AnimSequence, Yaw, Pitch, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added sample %s at (yaw=%.2f, pitch=%.2f)."), *AnimSequence, Yaw, Pitch);
	}
	else
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Unknown operation '%s'"), *Operation));
	}

	const FString Full = Summary + TEXT("\n") + FAimOffsetEditor::GetInfo(AimOffset);
	return FMcpResponse::Success(Full);
}
