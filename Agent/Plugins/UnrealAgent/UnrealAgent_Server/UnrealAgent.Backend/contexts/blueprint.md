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

**중요한 한계:** Python `unreal` API로는 블루프린트의 **노드 그래프(이벤트 그래프/함수 그래프)를 편집할 수 없다.** 노드 추가·핀 연결은 노출돼 있지 않다. 에셋 생성, 변수 추가, 컴포넌트 추가, 부모 클래스 변경, 컴파일 정도만 가능하다. 그래프 로직이 필요하면 그 한계를 사용자에게 알리고 대안(C++ 베이스 클래스, 수동 작업)을 제시한다.

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
