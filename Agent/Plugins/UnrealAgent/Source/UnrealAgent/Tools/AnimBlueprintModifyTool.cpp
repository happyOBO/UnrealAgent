#include "Tools/AnimBlueprintModifyTool.h"
#include "Anim/AnimBlueprintEditor.h"
#include "Animation/AnimBlueprint.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
		"- add_aim_offset_node: aim_offset_path, [pos_x], [pos_y] -> plays an AimOffset.\n"
		"    Wire its base pose with connect_anim_nodes (its first input pose pin is the Base Pose).\n"
		"    Bind its Yaw/Pitch via expose_pin (X/Y) + add_variable_get + connect (see below).\n"
		"- add_modify_bone: bone, [rotation_mode=Add], [rotation_space=Component], [pos_x],[pos_y]\n"
		"    -> Transform(Modify)Bone node (component-space spine twist without extra pose assets).\n"
		"    Its 'Rotation' pin is shown by default; drive it with MakeStruct(Rotator)+connect.\n"
		"- connect_anim_nodes: from_node_id, to_node_id (or 'output'/'result' for Output Pose),\n"
		"    [from_pin], [to_pin] (pose pins; default first output/input pin). Schema-based: auto-\n"
		"    inserts a Local/Component-space conversion node when pose pin spaces differ (like the\n"
		"    editor), and replaces an existing single-parent pose link.\n"
		"- delete_node: node_id [from_state, to_state] -> removes a node from the main AnimGraph\n"
		"    (or a transition condition graph). For plain nodes, not state/transition nodes.\n"
		"\n"
		"Bind a value pin (e.g. AimOffset Yaw/Pitch from an AnimInstance variable):\n"
		"- expose_pin: node_id, pin_name, [expose] -> shows an optional property as a graph pin.\n"
		"    AimOffset exposable pins: 'X' (Yaw), 'Y' (Pitch). Required before wiring them.\n"
		"- add_variable_get: variable, [pos_x], [pos_y] -> VariableGet node; node_id returned.\n"
		"    Then connect: from_pin = the variable name (the getter's data output).\n"
		"  Flow: add_aim_offset_node -> expose_pin(node,X)/expose_pin(node,Y)\n"
		"        -> add_variable_get(AimYaw) -> connect(getter, X pin of aim node).\n"
		"\n"
		"Generic graph editing (targets a transition CONDITION graph when from_state+to_state are\n"
		"given, else the main AnimGraph). Reuses the blueprint K2 node editor:\n"
		"- add_graph_node: node_type, [node_params JSON], [from_state],[to_state] -> node_id.\n"
		"    node_type/node_params match blueprint_modify add_node (CallFunction, VariableGet, Branch...).\n"
		"- connect: from_node_id, to_node_id ('result' = TransitionResult.bCanEnterTransition),\n"
		"    [from_pin],[to_pin],[from_state],[to_state]. Schema-checked (handles value pins).\n"
		"- set_pin_default: node_id, pin_name, value, [from_state],[to_state].\n"
		"- set_transition_auto_rule: state_machine, from_state, to_state, [enable],[trigger_time]\n"
		"    -> transition fires when its state's sequence finishes (no condition graph needed).\n"
		"  Condition flow: add_transition -> add_graph_node(VariableGet bIsTurningInPlace, from/to)\n"
		"        -> connect(getter, 'result', from/to). Use comparison CallFunctions + Branch for\n"
		"        richer expressions (e.g. AimYaw > 90 via Greater_DoubleDouble).\n"
		"\n"
		"Typical flow: create_state_machine -> add_state (x2) -> set_state_animation each\n"
		"-> set_entry_state -> add_transition -> (set condition via connect/add_graph_node).");
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
		FString NewNodeId;
		if (!FAnimBlueprintEditor::AddSlotNode(AnimBP, SlotName, PosX, PosY, NewNodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added slot node '%s'. node_id=%s"), *SlotName, *NewNodeId);
	}
	else if (Operation.Equals(TEXT("add_layered_blend_per_bone"), ESearchCase::IgnoreCase))
	{
		TArray<FString> BoneList;
		Bones.ParseIntoArray(BoneList, TEXT(","), true);
		for (FString& BoneEntry : BoneList) { BoneEntry = BoneEntry.TrimStartAndEnd(); }
		BoneList.RemoveAll([](const FString& B) { return B.IsEmpty(); });

		FString NewNodeId;
		if (!FAnimBlueprintEditor::AddLayeredBlendPerBone(AnimBP, BoneList, PosX, PosY, NewNodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added LayeredBlendPerBone (%d bone(s)). node_id=%s"), BoneList.Num(), *NewNodeId);
	}
	else if (Operation.Equals(TEXT("add_aim_offset_node"), ESearchCase::IgnoreCase))
	{
		if (AimOffsetPath.IsEmpty()) return FMcpResponse::Failure(TEXT("add_aim_offset_node requires 'aim_offset_path'"));
		FString NewNodeId;
		if (!FAnimBlueprintEditor::AddAimOffsetNode(AnimBP, AimOffsetPath, PosX, PosY, NewNodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added AimOffset node '%s'. node_id=%s"), *AimOffsetPath, *NewNodeId);
	}
	else if (Operation.Equals(TEXT("add_modify_bone"), ESearchCase::IgnoreCase))
	{
		if (Bone.IsEmpty()) return FMcpResponse::Failure(TEXT("add_modify_bone requires 'bone'"));
		FString NewNodeId;
		if (!FAnimBlueprintEditor::AddModifyBoneNode(AnimBP, Bone, RotationMode, RotationSpace, PosX, PosY, NewNodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added ModifyBone node for '%s'. node_id=%s"), *Bone, *NewNodeId);
	}
	else if (Operation.Equals(TEXT("connect_anim_nodes"), ESearchCase::IgnoreCase))
	{
		if (FromNodeId.IsEmpty() || ToNodeId.IsEmpty()) return FMcpResponse::Failure(TEXT("connect_anim_nodes requires 'from_node_id' and 'to_node_id'"));
		if (!FAnimBlueprintEditor::ConnectAnimNodes(AnimBP, FromNodeId, FromPin, ToNodeId, ToPin, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Connected %s -> %s."), *FromNodeId, *ToNodeId);
	}
	else if (Operation.Equals(TEXT("expose_pin"), ESearchCase::IgnoreCase))
	{
		if (NodeId.IsEmpty() || PinName.IsEmpty()) return FMcpResponse::Failure(TEXT("expose_pin requires 'node_id' and 'pin_name'"));
		if (!FAnimBlueprintEditor::ExposeNodePin(AnimBP, NodeId, PinName, bExpose, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("%s pin '%s' on node %s."), bExpose ? TEXT("Exposed") : TEXT("Hid"), *PinName, *NodeId);
	}
	else if (Operation.Equals(TEXT("add_variable_get"), ESearchCase::IgnoreCase))
	{
		if (Variable.IsEmpty()) return FMcpResponse::Failure(TEXT("add_variable_get requires 'variable'"));
		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("variable"), Variable);
		FString NewNodeId;
		if (!FAnimBlueprintEditor::AddGraphNode(AnimBP, TEXT("VariableGet"), Params, StateMachine, FromState, ToState, PosX, PosY, NewNodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added VariableGet '%s'. node_id=%s"), *Variable, *NewNodeId);
	}
	else if (Operation.Equals(TEXT("add_graph_node"), ESearchCase::IgnoreCase))
	{
		if (NodeType.IsEmpty()) return FMcpResponse::Failure(TEXT("add_graph_node requires 'node_type'"));
		TSharedPtr<FJsonObject> Params;
		if (!NodeParams.IsEmpty())
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NodeParams);
			if (!FJsonSerializer::Deserialize(Reader, Params) || !Params.IsValid())
				return FMcpResponse::Failure(TEXT("node_params must be a valid JSON object string"));
		}
		FString NewNodeId;
		if (!FAnimBlueprintEditor::AddGraphNode(AnimBP, NodeType, Params, StateMachine, FromState, ToState, PosX, PosY, NewNodeId, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Added %s node. node_id=%s"), *NodeType, *NewNodeId);
	}
	else if (Operation.Equals(TEXT("connect"), ESearchCase::IgnoreCase))
	{
		if (FromNodeId.IsEmpty() || ToNodeId.IsEmpty()) return FMcpResponse::Failure(TEXT("connect requires 'from_node_id' and 'to_node_id'"));
		if (!FAnimBlueprintEditor::ConnectGraphPins(AnimBP, FromNodeId, FromPin, ToNodeId, ToPin, StateMachine, FromState, ToState, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Connected %s -> %s."), *FromNodeId, *ToNodeId);
	}
	else if (Operation.Equals(TEXT("set_pin_default"), ESearchCase::IgnoreCase))
	{
		if (NodeId.IsEmpty() || PinName.IsEmpty()) return FMcpResponse::Failure(TEXT("set_pin_default requires 'node_id' and 'pin_name'"));
		if (!FAnimBlueprintEditor::SetGraphPinDefault(AnimBP, NodeId, PinName, Value, StateMachine, FromState, ToState, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Set pin '%s' = '%s' on node %s."), *PinName, *Value, *NodeId);
	}
	else if (Operation.Equals(TEXT("delete_node"), ESearchCase::IgnoreCase))
	{
		if (NodeId.IsEmpty()) return FMcpResponse::Failure(TEXT("delete_node requires 'node_id'"));
		if (!FAnimBlueprintEditor::DeleteGraphNode(AnimBP, NodeId, StateMachine, FromState, ToState, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Deleted node %s."), *NodeId);
	}
	else if (Operation.Equals(TEXT("set_transition_auto_rule"), ESearchCase::IgnoreCase))
	{
		if (StateMachine.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty()) return FMcpResponse::Failure(TEXT("set_transition_auto_rule requires 'state_machine', 'from_state', 'to_state'"));
		if (!FAnimBlueprintEditor::SetTransitionAutoRule(AnimBP, StateMachine, FromState, ToState, bEnable, TriggerTime, Error))
			return FMcpResponse::Failure(Error);
		Summary = FString::Printf(TEXT("Set auto rule (%s) on transition %s -> %s."), bEnable ? TEXT("enabled") : TEXT("disabled"), *FromState, *ToState);
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
