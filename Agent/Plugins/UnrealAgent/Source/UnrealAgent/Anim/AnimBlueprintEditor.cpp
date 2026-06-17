#include "Anim/AnimBlueprintEditor.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateGraphSchema.h"
#include "AnimationTransitionGraph.h"
#include "AnimationTransitionSchema.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectGlobals.h"

//-----------------------------------------------------------------------------
// 로드 / 조회
//-----------------------------------------------------------------------------

UAnimBlueprint* FAnimBlueprintEditor::LoadAnimBlueprint(const FString& BlueprintPath, FString& OutError)
{
	UObject* Loaded = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath);
	if (!Loaded && !BlueprintPath.StartsWith(TEXT("/")))
		Loaded = StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *(TEXT("/Game/") + BlueprintPath));

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Loaded);
	if (!AnimBP)
	{
		OutError = FString::Printf(TEXT("Animation Blueprint not found: %s"), *BlueprintPath);
		return nullptr;
	}

	const UPackage* Package = AnimBP->GetPackage();
	if (Package && Package->HasAnyPackageFlags(PKG_Cooked))
	{
		OutError = TEXT("Cooked Animation Blueprint cannot be modified.");
		return nullptr;
	}
	return AnimBP;
}

UEdGraph* FAnimBlueprintEditor::FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError)
{
	if (!AnimBP) { OutError = TEXT("Null AnimBlueprint"); return nullptr; }

	TArray<UEdGraph*> Graphs;
	AnimBP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (UAnimationGraph* AG = Cast<UAnimationGraph>(Graph))
			return AG;
	}
	OutError = TEXT("Animation Blueprint has no AnimGraph");
	return nullptr;
}

UAnimGraphNode_StateMachine* FAnimBlueprintEditor::FindStateMachine(UAnimBlueprint* AnimBP, const FString& Name, FString& OutError)
{
	TArray<UEdGraph*> Graphs;
	AnimBP->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_StateMachine* SM = Cast<UAnimGraphNode_StateMachine>(Node))
			{
				if (SM->GetStateMachineName().Equals(Name, ESearchCase::IgnoreCase))
					return SM;
			}
		}
	}
	OutError = FString::Printf(TEXT("State machine '%s' not found"), *Name);
	return nullptr;
}

UEdGraph* FAnimBlueprintEditor::GetStateMachineGraph(UAnimGraphNode_StateMachine* SM)
{
	return SM ? SM->EditorStateMachineGraph : nullptr;
}

UAnimStateNode* FAnimBlueprintEditor::FindStateNode(UAnimGraphNode_StateMachine* SM, const FString& StateName)
{
	UEdGraph* SMGraph = GetStateMachineGraph(SM);
	if (!SMGraph) return nullptr;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			if (State->GetStateName().Equals(StateName, ESearchCase::IgnoreCase))
				return State;
		}
	}
	return nullptr;
}

UEdGraphPin* FAnimBlueprintEditor::FirstPin(UEdGraphNode* Node, int32 Direction)
{
	if (!Node) return nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == static_cast<EEdGraphPinDirection>(Direction))
			return Pin;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
// 스테이트 머신 생성
//-----------------------------------------------------------------------------

