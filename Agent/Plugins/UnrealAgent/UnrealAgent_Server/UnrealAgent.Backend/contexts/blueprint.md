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
- `add_node` (node_type: CallFunction/Event/VariableGet/VariableSet/Branch/Sequence) → 반환된 node_id를 이후 연결에 사용
- `connect_pins` (핀명이 비면 exec 핀 자동), `set_pin_value`, `delete_node`, `disconnect_pins`, `list_nodes`
- 각 연산 후 자동 컴파일·저장됨

전형적 흐름: `add_node`(Event BeginPlay) → `add_node`(CallFunction PrintString) → `connect_pins`(exec) → `set_pin_value`(InString) → 완료.

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
