#pragma once

#include "CoreMinimal.h"

class UAnimBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UAnimGraphNode_StateMachine;
class UAnimStateNode;
class UAnimStateTransitionNode;
class FJsonObject;

/**
 * Animation Blueprint(스테이트 머신) 편집 헬퍼입니다.
 *
 * Python `unreal` API로 불가능한 스테이트 머신 골격 구성을 UE 에디터 API로 수행합니다.
 * 최소 기능 슬라이스: 스테이트 머신/스테이트/전이 생성, 스테이트 애님 시퀀스 연결, 진입 스테이트 설정.
 * 모든 함수는 게임 스레드에서 호출되어야 합니다.
 *
 * 식별: 스테이트 머신/스테이트는 내부 BoundGraph 이름(사용자 지정 이름)으로 조회합니다.
 */
class FAnimBlueprintEditor
{
public:
	static UAnimBlueprint* LoadAnimBlueprint(const FString& BlueprintPath, FString& OutError);

	/** 메인 AnimGraph를 찾습니다. */
	static UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBP, FString& OutError);

	/** 스테이트 머신 노드를 생성합니다 (내부 그래프 + Entry 노드 포함). */
	static bool CreateStateMachine(UAnimBlueprint* AnimBP, const FString& Name, int32 PosX, int32 PosY, FString& OutError);

	/** 스테이트를 추가합니다 (BoundGraph + Result 노드 포함). */
	static bool AddState(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& StateName, int32 PosX, int32 PosY, FString& OutError);

	/** 두 스테이트 사이에 전이를 추가합니다 (조건 그래프 포함). */
	static bool AddTransition(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& FromState, const FString& ToState, FString& OutError);

	/** 스테이트의 애님 그래프에 시퀀스 플레이어를 배치하고 출력 포즈에 연결합니다. */
	static bool SetStateAnimation(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& StateName, const FString& AnimSequencePath, FString& OutError);

	/** 진입 스테이트를 설정합니다 (Entry → State 재연결). */
	static bool SetEntryState(UAnimBlueprint* AnimBP, const FString& StateMachineName, const FString& StateName, FString& OutError);

	// ── 메인 AnimGraph 노드 추가 (스테이트 머신 외) ──
	// Python `unreal` API로 불가능한 AnimGraph 노드 생성을 네이티브로 수행합니다.
	// 생성된 노드는 NodeGuid 문자열로 식별하며, OutNodeId로 반환합니다.

	/** 메인 AnimGraph에 Slot 노드를 추가합니다 (몽타주 슬롯 재생). */
	static bool AddSlotNode(UAnimBlueprint* AnimBP, const FString& SlotName, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError);

	/** 메인 AnimGraph에 LayeredBlendPerBone 노드를 추가합니다 (지정 본들을 하나의 블렌드 레이어로 구성). */
	static bool AddLayeredBlendPerBone(UAnimBlueprint* AnimBP, const TArray<FString>& BoneNames, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError);

	/** 메인 AnimGraph에 AimOffset(RotationOffsetBlendSpace) 노드를 추가합니다. Base Pose 연결은 ConnectAnimNodes로 별도 수행합니다. */
	static bool AddAimOffsetNode(UAnimBlueprint* AnimBP, const FString& AimOffsetPath, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError);

	/**
	 * 메인 AnimGraph에 Transform (Modify) Bone 노드를 추가합니다 (UAnimGraphNode_ModifyBone).
	 * 추가 포즈 애셋 없이 스파인 트위스트(컴포넌트 스페이스 yaw 회전) 등을 구성할 때 사용합니다.
	 * Rotation 핀은 기본 노출되므로 MakeRotator 등으로 바로 구동할 수 있습니다.
	 * RotationMode: Ignore|Replace|Add(Additive) (기본 Add). RotationSpace: World|Component|Parent|Bone (기본 Component).
	 */
	static bool AddModifyBoneNode(UAnimBlueprint* AnimBP, const FString& BoneName, const FString& RotationMode, const FString& RotationSpace,
		int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError);

	/**
	 * 메인 AnimGraph의 두 노드를 연결합니다 (포즈 핀).
	 * 노드는 NodeGuid 문자열로 지정하며, 대상에 "output"/"result"를 주면 Output Pose(Root)에 연결합니다.
	 * 핀 이름이 비어 있으면 소스는 첫 출력 핀, 대상은 첫 입력 핀을 사용합니다.
	 */
	static bool ConnectAnimNodes(UAnimBlueprint* AnimBP, const FString& FromNodeId, const FString& FromPin, const FString& ToNodeId, const FString& ToPin, FString& OutError);

	// ── 서브그래프 범용 편집 (전이 조건 그래프 / 메인 AnimGraph) ──
	// 대상 그래프: from_state/to_state가 모두 지정되면 해당 전이의 조건 그래프, 아니면 메인 AnimGraph.
	// K2 노드 편집은 FBlueprintGraphEditor에 위임하므로(VariableGet/CallFunction/Branch 등) 임의
	// 불리언 조건식 구성 + 노드 핀에 변수 getter 연결(예: AimYaw → AimOffset X 핀)이 가능합니다.

	/**
	 * AnimGraph 노드의 옵셔널 프로퍼티(예: AimOffset의 X=Yaw / Y=Pitch)를 입력 핀으로 노출/숨김합니다.
	 * 노출해야 변수 getter를 해당 float 핀에 연결할 수 있습니다(노출 전에는 핀이 존재하지 않음).
	 */
	static bool ExposeNodePin(UAnimBlueprint* AnimBP, const FString& NodeId, const FString& PropertyName, bool bExpose, FString& OutError);

	/**
	 * 대상 그래프에 K2 노드를 생성합니다 (FBlueprintGraphEditor::CreateNode 위임). OutNodeId 반환.
	 * NodeType/NodeParams 규칙은 blueprint_modify의 add_node와 동일합니다.
	 */
	static bool AddGraphNode(UAnimBlueprint* AnimBP, const FString& NodeType, const TSharedPtr<FJsonObject>& NodeParams,
		const FString& StateMachine, const FString& FromState, const FString& ToState, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError);

	/**
	 * 대상 그래프의 두 핀을 연결합니다 (FBlueprintGraphEditor::ConnectPins 위임).
	 * ToNodeId가 'result'/'output'이면 그래프의 TransitionResult 노드로 해석하고, 핀 미지정 시
	 * bCanEnterTransition으로 자동 지정합니다.
	 */
	static bool ConnectGraphPins(UAnimBlueprint* AnimBP, const FString& FromNodeId, const FString& FromPin,
		const FString& ToNodeId, const FString& ToPin, const FString& StateMachine, const FString& FromState, const FString& ToState, FString& OutError);

	/** 대상 그래프 노드의 입력 핀 기본값을 설정합니다 (FBlueprintGraphEditor::SetPinDefaultValue 위임). */
	static bool SetGraphPinDefault(UAnimBlueprint* AnimBP, const FString& NodeId, const FString& PinName, const FString& Value,
		const FString& StateMachine, const FString& FromState, const FString& ToState, FString& OutError);

	/**
	 * 대상 그래프(메인 AnimGraph 또는 전이 조건 그래프)의 노드를 삭제합니다.
	 * AnimGraph/조건 그래프의 일반 노드용 — 바운드 그래프를 가진 상태/전이 노드는 대상이 아닙니다.
	 */
	static bool DeleteGraphNode(UAnimBlueprint* AnimBP, const FString& NodeId,
		const FString& StateMachine, const FString& FromState, const FString& ToState, FString& OutError);

	/**
	 * 전이에 "시퀀스 재생 완료 시 자동 전이" 룰을 설정합니다 (조건 그래프 없이 애님 종료 전이).
	 * TriggerTime이 음수면 변경하지 않습니다.
	 */
	static bool SetTransitionAutoRule(UAnimBlueprint* AnimBP, const FString& StateMachine, const FString& FromState, const FString& ToState,
		bool bEnable, float TriggerTime, FString& OutError);

	/**
	 * 메인 AnimGraph의 노드/핀/연결을 JSON 유사 텍스트로 반환합니다 (읽기 전용).
	 * Python으로 불가능한 AnimGraph introspection을 네이티브로 제공합니다.
	 */
	static FString GetAnimGraphInfo(UAnimBlueprint* AnimBP, FString& OutError);

	/** 컴파일하고 저장 마크합니다. */
	static FString CompileAndSave(UAnimBlueprint* AnimBP, bool& bOutSuccess);

