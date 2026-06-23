---
name: asset
description: Asset create/load/save/duplicate/import and asset registry queries (Python unreal API)
keywords:
  - asset
  - 에셋
  - 애셋
  - import
  - 임포트
  - 가져오기
  - load asset
  - save asset
  - duplicate
  - 복제
  - rename
  - 이름 변경
  - asset registry
  - content browser
  - 콘텐츠
  - fbx
  - texture import
  - find assets
---

For asset manipulation use `EditorAssetSubsystem` (or the compatible `EditorAssetLibrary`) and
`AssetTools`; for queries use `AssetRegistry`.

```python
import unreal

asset_sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

# Existence check / load / save
if asset_sub.does_asset_exist('/Game/BP_Door'):
    obj = asset_sub.load_asset('/Game/BP_Door')
asset_sub.save_asset('/Game/BP_Door')

# Duplicate / rename / delete
asset_sub.duplicate_asset('/Game/SM_A', '/Game/SM_A_Copy')
asset_sub.rename_asset('/Game/Old', '/Game/New')
asset_sub.delete_asset('/Game/Unused')   # destructive — check references first

# Search via the asset registry (fast, no disk scan)
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.wait_for_completion()   # ensure the initial scan
assets = ar.get_assets_by_path('/Game/Meshes', recursive=True)
for ad in assets:
    name = ad.asset_name           # FName
    cls = ad.asset_class_path.asset_name
    path = ad.get_full_name()

# Filter by class
filt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'StaticMesh')],
                       recursive_classes=True)
sm_list = ar.get_assets(filt)
```

FBX/texture import:
```python
task = unreal.AssetImportTask()
task.filename = r'C:/Models/chair.fbx'
task.destination_path = '/Game/Imported'
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
```

- **Never use Python file modules like `os`/`shutil` on asset files** — internal references will
  break. Always go through the `unreal` API.
- Paths are in `/Game/...` (content root) form. No extension / `.uasset`.
- Dependencies: check referencers with
  `unreal.EditorAssetLibrary.find_package_referencers_for_asset(path)`.
