# Run inside Unreal (editor Python, via tools/import_assets.sh): imports Assets/Generated/<id>.fbx as /Game/Props/<id>/<id>.
import os, unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC = os.path.join(ROOT, "Assets", "Generated")
lib = unreal.EditorAssetLibrary
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".fbx"):
        continue
    dst = f"/Game/Props/{f[:-4]}"
    only = os.environ.get("REIMPORT", "")  # REIMPORT=all or a comma list of ids: import over what is already there
    if lib.does_directory_have_assets(dst) and only != "all" and f[:-4] not in only.split(","):
        unreal.log(f"[import] skip {f}")
        continue
    if lib.does_directory_exist(dst):  # a reimport would keep the old materials and textures
        lib.delete_directory(dst)
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = dst
    t.destination_name = f[:-4]
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.import_materials = True
    o.import_textures = True
    o.import_animations = False
    o.static_mesh_import_data.set_editor_property("combine_meshes", True)
    o.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
    t.options = o
    t.factory = unreal.FbxFactory()
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    ok = isinstance(unreal.load_asset(f"{t.destination_path}/{t.destination_name}"), unreal.StaticMesh)
    for p in lib.list_assets(t.destination_path, recursive=True):
        m = unreal.load_asset(p)
        if isinstance(m, unreal.Material):  # props are drawn as instanced meshes; without this flag UE shows its grey grid material
            m.set_editor_property("used_with_instanced_static_meshes", True)
            unreal.MaterialEditingLibrary.recompile_material(m)
    unreal.log(f"[import] {'OK' if ok else 'FAILED'} {t.destination_name}")
lib.save_directory("/Game/Props", only_if_is_dirty=True, recursive=True)
unreal.SystemLibrary.quit_editor()
