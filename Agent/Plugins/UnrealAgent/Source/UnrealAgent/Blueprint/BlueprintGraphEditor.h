#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UClass;
class UScriptStruct;
class UEnum;
class FJsonObject;
class FMulticastDelegateProperty;
struct FEdGraphPinType;

/**
 * 블루프린트 노드 그래프 편집 헬퍼입니다.
 *
 * Python `unreal` API로는 불가능한 이벤트 그래프 노드 추가/핀 연결/기본값 설정을
 * UE 에디터 API(UK2Node, FGraphNodeCreator, UEdGraphSchema)로 수행합니다.
 *
 * 모든 함수는 게임 스레드에서 호출되어야 합니다 (MCP 도구 Execute()가 게임 스레드에서
 * 동기 실행되므로 별도 마샬링은 불필요).
 *
 * 노드 식별: 생성한 노드는 NodeComment에 "UA_ID:<id>"를 기록하고, 기존 노드는
 * NodeGuid 문자열로도 조회할 수 있습니다.
 */
class FBlueprintGraphEditor
{
public:
	/** 블루프린트 에셋을 로드합니다. 엔진/cooked 경로는 차단합니다. */
	static UBlueprint* LoadBlueprint(const FString& BlueprintPath, FString& OutError);

	/** 이벤트(Ubergraph) 또는 함수 그래프를 찾습니다. 빈 이름은 첫 그래프를 반환합니다. */
	static UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName, bool bFunctionGraph, FString& OutError);

	/**
	 * 노드를 생성하여 그래프에 추가합니다.
	 * NodeType: CallFunction | Event | OverrideEvent | VariableGet | VariableSet | Branch | Sequence
	 *         | Cast | CustomEvent | FunctionResult | ComponentBoundEvent | AddDelegate | RemoveDelegate | CreateDelegate
	 *         | SwitchEnum | CreateWidget | MacroInstance | ForLoop | ForEachLoop | WhileLoop | ...
	 * NodeParams: 타입별 추가 인자 (function/target_class/event/variable/num_outputs/
	 *             component/delegate_property/delegate_owner/bound_function/enum/macro)
	 */
	static UEdGraphNode* CreateNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& NodeType,
		const TSharedPtr<FJsonObject>& NodeParams, int32 PosX, int32 PosY, FString& OutNodeId, FString& OutError);

	/**
	 * 함수 그래프에 사용자 정의 입출력 파라미터를 추가합니다.
	 * bIsOutput=false → FunctionEntry의 출력 핀(입력 파라미터), true → FunctionResult의 입력 핀(반환값).
	 * FunctionResult 노드가 없으면 생성합니다. ParamType은 MakePinType 규칙을 따릅니다.
	 */
	static bool AddFunctionParam(UEdGraph* FuncGraph, const FString& ParamName, const FString& ParamType,
		bool bIsOutput, FString& OutError);

	/**
	 * 이름이 일치하는 CustomEvent 노드(이벤트 그래프 소재)에 사용자 정의 입력 파라미터를 추가합니다.
	 * CustomEvent 파라미터는 함수 그래프가 아닌 이벤트 노드의 출력 핀이므로 AddFunctionParam과 별도 경로입니다.
	 * ParamType은 MakePinType 규칙(enum/struct/class 포함)을 따릅니다.
	 */
	static bool AddCustomEventParam(UBlueprint* Blueprint, const FString& EventName, const FString& ParamName,
		const FString& ParamType, FString& OutError);

	/** 두 노드의 핀을 연결합니다. 핀 이름이 비면 exec 핀을 자동 선택합니다. */
	static bool ConnectPins(UEdGraph* Graph, const FString& SourceNodeId, const FString& SourcePinName,
		const FString& TargetNodeId, const FString& TargetPinName, FString& OutError);

	/** 두 노드의 핀 연결을 해제합니다. */
	static bool DisconnectPins(UEdGraph* Graph, const FString& SourceNodeId, const FString& SourcePinName,
		const FString& TargetNodeId, const FString& TargetPinName, FString& OutError);

	/** 입력 핀의 기본값을 설정합니다 (스키마 타입 파싱 경유). */
	static bool SetPinDefaultValue(UEdGraph* Graph, const FString& NodeId, const FString& PinName,
		const FString& Value, FString& OutError);

	/** 노드를 삭제합니다 (모든 연결 해제 후 제거). */
	static bool DeleteNode(UEdGraph* Graph, const FString& NodeId, FString& OutError);

	/** 노드 정보를 JSON으로 직렬화합니다 (id/class/pos/pins). */
	static TSharedPtr<FJsonObject> SerializeNodeInfo(UEdGraphNode* Node);

	/** 그래프의 모든 노드를 직렬화한 JSON 배열 문자열을 반환합니다. */
	static FString ListNodes(UEdGraph* Graph);

	/**
	 * 블루프린트에 컴포넌트를 추가합니다 (SubobjectDataSubsystem 경유, 게임 스레드).
	 * ComponentType: 클래스 이름(예: StaticMeshComponent). StaticMeshPath가 있으면 메시까지 설정.
	 * 실패 시 false + OutError(명확한 사유) — hang 없이 반환합니다.
	 */
	static bool AddComponent(UBlueprint* Blueprint, const FString& ComponentType, const FString& ComponentName,
		const FString& StaticMeshPath, FString& OutError);

	/** 블루프린트를 컴파일하고 저장 마크합니다. bOutSuccess에 성공 여부, 반환값은 상태/메시지. */
	static FString CompileAndSave(UBlueprint* Blueprint, bool& bOutSuccess);

	/** NodeComment ID 또는 NodeGuid로 노드를 찾습니다. */
	static UEdGraphNode* FindNodeById(UEdGraph* Graph, const FString& NodeId);

