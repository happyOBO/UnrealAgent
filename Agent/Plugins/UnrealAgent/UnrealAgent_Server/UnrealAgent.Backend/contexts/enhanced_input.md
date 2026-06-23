---
name: enhanced_input
description: Enhanced Input — creating InputAction/IMC data assets and mappings (Python unreal API)
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

Enhanced Input's `InputAction` and `InputMappingContext` are data assets, so create them with
AssetTools.

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Create an InputAction (specify the value type)
ia = asset_tools.create_asset('IA_Jump', '/Game/Input',
                              unreal.InputAction, unreal.InputActionFactory())
ia.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
unreal.EditorAssetLibrary.save_asset(ia.get_path_name())

# Create an InputMappingContext
imc = asset_tools.create_asset('IMC_Default', '/Game/Input',
                               unreal.InputMappingContext, unreal.InputMappingContextFactory())

# Add a key mapping to the action
mapping = imc.map_key(ia, unreal.InputCoreTypes.SPACE_BAR)
# If triggers/modifiers are needed, add them to the mapping (check the structure with help)

unreal.EditorAssetLibrary.save_asset(imc.get_path_name())
```

- Value types: `BOOLEAN` (button), `AXIS1D` (scalar), `AXIS2D` (movement), `AXIS3D`.
- If a factory class name is uncertain, search `dir(unreal)` for `*Factory` or check with `help`.
- Key constants are defined in `unreal.InputCoreTypes` (e.g. `W_KEY`, `SPACE_BAR`,
  `GAMEPAD_FACE_BUTTON_BOTTOM`).
- Actual binding (action→function) is handled in the character/player-controller C++/BP. Python
  only goes as far as preparing the assets.
