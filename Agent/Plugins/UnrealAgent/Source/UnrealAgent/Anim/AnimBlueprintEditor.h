#pragma once

#include "CoreMinimal.h"

class UAnimBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UAnimGraphNode_StateMachine;
class UAnimStateNode;
class UAnimStateTransitionNode;

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
	 * 메인 AnimGraph의 두 노드를 연결합니다 (포즈 핀).
	 * 노드는 NodeGuid 문자열로 지정하며, 대상에 "output"/"result"를 주면 Output Pose(Root)에 연결합니다.
	 * 핀 이름이 비어 있으면 소스는 첫 출력 핀, 대상은 첫 입력 핀을 사용합니다.
	 */
	static bool ConnectAnimNodes(UAnimBlueprint* AnimBP, const FString& FromNodeId, const FString& FromPin, const FString& ToNodeId, const FString& ToPin, FString& OutError);

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

	/** 노드의 지정 방향 첫 핀을 반환합니다. */
	static UEdGraphPin* FirstPin(UEdGraphNode* Node, int32 Direction);

	/** 메인 AnimGraph에서 NodeGuid 문자열로 노드를 찾습니다. */
	static UEdGraphNode* FindNodeById(UEdGraph* AnimGraph, const FString& NodeId);

	/** 노드에서 지정 방향의 핀을 이름으로 찾습니다. 이름이 비면 첫 핀을 반환합니다. */
	static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, int32 Direction);
};
