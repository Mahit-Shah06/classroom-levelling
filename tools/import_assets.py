# Run inside Unreal (editor Python, via tools/import_assets.sh): imports Assets/Generated/<id>.glb into /Game/Generated/<id>/.
# The game finds the StaticMesh in that folder through the asset registry (no renames: renaming mid-render crashed the editor).
import os, unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC = os.path.join(ROOT, "Assets", "Generated")
lib = unreal.EditorAssetLibrary
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".glb"):
        continue
    dst = f"/Game/Generated/{f[:-4]}"
    if lib.does_directory_have_assets(dst):
        unreal.log(f"[import] skip {f}")
        continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = dst
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    n = len(lib.list_assets(t.destination_path, recursive=True))
    unreal.log(f"[import] {'OK' if n else 'FAILED'} {os.path.basename(t.filename)} ({n} assets)")
lib.save_directory("/Game/Generated", only_if_is_dirty=True, recursive=True)
unreal.SystemLibrary.quit_editor()
