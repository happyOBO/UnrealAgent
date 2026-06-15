---
name: actor
description: 액터 스폰/이동/삭제/조회 및 컴포넌트 (Python unreal API)
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

UE5에서는 **에디터 서브시스템**을 우선 사용한다. `EditorLevelLibrary`는 deprecated이며 가급적 피한다.

```python
import unreal

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# 스폰 (클래스 + Transform)
loc = unreal.Vector(0, 0, 100)
rot = unreal.Rotator(0, 0, 0)
actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)

# 에셋(메시 등)으로 스폰
mesh = unreal.EditorAssetLibrary.load_asset('/Game/Meshes/SM_Cube')
actor = actor_sub.spawn_actor_from_object(mesh, loc, rot)

# 모든 레벨 액터 조회 — 큰 레벨에서는 절대 전부 print하지 말고 클래스/이름으로 필터
all_actors = actor_sub.get_all_level_actors()
sm_actors = [a for a in all_actors if isinstance(a, unreal.StaticMeshActor)]

# 라벨(아웃라이너 표시명)으로 찾기
target = next((a for a in all_actors if a.get_actor_label() == 'Cube_1'), None)

# Transform 조작 (항상 set_editor_property 대신 전용 메서드 사용 가능)
actor.set_actor_location(unreal.Vector(x, y, z), sweep=False, teleport=True)
actor.set_actor_rotation(unreal.Rotator(pitch, yaw, roll), teleport_physics=True)
actor.set_actor_scale3d(unreal.Vector(sx, sy, sz))
actor.set_actor_label('NewName')

# 컴포넌트 접근/속성 — set_editor_property로 pre/post edit 콜백 보장
smc = actor.static_mesh_component   # StaticMeshActor의 경우
smc.set_editor_property('cast_shadow', False)
smc.set_material(0, some_material)

# 삭제
actor_sub.destroy_actor(actor)
```

- 좌표계: Z-up, 단위 cm. 회전은 `Rotator(pitch, yaw, roll)` 순서 주의.
- 다수 액터 순회 시 반드시 `unreal.ScopedSlowTask`로 감싼다 (취소 가능, 에디터 프리즈 방지).
- 선택 상태: `actor_sub.set_selected_level_actors([actor])`.
