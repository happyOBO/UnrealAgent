---
name: character
description: 캐릭터 데이터 에셋 / 스탯 DataTable 생성·조회 (Python unreal API)
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

캐릭터 스탯/구성은 `DataAsset` 또는 `DataTable`로 관리한다.

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# DataTable 생성 (행 구조체 지정 — C++/BP에 정의된 USTRUCT가 필요)
row_struct = unreal.EditorAssetLibrary.load_asset('/Game/Data/CharacterStatRow')  # ScriptStruct
factory = unreal.DataTableFactory()
factory.set_editor_property('struct', row_struct)
dt = asset_tools.create_asset('DT_CharacterStats', '/Game/Data',
                              unreal.DataTable, factory)

# 행 추가/조회는 DataTableFunctionLibrary 사용
unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)  # 행 이름 목록
# 값 채우기는 JSON 임포트가 가장 안정적:
unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json_string)

# 캐릭터 무브먼트 속성 (액터 인스턴스/CDO에서)
char = actor   # ACharacter
move = char.character_movement   # CharacterMovementComponent
move.set_editor_property('max_walk_speed', 600.0)
move.set_editor_property('jump_z_velocity', 420.0)
```

- DataTable 행 구조체(USTRUCT)는 Python으로 정의 불가 — C++/BP에 미리 있어야 한다. 없으면 사용자에게 알린다.
- 스탯 데이터 입력은 `fill_data_table_from_json_string`(JSON)이 가장 견고하다.
- 변경 후 `save_asset` 필수.
- 캐릭터 클래스 자체(이동/카메라 로직)는 블루프린트 한계를 따르므로 [[blueprint]] 컨텍스트 참고.
