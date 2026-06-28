---
name: anim-graph
description: Build and wire Animation Blueprint graphs natively — state machines, AimOffset playback + Yaw/Pitch pin binding, transition conditions, and turn-in-place / spine-twist setups. Drives everything through compilation in one pass.
---

Carry this out **continuously to completion**. Do not stop after a single step, and do not defer
graph wiring as "needs manual editor work" — the `anim_blueprint_modify` ops below cover state
machines, value-pin binding, transition conditions, and bone modifiers. After each tool call,
continue to the next step and finish with compilation + `get_anim_graph` verification before
reporting.

## Goal
Create or modify the Animation Blueprint graph the user requested — locomotion state machines,
aim/look offset, turn-in-place, additive blends — and finish through automatic compilation and a
read-back of the graph.

## Tool-separation principle (important)
Python **cannot** edit the anim BP graph. Use the native tools; do not brute-force `execute_python`
with guessed API names (it freezes the editor — see "Not possible via Python" below).

- **AnimGraph + state-machine structure** = `anim_blueprint_modify`. After every write op the
  AnimBP is auto-compiled and marked dirty. Nodes are identified by `node_id` (NodeGuid) returned
  from `add_*` ops. **Do not introspect via Python** — use `get_anim_graph` (read-only) for
  node types / node_ids / pin connections. Do not open the editor to inspect (crash risk).
- **AimOffset asset (samples/axes)** = `aim_offset_modify` (`create` / `add_sample` / `get_info`).
  BlendSpace `SampleData`/`AddSample` are blocked in Python.
- **Montage slot tracks** = `montage_modify` (`create` / `add_slot` / `rename_slot` /
  `add_segment` / `get_info`). `SlotAnimTracks` is protected in Python.
- **Data-level work** (anim sequence length/frames, notifies, curves) = Python is fine
  (`unreal.AnimationLibrary`, `anim_seq.get_play_length()`, etc.). `save_asset` after.

## `anim_blueprint_modify` operations
All take `blueprint_path`. Write ops auto-compile.

**State machine skeleton**
- `create_state_machine` (state_machine[, pos_x, pos_y]) — also connects the SM output to the
  AnimGraph Output Pose.
- `add_state` (state_machine, state_name[, pos])
- `set_state_animation` (state_machine, state_name, anim_sequence) — places a sequence player
  wired to the state Result.
- `set_entry_state` (state_machine, state_name)
- `add_transition` (state_machine, from_state, to_state) — condition graph starts empty; set it
  with the generic ops or `set_transition_auto_rule` below.

**Main AnimGraph nodes** (`add_*` return `node_id`)
- `add_slot_node` (slot_name) — montage-slot playback.
- `add_layered_blend_per_bone` (bones: comma-separated) — per-bone layered blend.
- `add_aim_offset_node` (aim_offset_path) — `UAnimGraphNode_RotationOffsetBlendSpace`. Its first
  input pose pin is the Base Pose.
- `add_modify_bone` (bone[, rotation_mode=Add, rotation_space=Component]) — Transform(Modify)Bone.
  rotation_mode: Ignore|Replace|Add. rotation_space: World|Component|Parent|Bone. Its `Rotation`
  pin (FRotator) is shown by default — no `expose_pin` needed.
- `connect_anim_nodes` (from_node_id, to_node_id; `output`/`result` = Output Pose;
  [from_pin],[to_pin] default first pose pin) — **pose** pins. Schema-based like an editor drag:
  **auto-inserts** a `LocalToComponentSpace`/`ComponentToLocalSpace` conversion node when pose pin
  spaces differ, and replaces an existing single-parent pose link. Connect the chain in order; you
  do not (and cannot) add the conversion nodes manually.
- `delete_node` (node_id[, from_state, to_state]) — remove a node from the main AnimGraph (or a
  transition condition graph). Plain nodes only, not state/transition nodes. Use to clean up or
  recover from a mistake without reloading the package.

**Value-pin binding & generic K2 editing** (reuses the blueprint K2 node editor)
Generic ops target a **transition condition graph** when both `from_state` + `to_state` are given,
otherwise the **main AnimGraph**.
- `expose_pin` (node_id, pin_name[, expose=true]) — show an optional property as a graph pin.
  AimOffset exposable pins: `X` (Yaw), `Y` (Pitch). Required before wiring them.
- `add_variable_get` (variable[, pos]) — VariableGet node for an AnimInstance member (e.g.
  `AimYaw`). The getter's data output pin is **named after the variable**.
- `add_graph_node` (node_type[, node_params JSON string][, from_state, to_state]) — same
  node_type/node_params as `blueprint_modify add_node`: `CallFunction`, `VariableGet`, `Branch`,
  `MakeStruct`, `Cast`, … e.g. `node_params={"function":"Greater_DoubleDouble","target_class":"KismetMathLibrary"}`.
