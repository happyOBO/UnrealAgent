---
name: material
description: 머티리얼 인스턴스 생성/파라미터 설정/할당 (Python unreal API)
keywords:
  - material
  - 머티리얼
  - 재질
  - mat_inst
  - material instance
  - 인스턴스
  - shader
  - 셰이더
  - color
  - 색
  - 컬러
  - roughness
  - metallic
  - texture parameter
  - scalar parameter
  - vector parameter
---

머티리얼 인스턴스 생성·파라미터 설정은 `unreal.MaterialEditingLibrary`를 사용한다.

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
parent = unreal.EditorAssetLibrary.load_asset('/Game/Materials/M_Master')

# 머티리얼 인스턴스 컨스턴트(에디터 저장형) 생성
factory = unreal.MaterialInstanceConstantFactoryNew()
mi = asset_tools.create_asset('MI_Wood_Dark', '/Game/Materials',
                              unreal.MaterialInstanceConstant, factory)

# 부모 지정
unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)

# 파라미터 설정 (부모 머티리얼에 동일 이름의 파라미터가 노출돼 있어야 함)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.5)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
    mi, 'BaseColor', unreal.LinearColor(1.0, 0.0, 0.0, 1.0))
tex = unreal.EditorAssetLibrary.load_asset('/Game/Textures/T_Wood')
unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, 'Albedo', tex)

# 저장
unreal.EditorAssetLibrary.save_asset(mi.get_path_name())

# 액터 메시에 할당
smc = actor.static_mesh_component
smc.set_material(0, mi)   # 슬롯 인덱스 0
```

- 파라미터 이름은 **부모 머티리얼에 노출된 파라미터명과 정확히 일치**해야 한다. 모르면 `unreal.MaterialEditingLibrary.get_scalar_parameter_names(parent)` / `get_vector_parameter_names(parent)`로 조회.
- 런타임 동적 변경은 `create_dynamic_material_instance`를 쓰지만, 에디터 에셋 작업은 위 컨스턴트 방식.
- 색은 항상 `unreal.LinearColor` (0~1 범위), `unreal.Color`(0~255)와 혼동 금지.
