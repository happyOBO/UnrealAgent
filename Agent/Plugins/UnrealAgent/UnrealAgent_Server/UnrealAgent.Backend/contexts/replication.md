---
name: replication
description: What network replication setup is possible via Python, and its limits (Python unreal API + blueprint_modify)
keywords:
  - replication
  - 리플리케이션
  - 복제
  - network
  - 네트워크
  - multiplayer
  - 멀티플레이
  - rpc
  - replicated
  - net
  - server
  - client
  - 서버
  - 클라이언트
  - dedicated server
  - replicate movement
---

Replication is mostly **declarative** (UPROPERTY `Replicated`, UFUNCTION `Server/Client/
NetMulticast` RPCs) or graph logic, so the scope achievable with the pure Python runtime API is
limited. Distinguish clearly what is possible vs not.

## What IS possible via Python — actor/component replication flags

```python
import unreal

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = ...  # target actor (usually a blueprint CDO or a placed instance)

# Actor replicate settings
actor.set_editor_property('replicates', True)
actor.set_editor_property('replicate_movement', True)   # AActor.bReplicateMovement
actor.set_editor_property('net_dormancy', unreal.NetDormancy.DORM_NEVER)
actor.set_editor_property('net_update_frequency', 30.0)

# Component replicate
comp = actor.get_component_by_class(unreal.StaticMeshComponent)
comp.set_editor_property('is_replicated', True)
```

To apply to a blueprint class default (CDO):
```python
bp = unreal.EditorAssetLibrary.load_asset('/Game/BP_Pickup')
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('replicates', True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
```

## What you do with blueprint_modify — variable replication / RPC nodes

- **A variable's `Replicated` / `RepNotify` flag**: add the variable with `blueprint_modify`, but
  the replication condition / RepNotify function binding requires graph and meta settings. Python
  `add_member_variable` can only create the variable, and **toggling the replication flag is
  currently an automation limit** — report it to the user and advise setting it via the variable
  Details' Replication dropdown in the editor.
- **RPCs (Server/Client/Multicast events)**: a custom event can be created with
  `blueprint_modify add_node` (CustomEvent), but **specifying the RPC replication type
  (Server/Reliable, etc.) is a node-meta setting** and cannot be toggled with the current tool.
  Build the skeleton (event+logic) and report the replication-type specification as a limit.

## What is NOT possible via Python — no guessing

- The `GetLifetimeReplicatedProps` / `DOREPLIFETIME` macros are C++-only. There is no Python
  equivalent.
- RPC `_Validate`/`WithValidation`, and `COND_*` replication conditions — no Python API.
- Do not guess at similarly named functions in a loop. Beyond the scope above, report the limit
  clearly and advise that C++/manual editor work is needed.

## Recommended flow

Set the actor/component `replicates` flag + `replicate_movement` via Python → build the skeleton
of variable/RPC replication types with `blueprint_modify`, then report the replication meta as a
limit → compile + save after changes → check warnings with `get_output_log`.