bool FAnimBlueprintEditor::CreateStateMachine(UAnimBlueprint* AnimBP, const FString& Name, int32 PosX, int32 PosY, FString& OutError)
{
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP, OutError);
	if (!AnimGraph) return false;

	FString Ignored;
	if (FindStateMachine(AnimBP, Name, Ignored))
	{
		OutError = FString::Printf(TEXT("State machine '%s' already exists"), *Name);
		return false;
	}

	FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
	UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
	SMNode->NodePosX = PosX;
	SMNode->NodePosY = PosY;
	NodeCreator.Finalize();

	UAnimationStateMachineGraph* SMGraph = CastChecked<UAnimationStateMachineGraph>(
		FBlueprintEditorUtils::CreateNewGraph(SMNode, NAME_None,
			UAnimationStateMachineGraph::StaticClass(), UAnimationStateMachineSchema::StaticClass()));

	SMNode->EditorStateMachineGraph = SMGraph;
	SMGraph->OwnerAnimGraphNode = SMNode;

	// 그래프 이름 = 사용자 지정 이름 (조회 기준). 그래프 할당 후 호출해야 적용됨.
	SMNode->OnRenameNode(Name);

	if (const UEdGraphSchema* Schema = SMGraph->GetSchema())
		Schema->CreateDefaultNodesForGraph(*SMGraph);

	// 스테이트 머신 출력 포즈를 AnimGraph 루트(Output Pose)에 연결하여 실제로 동작하게 함.
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node))
		{
			UEdGraphPin* SMOut = FirstPin(SMNode, EGPD_Output);
			UEdGraphPin* RootIn = FirstPin(Root, EGPD_Input);
			if (SMOut && RootIn)
			{
				RootIn->BreakAllPinLinks();
				SMOut->MakeLinkTo(RootIn);
			}
			break;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

//-----------------------------------------------------------------------------
// 스테이트 추가
//-----------------------------------------------------------------------------

bool FAnimBlueprintEditor::AddState(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& StateName, int32 PosX, int32 PosY, FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	UEdGraph* SMGraph = GetStateMachineGraph(SM);
	if (!SMGraph) { OutError = TEXT("State machine graph missing"); return false; }

	if (FindStateNode(SM, StateName))
	{
		OutError = FString::Printf(TEXT("State '%s' already exists"), *StateName);
		return false;
	}

	FGraphNodeCreator<UAnimStateNode> NodeCreator(*SMGraph);
	UAnimStateNode* StateNode = NodeCreator.CreateNode();
	StateNode->NodePosX = PosX;
	StateNode->NodePosY = PosY;
	// 어설션: BoundGraph 설정 전에 Finalize 필수.
	NodeCreator.Finalize();

	UAnimationStateGraph* StateGraph = CastChecked<UAnimationStateGraph>(
		FBlueprintEditorUtils::CreateNewGraph(StateNode, FName(*StateName),
			UAnimationStateGraph::StaticClass(), UAnimationStateGraphSchema::StaticClass()));
	StateNode->BoundGraph = StateGraph;

	if (const UEdGraphSchema* Schema = StateGraph->GetSchema())
		Schema->CreateDefaultNodesForGraph(*StateGraph);

	// Result 노드 폴백 생성
	bool bHasResult = false;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		if (Node->IsA<UAnimGraphNode_StateResult>()) { bHasResult = true; break; }
	}
	if (!bHasResult)
	{
		FGraphNodeCreator<UAnimGraphNode_StateResult> ResultCreator(*StateGraph);
		UAnimGraphNode_StateResult* ResultNode = ResultCreator.CreateNode();
		ResultNode->NodePosX = 200;
		ResultNode->NodePosY = 0;
		ResultCreator.Finalize();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

//-----------------------------------------------------------------------------
// 전이 추가
//-----------------------------------------------------------------------------

bool FAnimBlueprintEditor::AddTransition(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& FromState, const FString& ToState, FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	UEdGraph* SMGraph = GetStateMachineGraph(SM);
	if (!SMGraph) { OutError = TEXT("State machine graph missing"); return false; }

	UAnimStateNode* Source = FindStateNode(SM, FromState);
	UAnimStateNode* Target = FindStateNode(SM, ToState);
	if (!Source) { OutError = FString::Printf(TEXT("From-state '%s' not found"), *FromState); return false; }
	if (!Target) { OutError = FString::Printf(TEXT("To-state '%s' not found"), *ToState); return false; }

	FGraphNodeCreator<UAnimStateTransitionNode> NodeCreator(*SMGraph);
	UAnimStateTransitionNode* Transition = NodeCreator.CreateNode();
	Transition->NodePosX = (Source->NodePosX + Target->NodePosX) / 2;
	Transition->NodePosY = (Source->NodePosY + Target->NodePosY) / 2;
	NodeCreator.Finalize();

	// 흐름 핀 연결: Source.Out → Transition.In, Transition.Out → Target.In
	if (UEdGraphPin* SrcOut = Source->GetOutputPin())
		if (UEdGraphPin* TransIn = Transition->GetInputPin())
			SrcOut->MakeLinkTo(TransIn);
	if (UEdGraphPin* TransOut = Transition->GetOutputPin())
		if (UEdGraphPin* TgtIn = Target->GetInputPin())
			TransOut->MakeLinkTo(TgtIn);

	// 전이 조건 그래프 생성 (Result 노드 포함). 조건 자체 설정은 후속 작업.
	UAnimationTransitionGraph* TransGraph = CastChecked<UAnimationTransitionGraph>(
		FBlueprintEditorUtils::CreateNewGraph(Transition, NAME_None,
			UAnimationTransitionGraph::StaticClass(), UAnimationTransitionSchema::StaticClass()));
	Transition->BoundGraph = TransGraph;

	if (const UEdGraphSchema* Schema = TransGraph->GetSchema())
		Schema->CreateDefaultNodesForGraph(*TransGraph);

	bool bHasResult = false;
	for (UEdGraphNode* Node : TransGraph->Nodes)
	{
		if (Node->IsA<UAnimGraphNode_TransitionResult>()) { bHasResult = true; break; }
	}
	if (!bHasResult)
	{
		FGraphNodeCreator<UAnimGraphNode_TransitionResult> ResultCreator(*TransGraph);
		UAnimGraphNode_TransitionResult* ResultNode = ResultCreator.CreateNode();
		ResultNode->NodePosX = 200;
		ResultNode->NodePosY = 0;
		ResultCreator.Finalize();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

//-----------------------------------------------------------------------------
// 스테이트 애님 연결
//-----------------------------------------------------------------------------

bool FAnimBlueprintEditor::SetStateAnimation(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& StateName, const FString& AnimSequencePath, FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	UAnimStateNode* State = FindStateNode(SM, StateName);
	if (!State) { OutError = FString::Printf(TEXT("State '%s' not found"), *StateName); return false; }

	UEdGraph* StateGraph = State->GetBoundGraph();
	if (!StateGraph) { OutError = TEXT("State bound graph missing"); return false; }

	UAnimSequence* Sequence = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
	if (!Sequence) { OutError = FString::Printf(TEXT("AnimSequence not found: %s"), *AnimSequencePath); return false; }

	// Result 노드 찾기
	UEdGraphNode* ResultNode = nullptr;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		if (Node->IsA<UAnimGraphNode_StateResult>()) { ResultNode = Node; break; }
	}
	if (!ResultNode) { OutError = TEXT("State result node not found"); return false; }

	// 시퀀스 플레이어 노드 생성 (에셋은 Finalize 전에 설정)
	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*StateGraph);
	UAnimGraphNode_SequencePlayer* SeqNode = NodeCreator.CreateNode();
	SeqNode->NodePosX = ResultNode->NodePosX - 300;
	SeqNode->NodePosY = ResultNode->NodePosY;
	SeqNode->Node.SetSequence(Sequence);
	NodeCreator.Finalize();

	// 시퀀스 출력 포즈 → Result 입력 포즈
	UEdGraphPin* PoseOut = FirstPin(SeqNode, EGPD_Output);
	UEdGraphPin* ResultIn = FirstPin(ResultNode, EGPD_Input);
	if (!PoseOut || !ResultIn) { OutError = TEXT("Pose pins not found"); return false; }
	ResultIn->BreakAllPinLinks();
	PoseOut->MakeLinkTo(ResultIn);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

//-----------------------------------------------------------------------------
// 진입 스테이트 설정
//-----------------------------------------------------------------------------

bool FAnimBlueprintEditor::SetEntryState(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& StateName, FString& OutError)
{
	UAnimGraphNode_StateMachine* SM = FindStateMachine(AnimBP, StateMachineName, OutError);
	if (!SM) return false;

	UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(GetStateMachineGraph(SM));
	if (!SMGraph || !SMGraph->EntryNode) { OutError = TEXT("Entry node missing"); return false; }

	UAnimStateNode* State = FindStateNode(SM, StateName);
	if (!State) { OutError = FString::Printf(TEXT("State '%s' not found"), *StateName); return false; }

	UEdGraphPin* EntryOut = FirstPin(SMGraph->EntryNode, EGPD_Output);
	UEdGraphPin* StateIn = State->GetInputPin();
	if (!EntryOut || !StateIn) { OutError = TEXT("Entry/State pins not found"); return false; }

	EntryOut->BreakAllPinLinks();
	EntryOut->MakeLinkTo(StateIn);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

//-----------------------------------------------------------------------------
// 메인 AnimGraph 노드 추가
//-----------------------------------------------------------------------------

UEdGraphNode* FAnimBlueprintEditor::FindNodeById(UEdGraph* AnimGraph, const FString& NodeId)
{
	if (!AnimGraph) return nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (Node && Node->NodeGuid.ToString() == NodeId)
			return Node;
	}
	return nullptr;
}

UEdGraphPin* FAnimBlueprintEditor::FindPin(UEdGraphNode* Node, const FString& PinName, int32 Direction)
{
	if (!Node) return nullptr;
	if (!PinName.IsEmpty())
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == static_cast<EEdGraphPinDirection>(Direction)
				&& Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
				return Pin;
		}
		return nullptr;
	}
	return FirstPin(Node, Direction);
}

bool FAnimBlueprintEditor::AddSlotNode(UAnimBlueprint* AnimBP, const FString& SlotName, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError)
{
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP, OutError);
	if (!AnimGraph) return false;

	FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*AnimGraph);
	UAnimGraphNode_Slot* SlotNode = NodeCreator.CreateNode();
	SlotNode->NodePosX = PosX;
	SlotNode->NodePosY = PosY;
	if (!SlotName.IsEmpty())
		SlotNode->Node.SlotName = FName(*SlotName);
	NodeCreator.Finalize();

	OutNodeId = SlotNode->NodeGuid.ToString();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