- `connect` (from_node_id, from_pin, to_node_id, to_pin[, from_state, to_state]) — schema-checked
  (handles value pins). `to_node_id='result'` = the TransitionResult's `bCanEnterTransition` pin.
- `set_pin_default` (node_id, pin_name, value[, from_state, to_state]).
- `set_transition_auto_rule` (state_machine, from_state, to_state[, enable=true, trigger_time]) —
  transition fires when the state's sequence finishes; no condition graph needed.

## Procedure
1. **Assess**: For an existing ABP run `get_anim_graph` to see current nodes/ids/connections. For
   a new one confirm the path (`/Game/Characters/ABP_*`).
2. **Build the skeleton** (locomotion): `create_state_machine` → `add_state`×N →
   `set_state_animation` each → `set_entry_state` → `add_transition` between states.
3. **Set transition conditions**: drive each transition's bool. Simple bool var: `add_graph_node`
   (`VariableGet`, with from_state/to_state) → `connect` (getter → `result`). Richer expressions:
   add comparison `CallFunction`s (`Greater_DoubleDouble`, etc.) + `Branch`/AND and wire the bool
   to `result`. Animation-finished transitions: `set_transition_auto_rule`.
4. **Layer upper-body aim** (choose one):
   - **AimOffset**: `aim_offset_modify create` → `add_sample`×N (Yaw/Pitch corners) →
     `add_aim_offset_node` → `connect_anim_nodes` base pose in + output → bind pins:
     `expose_pin` X and Y → `add_variable_get` (AimYaw / AimPitch) → `connect`
     (from_pin = variable name → to_pin = `X`/`Y`).
   - **Spine twist (no extra pose assets)**: `add_modify_bone`(spine_01, Add, Component)×N →
     for each: `add_variable_get`(AimYaw) → `add_graph_node` CallFunction `Divide_DoubleDouble`
     (set B via `set_pin_default`, e.g. 3) → `add_graph_node` MakeStruct (`Rotator`) → `connect`
     divide → rotator `Yaw`, then `connect` rotator → ModifyBone `Rotation`. Chain the ModifyBone
     Component Pose pins in series with `connect_anim_nodes` and into the Output Pose. Pitch is the
     rotator `Pitch` analog. ModifyBone is **component-space**: feeding a local-space pose in (or
     its output into a local-space input) auto-inserts the conversion node — just connect in order.
5. **Compile**: automatic after each write op. If compilation fails, fix immediately and re-run.
6. **Verify**: `get_anim_graph` to confirm nodes/links exist as intended; `get_output_log` for
   anim warnings/errors.

## Gotchas
- **AimOffset X/Y are optional pins** — they don't exist until `expose_pin`. This is the #1
  reason a bind "fails."
- **VariableGet output pin name = the variable name**, so pass `from_pin=<VarName>` to `connect`
  (an empty from_pin looks for an exec pin and fails on a getter).
- **AnimInstance variables must already exist** (C++ UPROPERTY on the AnimInstance parent, or a BP
  variable). `add_variable_get` reads them; it does not declare them.
- **New tool params / reflection** require an **editor restart** to load. A `HOTRELOAD_*` artifact
  after Live Coding also calls for a restart, not a tool workaround.
- **Pass bone names exactly as given** (spine_01…); do not enumerate/validate the skeleton via
  Python. A wrong bone simply makes ModifyBone skip evaluation.
- A pin shown as `object` in read-back can be a display artifact; the real graph type is correct.

## Not possible via Python — do not guess/brute-force
- `unreal.AnimationBlueprintLibrary`, `unreal.SkeletalMeshLibrary` — the modules **do not exist**.
- Reference skeleton / bone index enumeration (`reference_skeleton`, `find_bone_index()`) — do not
  exist. Use the bone names from the user/request.
- AnimGraph pin/GUID/connection introspection — use `get_anim_graph`, not Python.

## Project conventions
- Animation Blueprint prefix **`ABP_`**. AimOffset assets often `AO_*`.
- Before creating, check whether the target ABP / AimOffset / montage already exists.

## Example — turn-in-place upper-body twist
Goal: stationary upper body follows the camera; legs turn when the angle exceeds a threshold.
1. `add_modify_bone`(spine_01/02/03, Add, Component) ×3; chain their poses base→spine_01→…→output
   via `connect_anim_nodes`.
2. Per bone: `add_variable_get`(AimYaw) → Divide_DoubleDouble (B=3) → MakeStruct Rotator(Yaw) →
   `connect` into the ModifyBone `Rotation` pin.
3. Turn state machine: `add_state` TurnLeft90/TurnRight90, `set_state_animation` each; condition
   `Idle→TurnRight90` = `add_graph_node`(VariableGet `bIsTurningInPlace`) + comparison on
   `TurnDirection`/`AimYaw` → `connect` to `result`; return transition via
   `set_transition_auto_rule` (sequence finished).
