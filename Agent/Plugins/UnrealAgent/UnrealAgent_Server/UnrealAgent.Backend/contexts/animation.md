---
name: animation
description: Anim sequences/montages/notifies and the Python limits of anim BPs (Python unreal API)
keywords:
  - animation
  - 애니메이션
  - 애님
  - anim
  - montage
  - 몽타주
  - sequence
  - 시퀀스
  - notify
  - 노티파이
  - skeleton
  - 스켈레톤
  - state machine
  - 스테이트 머신
  - blend space
  - blendspace
  - aim offset
  - aimoffset
  - aim
  - 에임
  - 에임오프셋
  - animinstance
  - retarget
---

**For state-machine construction, use the `anim_blueprint_modify` native tool, not Python.**
Python cannot edit the anim BP graph. Build the state-machine skeleton with this tool:
- `create_state_machine` (state_machine name; auto-connected to the AnimGraph output pose)
- `add_state` (state_machine, state_name)
- `set_state_animation` (state_machine, state_name, anim_sequence path)
- `set_entry_state` (entry state)
- `add_transition` (from_state, to_state; the condition graph is created empty — set the
  condition afterward)

Typical flow: `create_state_machine` → `add_state`×2 → `set_state_animation` for each state →
`set_entry_state` → `add_transition`.

**Query/add AnimGraph nodes with the `anim_blueprint_modify` native tool** (not possible via
Python).
- Query: `get_anim_graph` (read-only) — returns node type/node_id/slot_name/state_machine/pin
  connections. **Do not try to introspect pins/GUIDs/connections via Python** (no
  `get_all_pins`/`export_text`/`pins` attributes — a waste of time). Also do not open the editor
  to inspect — doing so wrongly can crash.

Targets the main AnimGraph, and `add_*` returns a node id (NodeGuid):
- `add_slot_node` (slot_name) — a montage-slot playback node
- `add_layered_blend_per_bone` (bones: comma-separated bone names) — a per-bone layered blend node
- `connect_anim_nodes` (from_node_id, to_node_id; giving `output`/`result` for `to` connects to
  the Output Pose; if from_pin/to_pin are omitted, the first output/input pose pin is used)

**Edit montage slot tracks with the `montage_modify` native tool.** In Python, `SlotAnimTracks`
is protected and blocked, so slot changes are not possible. This tool manipulates it directly in
C++:
- `create` (montage_path, skeleton or anim_sequence)
- `add_slot` / `rename_slot` (slot_name, new_slot_name) / `add_segment` (slot_name,
  anim_sequence, start_time)
- `get_info` — query slot/segment composition

**Do Aim Offset with the `aim_offset_modify` (asset) + `anim_blueprint_modify` (node) native
tools.** Editing BlendSpace samples/axes via Python is not possible (`SampleData`/`AddSample`
are blocked). An aim offset is a `UAimOffsetBlendSpace` (an additive specialization of
BlendSpace).

`aim_offset_modify` — asset creation/sample editing:
- `create` (aim_offset_path, skeleton) — the skeleton must be specified. Created with default
  Yaw/Pitch axes.
- `add_sample` (aim_offset_path, anim_sequence, yaw, pitch) — adds a corner pose (additive
  sequence) at the Yaw/Pitch coordinate. If the coordinate is outside the axis range, the range
  is auto-expanded.
- `get_info` (aim_offset_path) — query axis names/ranges and the sample list. **Do not open the
  editor to inspect** (crash risk) — verify via get_info.

`anim_blueprint_modify` — place an aim offset playback node in the main AnimGraph:
- `add_aim_offset_node` (aim_offset_path) — adds a `UAnimGraphNode_RotationOffsetBlendSpace` node
  and returns a node_id (NodeGuid).
- Connect the base pose with `connect_anim_nodes` (the aim offset node's first input pose pin is
  the Base Pose). Connect the aim offset output on to the Output Pose or a subsequent node.

Typical flow: `aim_offset_modify create` → `add_sample`×N (Yaw/Pitch corners) →
`anim_blueprint_modify add_aim_offset_node` → `connect_anim_nodes` (base pose input, and aim
offset → output) → compile (automatic after write ops).

Limits — the following are unsupported and done manually in the editor: variable binding of the
Yaw/Pitch float pins, customizing the axis range / grid division. Report limits to the user.

Data-level work such as querying anim sequences, adding notifies, and manipulating curves is
possible with the Python below.

```python
import unreal

# Load a skeletal mesh / anim asset
anim_seq = unreal.EditorAssetLibrary.load_asset('/Game/Anims/Run')   # AnimSequence

# Query length/frames
length = anim_seq.get_play_length()
fps = unreal.AnimationLibrary.get_frame_rate(anim_seq)
num_frames = unreal.AnimationLibrary.get_num_frames(anim_seq)

# Add a notify
unreal.AnimationLibrary.add_animation_notify_event(
    anim_seq, 'Footstep', 0.5, unreal.AnimNotify())

# Create an anim montage (from a sequence)
montage = unreal.AnimationLibrary.create_anim_montage(anim_seq)

# Query skeleton bone
skeleton = anim_seq.get_editor_property('target_skeleton')
```

- If anim BP logic (state transitions, blends) is needed, clearly state the Python limit and
  automate only up to preparing the data assets.
- `save_asset` is required after notify/curve work.

## What is NOT possible via Python — no guessing / brute-force

The following **do not exist** in the Python `unreal` API. Do not guess at similar names in a
loop (it freezes the editor and only wastes time). When needed, report the limit to the user:
- `unreal.SkeletalMeshLibrary`, `unreal.AnimationBlueprintLibrary` — **the modules themselves do
  not exist.**
- Querying a skeleton/skeletal mesh's reference skeleton or bone indices: the `reference_skeleton`
  / `ref_skeleton` properties, the `find_bone_index()` method — **do not exist.** Do not try to
  enumerate the bone list via Python.
- The anim BP's preview mesh (`preview_skeletal_mesh`) — `AnimBlueprint` has **no** such property
  (it is an editor-only setting and does not affect functionality).

Alternatives:
- **Pass bone names exactly as received from the user/request** to `anim_blueprint_modify`'s
  `add_layered_blend_per_bone` (`bones`). Do not try to validate/enumerate them via Python.
- If a skeleton reference is needed, use only the documented asset properties:
  `anim_seq.get_editor_property('target_skeleton')`, `skeletal_mesh.skeleton`.
- For APIs not in the docs, do not spend a long time exploring with `dir()`/`help()`; report the
  limit and proceed only as far as possible with the dedicated tools (`anim_blueprint_modify` /
  `montage_modify`).
