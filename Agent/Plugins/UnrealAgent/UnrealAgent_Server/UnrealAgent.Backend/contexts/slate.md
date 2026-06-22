---
name: slate
description: UMG 위젯 편집(widget_modify)과 Slate/에디터 UI의 Python 한계 (네이티브 도구 + Python unreal API)
keywords:
  - ui
  - 유아이
  - hud
  - widget
  - 위젯
  - umg
  - slate
  - 슬레이트
  - user widget
  - userwidget
  - menu
  - 메뉴
  - button
  - 버튼
  - canvas
  - text block
  - healthbar
  - 체력바
  - viewport ui
---

게임 UI는 **UMG(UUserWidget)** 로 만들고, 위젯 트리 편집은 Python이 아니라 **`widget_modify` 네이티브 도구**를 쓴다. Python `unreal` API로는 위젯 트리(패널/자식 위젯)의 구조적 편집이 불가능하다(`WidgetTree` 조작이 차단/불안정). 에디터(C++) UI인 Slate는 런타임 Python 대상이 아니다.

## 위젯 트리 편집은 widget_modify 네이티브 도구

- 위젯 블루프린트(WidgetBlueprint, `/Game/UI/WBP_*`)의 위젯 추가·중첩·속성 설정을 수행한다.
- 전형적 흐름: Canvas Panel 루트 → 자식 위젯(TextBlock/Button/ProgressBar/Image) 추가 → 속성(텍스트/색/앵커) 설정 → 컴파일.
- `widget_modify`가 실패(클래스/위젯 미발견 등)를 반환하면 **그 사유를 사용자에게 그대로 안내**하고 Python으로 우회하지 말 것.

## Python으로 가능한 보조 작업

```python
import unreal

# WidgetBlueprint 에셋 생성 (부모 UserWidget)
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property('parent_class', unreal.UserWidget)
wbp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'WBP_Main', '/Game/UI', None, factory)
unreal.EditorAssetLibrary.save_asset(wbp.get_path_name())

# 위젯 BP에 변수/그래프 로직이 필요하면 blueprint_modify 사용 (UserWidget도 블루프린트)
```

위젯의 **이벤트 그래프 로직**(OnClicked 바인딩, 애니메이션 트리거 등)은 `blueprint_modify`로 노드 그래프를 편집한다 — UserWidget도 블루프린트이므로 동일 도구가 적용된다.

## Python으로 불가능한 것 — 추정 금지

- `WidgetTree.construct_widget` / 패널 자식 추가를 Python으로 시도하지 말 것 — 차단되거나 에디터 상태가 깨진다. `widget_modify`를 쓴다.
- 위젯 애니메이션(UWidgetAnimation) 트랙 편집 — Python API 없음. 한계 보고.
- 에디터 패널/툴바 등 Slate UI는 C++(`SCompoundWidget`) 전용. Python으로 만들 수 없다.
- 비슷한 이름의 API를 루프로 추정하지 말고, 위 범위를 벗어나면 한계를 보고한다.

## 권장 흐름

`WidgetBlueprintFactory`로 WBP 에셋 생성(Python) → 위젯 트리/레이아웃은 `widget_modify` → 클릭 이벤트 등 로직은 `blueprint_modify` → 변경 후 compile + save → `capture_viewport`로 시각 검증(가능 시).