private:
	static UAnimGraphNode_StateMachine* FindStateMachine(UAnimBlueprint* AnimBP, const FString& Name, FString& OutError);
	static UEdGraph* GetStateMachineGraph(UAnimGraphNode_StateMachine* SM);
	static UAnimStateNode* FindStateNode(UAnimGraphNode_StateMachine* SM, const FString& StateName);

	/** From→To 전이 노드를 찾습니다. */
	static UAnimStateTransitionNode* FindTransitionNode(UAnimGraphNode_StateMachine* SM, const FString& FromState, const FString& ToState);

	/** 편집 대상 그래프를 해석합니다: from/to 지정 시 전이 조건 그래프, 아니면 메인 AnimGraph. */
	static UEdGraph* ResolveTargetGraph(UAnimBlueprint* AnimBP, const FString& StateMachine, const FString& FromState, const FString& ToState, FString& OutError);

	/** 노드의 지정 방향 첫 핀을 반환합니다. */
	static UEdGraphPin* FirstPin(UEdGraphNode* Node, int32 Direction);

	/** 메인 AnimGraph에서 NodeGuid 문자열로 노드를 찾습니다. */
	static UEdGraphNode* FindNodeById(UEdGraph* AnimGraph, const FString& NodeId);

	/** 노드에서 지정 방향의 핀을 이름으로 찾습니다. 이름이 비면 첫 핀을 반환합니다. */
	static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, int32 Direction);
};
