---
name: character
description: Character data asset / stat DataTable creation & query (Python unreal API)
keywords:
  - character
  - 캐릭터
  - 캐릭
  - data asset
  - 데이터 에셋
  - datatable
  - data table
  - 데이터 테이블
  - stats
  - 스탯
  - 능력치
  - pawn
  - movement component
  - 캐릭터 무브먼트
---

Manage character stats/configuration with a `DataAsset` or `DataTable`.

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Create a DataTable (specify the row struct — requires a USTRUCT defined in C++/BP)
row_struct = unreal.EditorAssetLibrary.load_asset('/Game/Data/CharacterStatRow')  # ScriptStruct
factory = unreal.DataTableFactory()
factory.set_editor_property('struct', row_struct)
dt = asset_tools.create_asset('DT_CharacterStats', '/Game/Data',
                              unreal.DataTable, factory)

# Use DataTableFunctionLibrary to add/query rows
unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)  # list of row names
# Filling values is most reliable via JSON import:
unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json_string)

# Character movement properties (from an actor instance/CDO)
char = actor   # ACharacter
move = char.character_movement   # CharacterMovementComponent
move.set_editor_property('max_walk_speed', 600.0)
move.set_editor_property('jump_z_velocity', 420.0)
```

- The DataTable row struct (USTRUCT) cannot be defined via Python — it must already exist in
  C++/BP. If it does not, tell the user.
- For entering stat data, `fill_data_table_from_json_string` (JSON) is the most robust.
- `save_asset` is required after changes.
- The character class itself (movement/camera logic) follows the Blueprint limits, so see the
  [[blueprint]] context.
