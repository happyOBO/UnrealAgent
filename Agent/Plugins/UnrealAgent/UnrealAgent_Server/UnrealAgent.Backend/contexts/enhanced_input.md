---
name: enhanced_input
description: Enhanced Input — InputAction/IMC 데이터 에셋 생성 및 매핑 (Python unreal API)
keywords:
  - enhanced input
  - input action
  - 인풋
  - 입력
  - ia_
  - imc_
  - mapping context
  - 매핑 컨텍스트
  - input mapping
  - 키 바인딩
  - keybinding
  - trigger
  - modifier
---

Enhanced Input의 `InputAction`, `InputMappingContext`는 데이터 에셋이므로 AssetTools로 생성한다.

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# InputAction 생성 (값 타입 지정)
ia = asset_tools.create_asset('IA_Jump', '/Game/Input',
                              unreal.InputAction, unreal.InputActionFactory())
ia.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
unreal.EditorAssetLibrary.save_asset(ia.get_path_name())

# InputMappingContext 생성
imc = asset_tools.create_asset('IMC_Default', '/Game/Input',
                               unreal.InputMappingContext, unreal.InputMappingContextFactory())

# 액션에 키 매핑 추가
mapping = imc.map_key(ia, unreal.InputCoreTypes.SPACE_BAR)
# 트리거/모디파이어가 필요하면 mapping에 추가 (구조는 help로 확인)

unreal.EditorAssetLibrary.save_asset(imc.get_path_name())
```

- 값 타입: `BOOLEAN`(버튼), `AXIS1D`(스칼라), `AXIS2D`(이동), `AXIS3D`.
- 팩토리 클래스명이 불확실하면 `dir(unreal)`에서 `*Factory`를 검색하거나 `help`로 확인.
- 키 상수는 `unreal.InputCoreTypes`에 정의(예: `W_KEY`, `SPACE_BAR`, `GAMEPAD_FACE_BUTTON_BOTTOM`).
- 실제 바인딩(액션→함수)은 캐릭터/플레이어컨트롤러 C++/BP에서 처리. Python은 에셋 준비까지.
