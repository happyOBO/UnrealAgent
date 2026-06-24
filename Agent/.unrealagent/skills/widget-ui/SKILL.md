---
name: widget-ui
description: Create and modify UMG widget blueprints (HUD, crosshair, etc.). Builds the widget tree, properties, button bindings, and visual verification in one pass.
---

Carry this out **continuously to completion**. Do not stop after a single step. After each
tool call, continue to the next step and finish with the final verification before reporting.

## Goal
Create or modify the UMG widget the user requested (HUD, crosshair, inventory panel, etc.) and
finish through compilation and visual verification.

## Tool-separation principle (important)
- **Widget tree (layout)** = use `widget_modify` only. Python cannot edit the WidgetTree
  because it is protected, so NEVER create widgets via execute_python.
  - `list_widgets` (read, inspect current tree) / `add_widget` / `remove_widget` /
    `set_widget_property` / `reparent_widget`
  - `set_widget_property` handles scalars/strings **and** dotted struct paths
    (`Font.Size=48`, `Font.TypefaceFontName=Bold`) **and** layout-slot props via a `Slot.`
    prefix (`Slot.Anchors`, `Slot.Padding`, `Slot.Alignment`), which target the widget's
    parent-panel slot (CanvasPanelSlot/VerticalBoxSlot/…). Pass struct values in ImportText
    form, e.g. `Slot.Anchors=(Minimum=(X=0.5,Y=0.5),Maximum=(X=0.5,Y=0.5))`.
  - `add_widget` takes `widget_type` (Button|TextBlock|VerticalBox|HorizontalBox|
    CanvasPanel|Image|ProgressBar, etc.), optional `widget_name`, optional `parent`.
    If parent is empty it becomes the root (only when the tree has no root); otherwise the
    parent must be a panel widget.
- **Event graph (logic/binding)** = use `blueprint_modify`. For widget events such as button
  clicks, create them with `add_node node_type=ComponentBoundEvent component=<ButtonName>
  delegate_property=OnClicked` and then wire the logic nodes after it.
- **Finish graph logic with the tools — do not defer it as "needs manual work."** All of the
  following are possible with `blueprint_modify`:
  - **Custom event parameters**: `add_function_param custom_event=<EventName> param_name=…
    param_type=…` — param_type supports enums (`EWeaponSlot`), classes (`ItemInstance`,
    `UInventoryComponent`), and structs (`FVector2D`/`Vector2D`).
    (Function-graph parameters use the same op without `custom_event`.)
  - **Switch on Enum**: `add_node node_type=SwitchEnum enum=<EnumName>`.
  - **Struct members without an accessor**: `add_node node_type=BreakStruct
    struct_type=<StructName>` splits a struct into per-member output pins — use this to read
    members that have no individual BP getter (e.g. `FMatchStatistics` →
    FinalPlacement/SurvivalTime/…). **Do not add a C++ getter to work around a missing accessor.**
    `MakeStruct` is the inverse.
  - **Formatted text** (named-arg SetText): `add_node node_type=FormatText
    format="MVP: {ItemName} ({Kills} kills)"` (or `"#{0}"`). Tokens auto-create argument input
    pins; wire values in and connect the Result text into the TextBlock's SetText.
  - **Loops**: `add_node node_type=ForLoop` (or `ForEachLoop`/`WhileLoop`, or
    `MacroInstance macro=ForLoop`).
  - **Widget creation**: `add_node node_type=CreateWidget target_class=<UserWidget class>`.
    Placed via `UWidgetBlueprintLibrary::Create`, so wire the WorldContextObject (self) and
    OwningPlayer pins.
  - **Override events**: `add_node node_type=OverrideEvent event=<ParentEvent>`.
    Mouse/drag events with a return value (`OnMouseButtonDown`/`OnDrop`/`OnDragDetected`) are
    created as function-override graphs; edit their body with `graph_name=<EventName>
    is_function_graph=true`. Return-less events `OnMouseEnter`/`OnMouseLeave` are placed as
    ordinary event nodes.
  - **Array operations (wildcard)**: `add_node CallFunction function=Array_Get` (or
    `Array_Length`/`Array_Add`, etc.) is built by the tool as `UK2Node_CallArrayFunction`, so
    connecting a typed array output (e.g. `OutItems: TArray<UItemInstance*>` from
    `GetInventoryItems`) via `connect_pins` resolves the wildcard element type automatically.
    **Do not add a `GetXxxAt(int32)` index helper to game code just to iterate an array** —
    use `GetInventoryCount` for loop length and `Array_Get(arr, i)` for element access.
    (Even if `list_nodes` shows a pin as 'object', the real graph type is correct — it is only
    a display artifact.)