bool FAnimBlueprintEditor::AddLayeredBlendPerBone(UAnimBlueprint* AnimBP, const TArray<FString>& BoneNames, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError)
{
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP, OutError);
	if (!AnimGraph) return false;

	FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
	UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
	BlendNode->NodePosX = PosX;
	BlendNode->NodePosY = PosY;

	// 지정 본들을 하나의 블렌드 레이어(BlendPose 0)의 BranchFilter로 구성합니다.
	// 입력 핀(Base Pose, Blend Poses 0)은 Finalize 시 LayerSetup 수에 맞춰 할당되므로 미리 채웁니다.
	if (BoneNames.Num() > 0)
	{
		FInputBlendPose Layer;
		for (const FString& Bone : BoneNames)
		{
			FBranchFilter Filter;
			Filter.BoneName = FName(*Bone);
			Filter.BlendDepth = 0;
			Layer.BranchFilters.Add(Filter);
		}
		BlendNode->Node.LayerSetup.Empty();
		BlendNode->Node.LayerSetup.Add(Layer);
		BlendNode->Node.BlendWeights.Empty();
		BlendNode->Node.BlendWeights.Add(1.0f);
	}
	NodeCreator.Finalize();

	OutNodeId = BlendNode->NodeGuid.ToString();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

bool FAnimBlueprintEditor::ConnectAnimNodes(UAnimBlueprint* AnimBP, const FString& FromNodeId, const FString& FromPin, const FString& ToNodeId, const FString& ToPin, FString& OutError)
{
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP, OutError);
	if (!AnimGraph) return false;

	UEdGraphNode* SourceNode = FindNodeById(AnimGraph, FromNodeId);
	if (!SourceNode) { OutError = FString::Printf(TEXT("Source node '%s' not found in AnimGraph"), *FromNodeId); return false; }

	// 대상이 output/result면 Output Pose(Root) 노드로 해석합니다.
	UEdGraphNode* TargetNode = nullptr;
	if (ToNodeId.Equals(TEXT("output"), ESearchCase::IgnoreCase) || ToNodeId.Equals(TEXT("result"), ESearchCase::IgnoreCase))
	{
		for (UEdGraphNode* Node : AnimGraph->Nodes)
		{
			if (Node->IsA<UAnimGraphNode_Root>()) { TargetNode = Node; break; }
		}
		if (!TargetNode) { OutError = TEXT("Output Pose (Root) node not found"); return false; }
	}
	else
	{
		TargetNode = FindNodeById(AnimGraph, ToNodeId);
		if (!TargetNode) { OutError = FString::Printf(TEXT("Target node '%s' not found in AnimGraph"), *ToNodeId); return false; }
	}

	UEdGraphPin* OutPin = FindPin(SourceNode, FromPin, EGPD_Output);
	UEdGraphPin* InPin = FindPin(TargetNode, ToPin, EGPD_Input);
	if (!OutPin) { OutError = FString::Printf(TEXT("Source output pin not found (%s)"), *FromPin); return false; }
	if (!InPin) { OutError = FString::Printf(TEXT("Target input pin not found (%s)"), *ToPin); return false; }

	InPin->BreakAllPinLinks();
	OutPin->MakeLinkTo(InPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	return true;
}

//-----------------------------------------------------------------------------
// 컴파일
//-----------------------------------------------------------------------------

FString FAnimBlueprintEditor::CompileAndSave(UAnimBlueprint* AnimBP, bool& bOutSuccess)
{
	bOutSuccess = false;
	if (!AnimBP) return TEXT("No AnimBlueprint");

	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	AnimBP->MarkPackageDirty();

	switch (AnimBP->Status)
	{
	case BS_UpToDate:
		bOutSuccess = true;
		return TEXT("Compiled: UpToDate");
	case BS_UpToDateWithWarnings:
		bOutSuccess = true;
		return TEXT("Compiled with warnings");
	case BS_Error:
		return TEXT("Compilation FAILED");
	default:
		return FString::Printf(TEXT("Compile status %d"), static_cast<int32>(AnimBP->Status));
	}
}
