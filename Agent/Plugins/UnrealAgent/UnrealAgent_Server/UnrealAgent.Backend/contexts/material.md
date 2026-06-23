---
name: material
description: Material instance creation / parameter setting / assignment (Python unreal API)
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

Use `unreal.MaterialEditingLibrary` for material instance creation and parameter setting.

```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
parent = unreal.EditorAssetLibrary.load_asset('/Game/Materials/M_Master')

# Create a Material Instance Constant (the editor-saved kind)
factory = unreal.MaterialInstanceConstantFactoryNew()
mi = asset_tools.create_asset('MI_Wood_Dark', '/Game/Materials',
                              unreal.MaterialInstanceConstant, factory)

# Set parent
unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)

# Set parameters (a parameter of the same name must be exposed on the parent material)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.5)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
    mi, 'BaseColor', unreal.LinearColor(1.0, 0.0, 0.0, 1.0))
tex = unreal.EditorAssetLibrary.load_asset('/Game/Textures/T_Wood')
unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, 'Albedo', tex)

# Save
unreal.EditorAssetLibrary.save_asset(mi.get_path_name())

# Assign to an actor's mesh
smc = actor.static_mesh_component
smc.set_material(0, mi)   # slot index 0
```

- Parameter names must **exactly match the parameter names exposed on the parent material**. If
  unknown, query with `unreal.MaterialEditingLibrary.get_scalar_parameter_names(parent)` /
  `get_vector_parameter_names(parent)`.
- For runtime dynamic changes use `create_dynamic_material_instance`, but for editor asset work
  use the constant approach above.
- Color is always `unreal.LinearColor` (0–1 range); do not confuse it with `unreal.Color`
  (0–255).