private:
	static UEdGraphNode* CreateCallFunctionNode(UEdGraph* Graph, const FString& FunctionName, const FString& TargetClass, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateEventNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& EventName, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateVariableNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FString& TargetClass, bool bSetter, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateBranchNode(UEdGraph* Graph, int32 PosX, int32 PosY);
	static UEdGraphNode* CreateSequenceNode(UEdGraph* Graph, int32 PosX, int32 PosY);
	static UEdGraphNode* CreateCastNode(UEdGraph* Graph, const FString& TargetClass, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateCustomEventNode(UEdGraph* Graph, const FString& EventName, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateFunctionResultNode(UEdGraph* Graph, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateComponentBoundEventNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& ComponentName, const FString& DelegateProperty, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateDelegateNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& NodeType, const FString& DelegateProperty, const FString& DelegateOwner, const FString& BoundFunction, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateSwitchEnumNode(UEdGraph* Graph, const FString& EnumName, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateMacroInstanceNode(UEdGraph* Graph, const FString& MacroName, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateWidgetNode(UEdGraph* Graph, const FString& WidgetClassName, int32 PosX, int32 PosY, FString& OutError);
	static UEdGraphNode* CreateOverrideEventNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& EventName, int32 PosX, int32 PosY, FString& OutError);

	/** 표준 매크로(StandardMacros) 그래프를 이름으로 찾습니다. */
	static UEdGraph* FindStandardMacroGraph(const FString& MacroName);
	/** NodeType이 표준 매크로 이름(ForLoop/ForEachLoop/WhileLoop 등)인지 검사합니다. */
	static bool IsStandardMacroNodeType(const FString& NodeType);

	/** 클래스 이름을 UClass로 해석합니다 (Kismet 별칭 + 전역 검색). 실패 시 nullptr. */
	static UClass* ResolveClass(const FString& ClassName);
	/** 스트럭트 이름을 UScriptStruct로 해석합니다 (F 접두사 보정 포함). 실패 시 nullptr. */
	static UScriptStruct* ResolveScriptStruct(const FString& StructName);
	/** enum 이름을 UEnum으로 해석합니다. 실패 시 nullptr. */
	static UEnum* ResolveEnum(const FString& EnumName);
	/** Owner 클래스에서 멀티캐스트 델리게이트 프로퍼티를 찾습니다. */
	static FMulticastDelegateProperty* FindMulticastDelegateProperty(UClass* Owner, const FString& PropertyName);
	/** 타입 문자열을 FEdGraphPinType으로 변환합니다 (bool/int/float/string/<Class>/<Struct>/<Enum> 등). */
	static bool MakePinType(const FString& TypeStr, struct FEdGraphPinType& OutPinType, FString& OutError);

	static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName, int32 Direction);
	static UEdGraphPin* GetExecPin(UEdGraphNode* Node, bool bOutput);

	static FString GenerateNodeId(const FString& NodeType, UEdGraph* Graph);
	static void SetNodeId(UEdGraphNode* Node, const FString& NodeId);
	static FString GetNodeId(UEdGraphNode* Node);

	static int32 NodeIdCounter;
	static const TCHAR* NodeIdPrefix;
};
