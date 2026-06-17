#include "Tools/MontageModifyTool.h"
#include "Anim/MontageEditor.h"
#include "Animation/AnimMontage.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(MontageModifyTool)

FString FMontageModifyTool::ToolDescription() const
{
	return TEXT(
		"Edit an AnimMontage asset natively (slot tracks Python cannot touch).\n"
		"Parameters are flat. The montage is marked dirty after each op.\n"
		"\n"
		"- create: montage_path, skeleton or anim_sequence, [anim_sequence]\n"
		"    Creates a new montage; if anim_sequence is given, fills the default slot with it.\n"
		"- get_info: montage_path -> lists slots and segments (JSON-like)\n"
		"- add_slot: montage_path, slot_name\n"
		"- rename_slot: montage_path, slot_name (current), new_slot_name\n"
		"- add_segment: montage_path, slot_name, anim_sequence, [start_time]");
}

FMcpResponse FMontageModifyTool::Execute()
{
	if (Operation.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'operation'"));
	if (MontagePath.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'montage_path'"));

	FString Error;

	// create는 로드가 아니라 생성이므로 별도 처리합니다.
	if (Operation.Equals(TEXT("create"), ESearchCase::IgnoreCase))
	{
		UAnimMontage* Created = FMontageEditor::CreateMontage(MontagePath, Skeleton, AnimSequence, Error);
		if (!Created)
			return FMcpResponse::Failure(Error);
		return FMcpResponse::Success(FString::Printf(TEXT("Created montage '%s'.\n%s"),
			*MontagePath, *FMontageEditor::GetInfo(Created)));
	}

	UAnimMontage* Montage = FMontageEditor::LoadMontage(MontagePath, Error);
	if (!Montage)
		return FMcpResponse::Failure(Error);

	FString Summary;

	if (Operation.Equals(TEXT("get_info"), ESearchCase::IgnoreCase))
	{
		return FMcpResponse::Success(FMontageEditor::GetInfo(Montage));
	}
	else if (Operation.Equals(TEXT("add_slot"), ESearchCase::IgnoreCase))
	{
		if (SlotName.IsEmpty()) return FMcpResponse::Failure(TEXT("add_slot requires 'slot_name'"));
		if (!FMontageEditor::AddSlot(Montage, SlotName, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added slot '%s'."), *SlotName);
	}
	else if (Operation.Equals(TEXT("rename_slot"), ESearchCase::IgnoreCase))
	{
		if (SlotName.IsEmpty() || NewSlotName.IsEmpty()) return FMcpResponse::Failure(TEXT("rename_slot requires 'slot_name' and 'new_slot_name'"));
		if (!FMontageEditor::RenameSlot(Montage, SlotName, NewSlotName, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Renamed slot '%s' -> '%s'."), *SlotName, *NewSlotName);
	}
	else if (Operation.Equals(TEXT("add_segment"), ESearchCase::IgnoreCase))
	{
		if (SlotName.IsEmpty() || AnimSequence.IsEmpty()) return FMcpResponse::Failure(TEXT("add_segment requires 'slot_name' and 'anim_sequence'"));
		if (!FMontageEditor::AddSegment(Montage, SlotName, AnimSequence, StartTime, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added segment %s to slot '%s'."), *AnimSequence, *SlotName);
	}
	else
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Unknown operation '%s'"), *Operation));
	}

	const FString Full = Summary + TEXT("\n") + FMontageEditor::GetInfo(Montage);
	return FMcpResponse::Success(Full);
}
