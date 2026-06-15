---
name: asset
description: 에셋 생성/로드/저장/복제/임포트 및 에셋 레지스트리 조회 (Python unreal API)
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

에셋 조작은 `EditorAssetSubsystem`(또는 호환 `EditorAssetLibrary`)과 `AssetTools`, 조회는 `AssetRegistry`를 쓴다.

```python
import unreal

asset_sub = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

# 존재 확인 / 로드 / 저장
if asset_sub.does_asset_exist('/Game/BP_Door'):
    obj = asset_sub.load_asset('/Game/BP_Door')
asset_sub.save_asset('/Game/BP_Door')

# 복제 / 이름변경 / 삭제
asset_sub.duplicate_asset('/Game/SM_A', '/Game/SM_A_Copy')
asset_sub.rename_asset('/Game/Old', '/Game/New')
asset_sub.delete_asset('/Game/Unused')   # 파괴적 — 참조 확인 후

# 에셋 레지스트리로 검색 (디스크 스캔 없이 빠름)
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.wait_for_completion()   # 초기 스캔 보장
assets = ar.get_assets_by_path('/Game/Meshes', recursive=True)
for ad in assets:
    name = ad.asset_name           # FName
    cls = ad.asset_class_path.asset_name
    path = ad.get_full_name()

# 클래스로 필터
filt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'StaticMesh')],
                       recursive_classes=True)
sm_list = ar.get_assets(filt)
```

FBX/텍스처 임포트:
```python
task = unreal.AssetImportTask()
task.filename = r'C:/Models/chair.fbx'
task.destination_path = '/Game/Imported'
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
```

- 에셋 파일에는 **절대 `os`/`shutil` 등 파이썬 파일 모듈을 쓰지 말 것** — 내부 참조가 깨진다. 항상 `unreal` API 경유.
- 경로는 `/Game/...` (콘텐츠 루트) 형식. 확장자/`.uasset` 없이.
- 의존성: `unreal.EditorAssetLibrary.find_package_referencers_for_asset(path)`로 참조처 확인.
