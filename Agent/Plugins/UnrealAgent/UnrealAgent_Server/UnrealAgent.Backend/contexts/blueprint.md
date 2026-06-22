---
name: blueprint
description: 블루프린트 에셋 생성/변수·컴포넌트 추가 및 Python의 한계 (Python unreal API)
keywords:
  - blueprint
  - 블루프린트
  - 블프
  - bp_
  - event graph
  - 이벤트 그래프
  - node
  - 노드
  - 변수 추가
  - add variable
  - construction script
  - bp component
  - reparent
  - compile blueprint
---

**노드 그래프 편집은 Python이 아니라 `blueprint_modify` 네이티브 도구를 사용한다.** Python `unreal` API로는 이벤트 그래프의 노드 추가·핀 연결이 불가능하므로, 그래프 로직 작업에는 반드시 `blueprint_modify` 도구를 쓴다:
- `add_node` node_type:
  - 기본: CallFunction / Event / VariableGet / VariableSet / Branch / Sequence / Cast / CustomEvent / FunctionResult
  - 델리게이트/이벤트 바인딩: ComponentBoundEvent / AddDelegate / RemoveDelegate / CreateDelegate
  - 제어 흐름·UMG: **SwitchEnum**(enum=EnumName) / **MacroInstance**(macro=ForLoop|ForEachLoop|WhileLoop|DoOnce|…, 또는 매크로 이름을 node_type으로 직접) / **CreateWidget**(target_class=UserWidget 클래스)
  - **OverrideEvent**(event=부모 함수/이벤트 이름): 마우스/드래그 등 오버라이드 전용. 반환값 있는 이벤트(OnMouseButtonDown→FEventReply, OnDrop→bool, OnDragDetected→out Operation)는 **함수 오버라이드 그래프**로 생성되며 반환된 node_id가 그 FunctionEntry다. 본문은 `graph_name=<이벤트명> is_function_graph=true`로 편집. 반환값 없는 이벤트(OnMouseEnter/Leave)는 일반 이벤트 노드로 배치된다.
- `connect_pins` (핀명이 비면 exec 핀 자동), `set_pin_value`, `delete_node`, `disconnect_pins`, `list_nodes`
- `add_function_param` param_type = bool|int|int64|float|double|string|name|text|`<Class>`|`<Struct>`|`<Enum>` (struct는 `FVector2D`/`Vector2D` 모두, enum은 `EWeaponSlot` 형태). **CustomEvent에 파라미터**를 추가하려면 `custom_event=<이벤트명>`을 함께 넘긴다(함수 그래프가 아닌 이벤트 노드의 입력 핀으로 추가됨).
- 각 연산 후 자동 컴파일·저장됨

전형적 흐름: `add_node`(Event BeginPlay) → `add_node`(CallFunction PrintString) → `connect_pins`(exec) → `set_pin_value`(InString) → 완료.

> **HOTRELOAD/Live Coding 오염 주의:** C++ 타입(예: 직접 만든 Character/Component)을 Live Coding으로 재컴파일한 뒤 그 타입을 참조하는 Cast·함수 호출 노드가 `HOTRELOAD_*` 스텁을 가리켜 컴파일/바인딩이 실패할 수 있다. 이는 도구 한계가 아니라 UE Live Coding 산물이므로 **에디터를 재시작**하면 해소된다.

**컴포넌트 추가도 `blueprint_modify`의 `add_component`를 쓴다 — Python으로 `SubobjectDataSubsystem`을 직접 다루지 말 것.** Python SCS 조작은 API가 까다로워 중복 컴포넌트 생성·에디터 hang을 유발한다. 대신:
- `add_component`: `component_type`(예: StaticMeshComponent) + 선택 `component_name` + 선택 `static_mesh`(에셋 경로). 메시까지 한 번에 설정됨.
- 도구가 실패를 반환하면(예: 클래스/메시 미발견) **그 사유를 사용자에게 그대로 안내**하고, 같은 작업을 Python으로 우회 시도하지 말 것.

에셋 생성·변수 추가·부모 클래스 변경 등 비(非)그래프 작업은 아래 Python으로도 가능하다.

```python
import unreal

# 블루프린트 에셋 생성 (부모 클래스 지정)
factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.Actor)
bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'BP_Door', '/Game/Blueprints', unreal.Blueprint, factory)

# 변수 추가 (BlueprintEditorLibrary)
unreal.BlueprintEditorLibrary.add_member_variable(
    bp, 'Health', unreal.EdGraphPinType(
        pin_category='float'))   # 핀 타입은 런타임에 dir()로 구조 확인 권장

# 부모 클래스 변경
unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.Pawn)

# 컴파일 + 저장 (변경 후 항상)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())

# 기본값(CDO) 속성 설정
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('some_property', value)
```

- 변수 핀 타입 구조가 불확실하면 `help(unreal.BlueprintEditorLibrary.add_member_variable)`로 시그니처를 먼저 확인한다.
- 컴포넌트 추가는 Subobject Data Subsystem(`unreal.SubobjectDataSubsystem`)을 거쳐야 하며 복잡하다. 필요 시 단계적으로 접근하고 각 단계 결과를 print로 검증.
- 변경 후 **반드시 compile + save**. 안 하면 에디터에서 반영되지 않는다.
