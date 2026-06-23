---
name: ue-core-api
description: UE5 Python unreal API core — editor subsystems / math types / class hierarchy / common pitfalls (Python unreal API)
keywords:
  - unreal
  - python
  - 파이썬
  - subsystem
  - 서브시스템
  - editor subsystem
  - vector
  - rotator
  - transform
  - 트랜스폼
  - get_editor_property
  - set_editor_property
  - find_component
  - component
  - world
  - 월드
  - class hierarchy
  - uclass
  - load_asset
---

UE5 Python (`unreal` module) core patterns. This is the foundation for all domain work; **prefer
editor subsystems** and avoid deprecated libraries (`EditorLevelLibrary`, parts of
`EditorAssetLibrary`) when possible.

## Editor subsystems (the recommended UE5 entry points)

```python
import unreal

actor_sub  = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)    # actor spawn/query/delete
asset_sub  = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)    # asset load/save/duplicate
level_sub  = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)    # level open/save/PIE
util_sub   = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)  # editor utility widgets
```

If `unreal.get_editor_subsystem(...)` returns None (subsystem unavailable), report the limit and
do not guess at a workaround.

## Math types — no custom implementations, use unreal types

```python
loc  = unreal.Vector(0, 0, 100)            # X, Y, Z (cm, double). Z-up.
rot  = unreal.Rotator(pitch, yaw, roll)    # degrees. Watch the argument order!
xform = unreal.Transform(location=loc, rotation=rot, scale=unreal.Vector(1,1,1))
col  = unreal.LinearColor(1.0, 0.0, 0.0, 1.0)   # materials/lights use LinearColor
q    = rot.quaternion()                    # quaternion when needed
# Use unreal.MathLibrary for math helpers (do not implement trig yourself)
```

## Property access — always get/set_editor_property

```python
# Direct attribute access skips pre/post edit callbacks and misses editor refresh. Always:
comp.set_editor_property('cast_shadow', False)
val = comp.get_editor_property('relative_location')
```

## Component lookup (runtime instance)

```python
# Find a component by class on an actor
smc = actor.get_component_by_class(unreal.StaticMeshComponent)
comps = actor.get_components_by_class(unreal.SceneComponent)   # copied array
# Root/owner
root = actor.get_editor_property('root_component')
```

## Class hierarchy essentials (the commonly used ones)

```
AActor → APawn → ACharacter | AController → APlayerController | AGameModeBase | ALight
UActorComponent → USceneComponent → UPrimitiveComponent → UMeshComponent
                  → UStaticMeshComponent | USkeletalMeshComponent
                  → UShapeComponent → UBox/USphere/UCapsuleComponent
                  → UCameraComponent | USpringArmComponent | UMovementComponent
UObject → UUserWidget | UDataAsset | UAnimInstance | UBlueprintFunctionLibrary
```
Reference classes directly like `unreal.StaticMeshActor`, or get an asset's generated class via
`bp.generated_class()`.

## Common pitfalls — no guessing / brute-force

- Do not guess at undocumented class/property names in a loop. It freezes the editor and only
  wastes time. If unknown, check **once** with `help(unreal.SomeClass.method)` / `dir(obj)`, and
  if it does not exist, report the limit.
- Always wrap iteration over many items with `unreal.ScopedSlowTask` to make it cancelable.
- After changes, `asset_sub.save_asset(path)` is required for assets. For blueprints, save after
  compile.
- `print()` is the only way to see results. After changes, self-verify with `capture_viewport`
  (visual) / `get_output_log` (warnings/errors).
- For areas **not possible via Python** such as graphs/state machines/slots, use the native
  tools (`blueprint_modify`, `anim_blueprint_modify`, `montage_modify`, `widget_modify`).
