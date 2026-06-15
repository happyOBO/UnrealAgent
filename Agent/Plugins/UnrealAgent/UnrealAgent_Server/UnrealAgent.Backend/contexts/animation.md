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

**한계:** 애님 블루프린트의 **스테이트 머신·블렌드 그래프 노드 편집은 Python으로 불가**하다. 가능한 것은 애님 시퀀스 조회, 노티파이 추가, 몽타주 생성, 커브 조작 등 데이터 수준 작업이다.

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
