---
name: animation
description: 애님 시퀀스/몽타주/노티파이 및 애님 BP의 Python 한계 (Python unreal API)
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
  - animinstance
  - retarget
---

**스테이트 머신 구성은 Python이 아니라 `anim_blueprint_modify` 네이티브 도구를 사용한다.** Python으로는 애님 BP 그래프 편집이 불가능하다. 스테이트 머신 골격은 이 도구로 구성한다:
- `create_state_machine` (state_machine 이름; AnimGraph 출력 포즈에 자동 연결)
- `add_state` (state_machine, state_name)
- `set_state_animation` (state_machine, state_name, anim_sequence 경로)
- `set_entry_state` (진입 스테이트)
- `add_transition` (from_state, to_state; 조건 그래프는 비어 있게 생성 — 조건 설정은 후속)

전형적 흐름: `create_state_machine` → `add_state`×2 → 각 스테이트 `set_state_animation` → `set_entry_state` → `add_transition`.

**AnimGraph 조회/추가는 `anim_blueprint_modify` 네이티브 도구로 한다** (Python으로 불가능).
- 조회: `get_anim_graph` (읽기 전용) — 노드 type/node_id/slot_name/state_machine/핀 연결을 반환. **Python으로 핀/GUID/연결을 introspection하려 하지 말 것** (`get_all_pins`/`export_text`/`pins` 속성 없음, 시간 낭비). 또한 확인 목적으로 에디터를 열지 말 것 — 잘못하면 크래시한다.

메인 AnimGraph 대상이며 `add_*`는 노드 id(NodeGuid)를 반환한다:
- `add_slot_node` (slot_name) — 몽타주 슬롯 재생 노드
- `add_layered_blend_per_bone` (bones: 쉼표 구분 본 이름) — 본별 레이어 블렌드 노드
- `connect_anim_nodes` (from_node_id, to_node_id; to에 `output`/`result`를 주면 Output Pose에 연결; from_pin/to_pin 생략 시 첫 출력/입력 포즈 핀)

**몽타주 슬롯 트랙 편집은 `montage_modify` 네이티브 도구로 한다.** Python은 `SlotAnimTracks`가 protected로 차단되어 슬롯 변경이 불가능하다. 이 도구는 C++에서 직접 조작한다:
- `create` (montage_path, skeleton 또는 anim_sequence)
- `add_slot` / `rename_slot` (slot_name, new_slot_name) / `add_segment` (slot_name, anim_sequence, start_time)
- `get_info` — 슬롯/세그먼트 구성 조회

애님 시퀀스 조회, 노티파이 추가, 커브 조작 등 데이터 수준 작업은 아래 Python으로 가능하다.

```python
import unreal

# 스켈레탈 메시 / 애님 에셋 로드
anim_seq = unreal.EditorAssetLibrary.load_asset('/Game/Anims/Run')   # AnimSequence

# 길이/프레임 조회
length = anim_seq.get_play_length()
fps = unreal.AnimationLibrary.get_frame_rate(anim_seq)
num_frames = unreal.AnimationLibrary.get_num_frames(anim_seq)

# 노티파이 추가
unreal.AnimationLibrary.add_animation_notify_event(
    anim_seq, 'Footstep', 0.5, unreal.AnimNotify())

# 애님 몽타주 생성 (시퀀스로부터)
montage = unreal.AnimationLibrary.create_anim_montage(anim_seq)

# 스켈레톤 본 조회
skeleton = anim_seq.get_editor_property('target_skeleton')
```

- 애님 BP 로직(상태 전이, 블렌드)이 필요하면 Python 한계를 명확히 알리고, 데이터 자산 준비까지만 자동화한다.
- 노티파이/커브 작업 후 `save_asset` 필수.
- API 시그니처가 불확실하면 `dir(unreal.AnimationLibrary)` / `help(...)`로 먼저 확인.
