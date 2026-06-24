---
name: blueprint
description: Blueprint asset creation / adding variables & components, and the limits of Python (Python unreal API)
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

**For node-graph editing, use the `blueprint_modify` native tool, not Python.** The Python
`unreal` API cannot add nodes or connect pins in the event graph, so always use
`blueprint_modify` for graph-logic work:
- `add_node` node_type:
  - Basic: CallFunction / Event / VariableGet / VariableSet / Branch / Sequence / Cast /
    CustomEvent / FunctionResult
  - **VariableGet/VariableSet on an external object**: pass `target_class=<OwnerClass>` (e.g.
    `ItemDataAsset`) to read/write that class's `BlueprintReadOnly`/`BlueprintReadWrite`
    UPROPERTY **directly** — a `BlueprintReadOnly` property is exposed as a Get node with a Target
    pin, so **no getter UFUNCTION is needed** (do not add C++ getters to work around this; wire the
    object into the node's Target pin via `connect_pins`). Omit `target_class` for self/inherited/
    widget-tree variables.
  - **CallFunction on an inherited (self) function**: omit `target_class` for functions this
    Blueprint inherits (e.g. `GetOwningPlayerState`/`GetOwningPlayer` on a UserWidget) — they
    resolve by `function` name alone. `target_class` is only for external libraries/classes
    (`KismetSystemLibrary`|`KismetMathLibrary`|`GameplayStatics`|`<Class>`).
  - **Struct & text**: **BreakStruct** (`struct_type=StructName`) splits a struct into one output
    pin per member — use to read members that have **no individual BP accessor** (e.g.
    `FMatchStatistics` → FinalPlacement/SurvivalTime/…); **MakeStruct** (`struct_type`) is the
    inverse. **FormatText** (`format="…{0}/{Name}…"`) auto-creates an argument input pin per
    token; wire values in and connect the Result text into a SetText/Text pin.
  - Delegate/event binding: ComponentBoundEvent / AddDelegate / RemoveDelegate / CreateDelegate
  - Control flow / UMG: **SwitchEnum** (enum=EnumName) / **MacroInstance**
    (macro=ForLoop|ForEachLoop|WhileLoop|DoOnce|…, or the macro name directly as node_type) /
    **CreateWidget** (target_class=UserWidget class)
  - **OverrideEvent** (event=parent function/event name): for mouse/drag and other overrides
    only. Events with a return value (OnMouseButtonDown→FEventReply, OnDrop→bool,
    OnDragDetected→out Operation) are created as a **function-override graph**, and the returned
    node_id is its FunctionEntry. Edit the body with `graph_name=<EventName>
    is_function_graph=true`. Return-less events (OnMouseEnter/Leave) are placed as ordinary
    event nodes.
- `connect_pins` (when a pin name is empty, the exec pin is used automatically), `set_pin_value`,
  `delete_node`, `disconnect_pins`, `list_nodes`
- `add_function_param` param_type = bool|int|int64|float|double|string|name|text|`<Class>`|
  `<Struct>`|`<Enum>` (struct accepts both `FVector2D`/`Vector2D`, enum is the `EWeaponSlot`
  form). To **add a parameter to a CustomEvent**, also pass `custom_event=<EventName>` (it is
  added as an input pin of the event node, not a function graph).
- Each operation auto-compiles and saves.

Typical flow: `add_node`(Event BeginPlay) → `add_node`(CallFunction PrintString) →
`connect_pins`(exec) → `set_pin_value`(InString) → done.

> **HOTRELOAD/Live Coding contamination warning:** After recompiling a C++ type (e.g. your own
> Character/Component) via Live Coding, Cast or function-call nodes that reference that type may
> point to a `HOTRELOAD_*` stub, causing compile/binding failures. This is not a tool limitation
> but a UE Live Coding artifact, so it is resolved by **restarting the editor**.

**Add components with `blueprint_modify`'s `add_component` too — do not manipulate
`SubobjectDataSubsystem` directly via Python.** Python SCS manipulation has a finicky API that
causes duplicate component creation and editor hangs. Instead:
- `add_component`: `component_type` (e.g. StaticMeshComponent) + optional `component_name` +
  optional `static_mesh` (asset path). Sets the mesh in one call.
- If the tool returns a failure (e.g. class/mesh not found), **report that reason to the user
  as-is** and do not attempt to work around the same task via Python.

Non-graph work such as asset creation, adding variables, and changing the parent class is also
possible with the Python below.

```python
import unreal

# Create a blueprint asset (specify parent class)
factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.Actor)
bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'BP_Door', '/Game/Blueprints', unreal.Blueprint, factory)

# Add a variable (BlueprintEditorLibrary)
unreal.BlueprintEditorLibrary.add_member_variable(
    bp, 'Health', unreal.EdGraphPinType(
        pin_category='float'))   # recommended to confirm pin-type structure at runtime via dir()

# Change parent class
unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.Pawn)

# Compile + save (always, after changes)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())

# Set default (CDO) properties
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('some_property', value)
```

- If the variable pin-type structure is uncertain, first check the signature with
  `help(unreal.BlueprintEditorLibrary.add_member_variable)`.
- Adding components must go through the Subobject Data Subsystem
  (`unreal.SubobjectDataSubsystem`) and is complex. When needed, approach it step by step and
  verify each step's result with print.
- After changes, **always compile + save**. Otherwise it will not be reflected in the editor.
