---
name: slate
description: UMG widget editing (widget_modify) and the Python limits of Slate/editor UI (native tools + Python unreal API)
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

Build game UI with **UMG (UUserWidget)**, and edit the widget tree with the **`widget_modify`
native tool**, not Python. The Python `unreal` API cannot do structural editing of the widget
tree (panels/child widgets) — `WidgetTree` manipulation is blocked/unstable. Slate, the editor
(C++) UI, is not a target for runtime Python.

## Widget tree editing is the widget_modify native tool

- Performs adding/nesting widgets and setting properties on a widget blueprint (WidgetBlueprint,
  `/Game/UI/WBP_*`).
- Typical flow: Canvas Panel root → add child widgets (TextBlock/Button/ProgressBar/Image) → set
  properties (text/color/anchor) → compile.
- If `widget_modify` returns a failure (class/widget not found, etc.), **report that reason to
  the user as-is** and do not work around it via Python.

## Auxiliary work possible via Python

```python
import unreal

# Create a WidgetBlueprint asset (parent UserWidget)
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property('parent_class', unreal.UserWidget)
wbp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'WBP_Main', '/Game/UI', None, factory)
unreal.EditorAssetLibrary.save_asset(wbp.get_path_name())

# If the widget BP needs variables/graph logic, use blueprint_modify (a UserWidget is also a blueprint)
```

For a widget's **event graph logic** (OnClicked binding, animation triggers, etc.), edit the node
graph with `blueprint_modify` — a UserWidget is also a blueprint, so the same tool applies.

## What is NOT possible via Python — no guessing

- Do not attempt `WidgetTree.construct_widget` / adding panel children via Python — it is blocked
  or breaks the editor state. Use `widget_modify`.
- Editing widget animation (UWidgetAnimation) tracks — no Python API. Report the limit.
- Editor panels/toolbars and other Slate UI are C++-only (`SCompoundWidget`). They cannot be
  created via Python.
- Do not guess at similarly named APIs in a loop; beyond the scope above, report the limit.

## Recommended flow

Create the WBP asset with `WidgetBlueprintFactory` (Python) → widget tree/layout with
`widget_modify` → logic such as click events with `blueprint_modify` → compile + save after
changes → visual verification with `capture_viewport` (when possible).