## Procedure
1. **Assess the current state**: If the target WBP exists, first inspect the current tree with
   `widget_modify list_widgets`. If not, confirm the new widget BP path (`/Game/UI/...`).
2. **Build the layout**: Place panels (CanvasPanel/VerticalBox, etc.) first, then `add_widget`
   child widgets under them. Do not stack widgets flat without a panel.
3. **Set properties**: Use `set_widget_property` for widget properties — scalars/strings, dotted
   struct paths (`Font.Size`, `Font.TypefaceFontName=Bold`), and layout-slot props via the
   `Slot.` prefix (`Slot.Anchors`/`Slot.Padding`/`Slot.Alignment`). Pass struct values in
   ImportText form. **Do not defer font/slot/struct tuning to manual work** — only fall back to
   manual guidance if a specific property genuinely cannot be reached this way.
   (Enum/struct/class parameters on the graph side are handled with `add_function_param` above.)
4. **Binding/logic**: Handle clicks, value updates, loops, branches, drag-and-drop, and
   override events all with `blueprint_modify` — combine ComponentBoundEvent / custom events
   (+parameters) / SwitchEnum / ForLoop / CreateWidget / OverrideEvent. If a HOTRELOAD makes a
   Cast node point to `HOTRELOAD_*` and gets stuck, that is not a tool limitation but a Live
   Coding artifact, so advise an **editor restart**.
5. **Compile**: widget_modify auto-compiles after edits. If an error occurs, fix it immediately.
6. **Visual verification**: Confirm the actual screen with `capture_viewport`. If it differs
   from the intent, repeat steps 1–5.
7. **Log check**: Use `get_output_log` to confirm there are no widget-related warnings/errors
   (WidgetBlueprintGeneratedClass ensure, etc.).

## Project conventions
- Widget blueprint prefix **`WBP_`** (e.g. `WBP_HealthBar`, `WBP_Crosshair`).
- Do not create duplicate widgets — before creating, check whether a similar widget already
  exists under `/Game/UI`.

## Examples
- Crosshair: CanvasPanel root → 4 center-aligned Images (top/bottom/left/right lines) or a
  single Image. State changes (idle / while swinging / on hit) via widget variables + graph
  logic.
- HUD weapon display (WBP_WeaponDisplay): an icon Image + ammo TextBlock inside a HorizontalBox,
  with value updates bound to inventory-change events.
- Inventory slot (WBP_*Slot): attach an input parameter to the `SetItem` custom event with
  `add_function_param custom_event=SetItem param_type=ItemInstance`; branch grade color with
  `SwitchEnum enum=EItemGrade`, and slot silhouette/number key with
  `SwitchEnum enum=EArmorSlot|EWeaponSlot`. Fill the grid with `ForLoop` + `CreateWidget`, and
  handle right-click/hover/drag with `OverrideEvent`. Complete it all with the tools.
- Result/stats screen (WBP_MatchResult): read a stats struct that has no per-field getter via
  `BreakStruct struct_type=<StatsStruct>`, format each TextBlock with `FormatText`
  (`"#{0}"`, `"MVP: {ItemName} ({Kills} kills)"`), and tune layout with
  `set_widget_property Font.Size`/`Font.TypefaceFontName=Bold` and `Slot.Anchors`/`Slot.Padding`.
