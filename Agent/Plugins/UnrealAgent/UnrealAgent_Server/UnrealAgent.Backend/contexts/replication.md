---
name: replication
description: 네트워크 리플리케이션 설정의 Python 가능 범위와 한계 (Python unreal API + blueprint_modify)
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

리플리케이션은 대부분 **선언적(UPROPERTY `Replicated`, UFUNCTION `Server/Client/NetMulticast` RPC)** 이거나 그래프 로직이라, 순수 Python 런타임 API로 할 수 있는 범위가 제한적이다. 가능/불가능을 명확히 구분한다.

## Python으로 가능한 것 — 액터/컴포넌트의 리플리케이션 플래그

```python
import unreal

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = ...  # 대상 액터 (보통 블루프린트 CDO 또는 배치 인스턴스)

# 액터 리플리케이트 설정
actor.set_editor_property('replicates', True)
actor.set_editor_property('replicate_movement', True)   # AActor.bReplicateMovement
actor.set_editor_property('net_dormancy', unreal.NetDormancy.DORM_NEVER)
actor.set_editor_property('net_update_frequency', 30.0)

# 컴포넌트 리플리케이트
comp = actor.get_component_by_class(unreal.StaticMeshComponent)
comp.set_editor_property('is_replicated', True)
```

블루프린트 클래스 기본값(CDO)에 적용하려면:
```python
bp = unreal.EditorAssetLibrary.load_asset('/Game/BP_Pickup')
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('replicates', True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
```

## blueprint_modify로 하는 것 — 변수 리플리케이션 / RPC 노드

- **변수의 `Replicated` / `RepNotify` 플래그**: 변수는 `blueprint_modify`로 추가하되, 리플리케이션 조건/RepNotify 함수 바인딩은 그래프·메타 설정이 필요하다. Python `add_member_variable`로 변수만 만들 수 있고 **리플리케이션 플래그 토글은 현재 자동화 한계** — 사용자에게 보고하고 에디터에서 변수 Details의 Replication 드롭다운으로 설정하도록 안내한다.
- **RPC(Server/Client/Multicast 이벤트)**: 커스텀 이벤트는 `blueprint_modify add_node`(CustomEvent)로 만들 수 있으나, **RPC 복제 타입(Server/Reliable 등) 지정은 노드 메타 설정**이며 현재 도구로 토글 불가. 골격(이벤트+로직)까지 만들고 복제 타입 지정은 한계로 보고한다.

## Python으로 불가능한 것 — 추정 금지

- `GetLifetimeReplicatedProps` / `DOREPLIFETIME` 매크로는 C++ 전용. Python에 등가물 없다.
- RPC의 `_Validate`/`WithValidation`, `COND_*` 리플리케이션 조건 — Python API 없음.
- 비슷한 이름의 함수를 루프로 추정하지 말 것. 위 범위를 벗어나면 한계를 명확히 보고하고, C++/에디터 수동 작업이 필요함을 안내한다.

## 권장 흐름

액터/컴포넌트 `replicates` 플래그 + `replicate_movement`까지 Python으로 설정 → 변수/RPC의 복제 타입은 `blueprint_modify`로 골격 생성 후 복제 메타는 한계 보고 → 변경 후 compile + save → `get_output_log`로 경고 확인.
