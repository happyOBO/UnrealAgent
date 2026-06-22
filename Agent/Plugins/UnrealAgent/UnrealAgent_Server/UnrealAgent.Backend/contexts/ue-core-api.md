---
name: ue-core-api
description: UE5 Python unreal API 코어 — 에디터 서브시스템/수학 타입/클래스 계층/공통 함정 (Python unreal API)
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

UE5 Python(`unreal` 모듈) 코어 패턴. 모든 도메인 작업의 기반이며, **에디터 서브시스템을 우선 사용**하고 deprecated 라이브러리(`EditorLevelLibrary`, `EditorAssetLibrary`의 일부)는 가급적 피한다.

## 에디터 서브시스템 (UE5 권장 진입점)

```python
import unreal

actor_sub  = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)    # 액터 스폰/조회/삭제
asset_sub  = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)    # 에셋 로드/저장/복제
level_sub  = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)    # 레벨 열기/저장/PIE
util_sub   = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)  # 에디터 유틸리티 위젯
```

`unreal.get_editor_subsystem(...)`가 None을 반환하면(서브시스템 미가용) 한계를 보고하고 추정 우회하지 말 것.

## 수학 타입 — 커스텀 구현 금지, unreal 타입 사용

```python
loc  = unreal.Vector(0, 0, 100)            # X, Y, Z (cm, double). Z-up.
rot  = unreal.Rotator(pitch, yaw, roll)    # 도(degree). 인자 순서 주의!
xform = unreal.Transform(location=loc, rotation=rot, scale=unreal.Vector(1,1,1))
col  = unreal.LinearColor(1.0, 0.0, 0.0, 1.0)   # 머티리얼/라이트는 LinearColor
q    = rot.quaternion()                    # 필요 시 쿼터니언
# 수학 헬퍼는 unreal.MathLibrary 사용 (직접 삼각함수 구현 금지)
```

## 프로퍼티 접근 — 항상 get/set_editor_property

```python
# 직접 속성 접근은 pre/post edit 콜백을 건너뛰어 에디터 갱신이 누락된다. 반드시:
comp.set_editor_property('cast_shadow', False)
val = comp.get_editor_property('relative_location')
```

## 컴포넌트 조회 (런타임 인스턴스)

```python
# 액터에서 클래스로 컴포넌트 찾기
smc = actor.get_component_by_class(unreal.StaticMeshComponent)
comps = actor.get_components_by_class(unreal.SceneComponent)   # 복사본 배열
# 루트/소유자
root = actor.get_editor_property('root_component')
```

## 클래스 계층 요지 (자주 쓰는 것)

```
AActor → APawn → ACharacter | AController → APlayerController | AGameModeBase | ALight
UActorComponent → USceneComponent → UPrimitiveComponent → UMeshComponent
                  → UStaticMeshComponent | USkeletalMeshComponent
                  → UShapeComponent → UBox/USphere/UCapsuleComponent
                  → UCameraComponent | USpringArmComponent | UMovementComponent
UObject → UUserWidget | UDataAsset | UAnimInstance | UBlueprintFunctionLibrary
```
클래스 참조는 `unreal.StaticMeshActor`처럼 직접 쓰거나, 에셋의 generated class는 `bp.generated_class()`.

## 흔한 함정 — 추정/brute-force 금지

- 문서에 없는 클래스/프로퍼티명을 루프로 추정하지 말 것. 에디터를 얼리고 시간만 낭비한다. 모르면 `help(unreal.SomeClass.method)` / `dir(obj)`로 **한 번** 확인하고, 없으면 한계를 보고한다.
- 다수 항목 순회는 반드시 `unreal.ScopedSlowTask`로 감싸 취소 가능하게 한다.
- 변경 후 에셋은 `asset_sub.save_asset(path)` 필수. 블루프린트는 compile 후 save.
- `print()`만이 결과를 보는 유일한 수단. 변경 후 `capture_viewport`(시각) / `get_output_log`(경고·에러)로 자가검증.
- 그래프/스테이트 머신/슬롯 등 **Python으로 불가능한 영역은 네이티브 도구**(`blueprint_modify`, `anim_blueprint_modify`, `montage_modify`, `widget_modify`)를 쓴다.
