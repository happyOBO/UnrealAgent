---
name: widget-ui
description: UMG 위젯 블루프린트(HUD/크로스헤어 등)를 생성·수정한다. 위젯 트리·속성·버튼 바인딩·시각 검증까지 한 번에 수행.
---

이 작업은 **완료까지 연속으로 수행**한다. 한 단계만 하고 멈추지 말 것. 각 도구 호출 후
다음 단계로 이어서 최종 검증까지 끝낸 뒤 보고한다.

## 목표
사용자가 요청한 UMG 위젯(HUD, 크로스헤어, 인벤토리 패널 등)을 생성하거나 수정하고,
컴파일과 시각 검증까지 마친다.

## 도구 분리 원칙 (중요)
- **위젯 트리(레이아웃)** = `widget_modify` 만 사용. Python으로는 WidgetTree가 protected라
  편집 불가하므로 절대 execute_python으로 위젯을 만들지 말 것.
  - `list_widgets` (읽기, 현재 트리 확인) / `add_widget` / `remove_widget` /
    `set_widget_property` (스칼라·문자열 속성만) / `reparent_widget`
  - `add_widget`은 `widget_type`(Button|TextBlock|VerticalBox|HorizontalBox|
    CanvasPanel|Image|ProgressBar 등), 선택 `widget_name`, 선택 `parent`.
    parent가 비면 루트(트리에 루트가 없을 때만), 아니면 parent는 패널 위젯이어야 함.
- **이벤트 그래프(로직/바인딩)** = `blueprint_modify` 사용. 버튼 클릭 등 위젯 이벤트는
  `add_node node_type=ComponentBoundEvent component=<ButtonName>
  delegate_property=OnClicked` 로 만들고 그 뒤에 로직 노드를 연결한다.
- **그래프 로직은 도구로 끝까지 처리한다 — "수동 필요"로 미루지 말 것.** 아래는 모두
  `blueprint_modify`로 가능하다:
  - **커스텀 이벤트 파라미터**: `add_function_param custom_event=<이벤트명> param_name=…
    param_type=…` — param_type은 enum(`EWeaponSlot`), 클래스(`ItemInstance`,
    `UInventoryComponent`), 구조체(`FVector2D`/`Vector2D`)까지 지원.
    (함수 그래프 파라미터는 `custom_event` 없이 동일 op 사용.)
  - **Switch on Enum**: `add_node node_type=SwitchEnum enum=<EnumName>`.
  - **반복**: `add_node node_type=ForLoop`(또는 `ForEachLoop`/`WhileLoop`, 혹은
    `MacroInstance macro=ForLoop`).
  - **위젯 생성**: `add_node node_type=CreateWidget target_class=<UserWidget클래스>`.
    `UWidgetBlueprintLibrary::Create`로 배치되므로 WorldContextObject(self)·OwningPlayer
    핀을 wire한다.
  - **오버라이드 이벤트**: `add_node node_type=OverrideEvent event=<부모이벤트>`.
    반환값 있는 마우스/드래그 이벤트(`OnMouseButtonDown`/`OnDrop`/`OnDragDetected`)는
    함수 오버라이드 그래프로 생성되며, 본문은 `graph_name=<이벤트명> is_function_graph=true`
    로 이어서 편집한다. 반환값 없는 `OnMouseEnter`/`OnMouseLeave`는 일반 이벤트 노드로 배치된다.

## 절차
1. **현황 파악**: 대상 WBP가 있으면 `widget_modify list_widgets` 로 현재 트리를 먼저 본다.
   없으면 새 위젯 BP 경로를 확인한다(`/Game/UI/...`).
2. **레이아웃 구성**: 패널(CanvasPanel/VerticalBox 등)을 먼저 두고 그 아래 자식 위젯을
   `add_widget` 한다. 패널 없이 평면으로 쌓지 말 것.
3. **속성 설정**: 텍스트·색상·정렬 등 단순 위젯 속성은 `set_widget_property`(스칼라·문자열
   전용). 위젯 *디자이너* 속성이 구조체라 set_widget_property로 안 되는 경우에만 수동 설정
   위치를 안내한다. (그래프 쪽 enum/struct/class 파라미터는 위 `add_function_param`으로 처리하므로
   수동으로 미루지 않는다.)
4. **바인딩/로직**: 클릭·값 갱신·반복·분기·드래그앤드롭·오버라이드 이벤트 모두 `blueprint_modify`
   로 끝낸다 — ComponentBoundEvent/커스텀이벤트(+파라미터)/SwitchEnum/ForLoop/CreateWidget/
   OverrideEvent를 조합한다. HOTRELOAD로 Cast 노드가 `HOTRELOAD_*`를 가리켜 막히면 도구 한계가
   아니라 Live Coding 산물이니 **에디터 재시작**을 안내한다.
5. **컴파일**: widget_modify는 수정 후 자동 컴파일된다. 에러가 나면 즉시 수정한다.
6. **시각 검증**: `capture_viewport` 로 실제 화면을 확인한다. 의도와 다르면 1~5를 반복.
7. **로그 확인**: `get_output_log` 로 위젯 관련 경고/에러(WidgetBlueprintGeneratedClass
   ensure 등)가 없는지 확인한다.

## 프로젝트 컨벤션
- 위젯 블루프린트 접두사 **`WBP_`** (예: `WBP_HealthBar`, `WBP_Crosshair`).
- 기존 위젯과 중복 생성 금지 — 만들기 전에 `/Game/UI` 하위에 유사 위젯이 있는지 확인.

## 예시
- 크로스헤어: CanvasPanel 루트 → 중앙 정렬 Image 4개(상/하/좌/우 라인) 또는 단일
  Image. 가만히 있을 때 / 휘둘렀을 때 / 타격 시 상태 변화는 위젯 변수 + 그래프 로직으로.
- HUD 무기 표시(WBP_WeaponDisplay): HorizontalBox 안에 아이콘 Image + 탄약 TextBlock,
  값 갱신은 인벤토리 변경 이벤트에 바인딩.
- 인벤토리 슬롯(WBP_*Slot): `SetItem` 커스텀이벤트에 `add_function_param custom_event=SetItem
  param_type=ItemInstance`로 입력 파라미터를 달고, 등급색은 `SwitchEnum enum=EItemGrade`,
  슬롯 실루엣/숫자키는 `SwitchEnum enum=EArmorSlot|EWeaponSlot`로 분기. 그리드 채우기는
  `ForLoop` + `CreateWidget`, 우클릭/호버/드래그는 `OverrideEvent`로 처리. 전부 도구로 완결한다.
