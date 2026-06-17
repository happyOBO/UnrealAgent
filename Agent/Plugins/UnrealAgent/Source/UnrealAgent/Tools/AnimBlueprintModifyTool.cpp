#include "Tools/AnimBlueprintModifyTool.h"
#include "Anim/AnimBlueprintEditor.h"
#include "Animation/AnimBlueprint.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimBlueprintModifyTool)

FString FAnimBlueprintModifyTool::ToolDescription() const
{
	return TEXT(
		"Edit/inspect an Animation Blueprint natively (operations Python cannot do).\n"
		"After any write op the AnimBP is compiled and marked dirty. Parameters are flat.\n"
		"\n"
		"- get_anim_graph: (read-only) lists main AnimGraph nodes with type, node_id (NodeGuid),\n"
		"    slot_name/state_machine, and pin connections. Use this instead of Python introspection.\n"
		"\n"
		"- create_state_machine: state_machine, [pos_x], [pos_y]\n"
		"    Also connects the state machine output to the AnimGraph Output Pose.\n"
		"- add_state: state_machine, state_name, [pos_x], [pos_y]\n"
		"- add_transition: state_machine, from_state, to_state\n"
		"    (transition is created with an empty condition graph; set conditions later)\n"
		"- set_state_animation: state_machine, state_name, anim_sequence (asset path)\n"
		"- set_entry_state: state_machine, state_name\n"
		"\n"
		"AnimGraph node ops (main AnimGraph, not state machine). add_* returns a node id (NodeGuid):\n"
		"- add_slot_node: slot_name, [pos_x], [pos_y] -> plays a montage slot\n"
		"- add_layered_blend_per_bone: bones (comma-separated), [pos_x], [pos_y]\n"
		"- connect_anim_nodes: from_node_id, to_node_id (or 'output'/'result' for Output Pose),\n"
		"    [from_pin], [to_pin] (pose pins; default first output/input pin)\n"
		"\n"
		"Typical flow: create_state_machine -> add_state (x2) -> set_state_animation each\n"
		"-> set_entry_state -> add_transition.");
}

FMcpResponse FAnimBlueprintModifyTool::Execute()
{
	if (Operation.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'operation'"));
	if (BlueprintPath.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'blueprint_path'"));

	FString Error;
	UAnimBlueprint* AnimBP = FAnimBlueprintEditor::LoadAnimBlueprint(BlueprintPath, Error);
	if (!AnimBP)
		return FMcpResponse::Failure(Error);

	// 읽기 전용 조회: 컴파일/저장 없이 조기 반환합니다.
	if (Operation.Equals(TEXT("get_anim_graph"), ESearchCase::IgnoreCase))
	{
		const FString Info = FAnimBlueprintEditor::GetAnimGraphInfo(AnimBP, Error);
		if (!Error.IsEmpty())
			return FMcpResponse::Failure(Error);
		return FMcpResponse::Success(Info);
	}

	FString Summary;

	if (Operation.Equals(TEXT("create_state_machine"), ESearchCase::IgnoreCase))
	{
		if (StateMachine.IsEmpty()) return FMcpResponse::Failure(TEXT("create_state_machine requires 'state_machine'"));
		if (!FAnimBlueprintEditor::CreateStateMachine(AnimBP, StateMachine, PosX, PosY, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Created state machine '%s'."), *StateMachine);
	}
	else if (Operation.Equals(TEXT("add_state"), ESearchCase::IgnoreCase))
	{
		if (StateMachine.IsEmpty() || StateName.IsEmpty()) return FMcpResponse::Failure(TEXT("add_state requires 'state_machine' and 'state_name'"));
		if (!FAnimBlueprintEditor::AddState(AnimBP, StateMachine, StateName, PosX, PosY, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added state '%s'."), *StateName);
	}
	else if (Operation.Equals(TEXT("add_transition"), ESearchCase::IgnoreCase))
	{
		if (StateMachine.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty()) return FMcpResponse::Failure(TEXT("add_transition requires 'state_machine', 'from_state', 'to_state'"));
		if (!FAnimBlueprintEditor::AddTransition(AnimBP, StateMachine, FromState, ToState, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added transition %s -> %s."), *FromState, *ToState);
	}
	else if (Operation.Equals(TEXT("set_state_animation"), ESearchCase::IgnoreCase))
	{
		if (StateMachine.IsEmpty() || StateName.IsEmpty() || AnimSequence.IsEmpty()) return FMcpResponse::Failure(TEXT("set_state_animation requires 'state_machine', 'state_name', 'anim_sequence'"));
		if (!FAnimBlueprintEditor::SetStateAnimation(AnimBP, StateMachine, StateName, AnimSequence, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Set animation of '%s' to %s."), *StateName, *AnimSequence);
	}
	else if (Operation.Equals(TEXT("set_entry_state"), ESearchCase::IgnoreCase))
	{
		if (StateMachine.IsEmpty() || StateName.IsEmpty()) return FMcpResponse::Failure(TEXT("set_entry_state requires 'state_machine' and 'state_name'"));
		if (!FAnimBlueprintEditor::SetEntryState(AnimBP, StateMachine, StateName, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Set entry state to '%s'."), *StateName);
	}
	else if (Operation.Equals(TEXT("add_slot_node"), ESearchCase::IgnoreCase))
	{
		if (SlotName.IsEmpty()) return FMcpResponse::Failure(TEXT("add_slot_node requires 'slot_name'"));
		FString NodeId;
		if (!FAnimBlueprintEditor::AddSlotNode(AnimBP, SlotName, PosX, PosY, NodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added slot node '%s'. node_id=%s"), *SlotName, *NodeId);
	}
	else if (Operation.Equals(TEXT("add_layered_blend_per_bone"), ESearchCase::IgnoreCase))
	{
		TArray<FString> BoneList;
		Bones.ParseIntoArray(BoneList, TEXT(","), true);
		for (FString& Bone : BoneList) { Bone = Bone.TrimStartAndEnd(); }
		BoneList.RemoveAll([](const FString& B) { return B.IsEmpty(); });

		FString NodeId;
		if (!FAnimBlueprintEditor::AddLayeredBlendPerBone(AnimBP, BoneList, PosX, PosY, NodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added LayeredBlendPerBone (%d bone(s)). node_id=%s"), BoneList.Num(), *NodeId);
	}
	else if (Operation.Equals(TEXT("connect_anim_nodes"), ESearchCase::IgnoreCase))
	{
		if (FromNodeId.IsEmpty() || ToNodeId.IsEmpty()) return FMcpResponse::Failure(TEXT("connect_anim_nodes requires 'from_node_id' and 'to_node_id'"));
		if (!FAnimBlueprintEditor::ConnectAnimNodes(AnimBP, FromNodeId, FromPin, ToNodeId, ToPin, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Connected %s -> %s."), *FromNodeId, *ToNodeId);
	}
	else
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Unknown operation '%s'"), *Operation));
	}

	bool bCompiled = false;
	const FString CompileMsg = FAnimBlueprintEditor::CompileAndSave(AnimBP, bCompiled);
	const FString Full = Summary + TEXT("\n") + CompileMsg;
	return bCompiled ? FMcpResponse::Success(Full) : FMcpResponse::Failure(Full);
}
