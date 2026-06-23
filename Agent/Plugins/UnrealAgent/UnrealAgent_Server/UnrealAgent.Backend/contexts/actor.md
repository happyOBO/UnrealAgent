---
name: actor
description: Actor spawn/move/delete/query and components (Python unreal API)
keywords:
  - actor
  - 액터
  - spawn
  - 스폰
  - 배치
  - place
  - move
  - 이동
  - transform
  - location
  - rotation
  - scale
  - component
  - 컴포넌트
  - delete actor
  - destroy
  - level actor
---

**For actor spawn/move/delete/set-property and level-actor queries, prefer the native tools** —
they return structured results and are more reliable than Python workarounds:
- `actor_modify` (operation: `spawn`/`move`/`delete`/`set_property`)
- `get_level_actors` (class/name filter + pagination; avoids the footgun of printing everything
  in a large level)

Use the Python below only for fine-grained work the native tools cannot do (special spawn
parameters, batch-processing logic, etc.). In UE5, prefer **editor subsystems**.
`EditorLevelLibrary` is deprecated and should be avoided when possible.

```python
import unreal

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn (class + Transform)
loc = unreal.Vector(0, 0, 100)
rot = unreal.Rotator(0, 0, 0)
actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)

# Spawn from an asset (mesh, etc.)
mesh = unreal.EditorAssetLibrary.load_asset('/Game/Meshes/SM_Cube')
actor = actor_sub.spawn_actor_from_object(mesh, loc, rot)

# Query all level actors — in a large level, never print them all; filter by class/name
all_actors = actor_sub.get_all_level_actors()
sm_actors = [a for a in all_actors if isinstance(a, unreal.StaticMeshActor)]

# Find by label (outliner display name)
target = next((a for a in all_actors if a.get_actor_label() == 'Cube_1'), None)

# Transform manipulation (you can use the dedicated methods instead of set_editor_property)
actor.set_actor_location(unreal.Vector(x, y, z), sweep=False, teleport=True)
actor.set_actor_rotation(unreal.Rotator(pitch, yaw, roll), teleport_physics=True)
actor.set_actor_scale3d(unreal.Vector(sx, sy, sz))
actor.set_actor_label('NewName')

# Component access/properties — set_editor_property guarantees pre/post edit callbacks
smc = actor.static_mesh_component   # for a StaticMeshActor
smc.set_editor_property('cast_shadow', False)
smc.set_material(0, some_material)

# Delete
actor_sub.destroy_actor(actor)
```

- Coordinate system: Z-up, units cm. Watch the rotation argument order `Rotator(pitch, yaw, roll)`.
- When iterating many actors, always wrap with `unreal.ScopedSlowTask` (cancelable, prevents
  editor freeze).
- Selection state: `actor_sub.set_selected_level_actors([actor])`.
