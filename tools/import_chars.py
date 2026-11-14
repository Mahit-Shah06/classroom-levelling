# Run inside Unreal (tools/import_assets.sh chars): imports Assets/Characters/<id>.fbx (rigged by tools/rig_char.py)
# as skeletal meshes on the mannequin skeleton into /Game/Chars/<id>/, so the mannequin AnimBP drives them.
import os, unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC = os.path.join(ROOT, "Assets", "Characters")
lib = unreal.EditorAssetLibrary
skel = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SK_Mannequin")
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".fbx"):
        continue
    dst = f"/Game/Chars/{f[:-4]}"
    only = os.environ.get("REIMPORT", "")  # REIMPORT=all or a comma list of ids: import over what is already there
    if lib.does_directory_have_assets(dst) and only != "all" and f[:-4] not in only.split(","):
        unreal.log(f"[import] skip {f}")
        continue
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    o.skeleton = skel
    o.import_animations = False
    o.import_materials = True
    o.import_textures = True
    o.create_physics_asset = False
    o.skeletal_mesh_import_data.set_editor_property("use_t0_as_ref_pose", False)
    o.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose", False)
    if lib.does_directory_exist(dst):  # a reimport would keep the old materials and textures
        lib.delete_directory(dst)
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = dst
    t.destination_name = f[:-4]
    t.options = o
    t.factory = unreal.FbxFactory()
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    sk = unreal.load_asset(f"{t.destination_path}/{t.destination_name}")
    ok = isinstance(sk, unreal.SkeletalMesh)
    if ok:
        try:  # LODs: forty skinned characters must stay cheap on low-spec machines
            unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).regenerate_lod(sk, 3, False, False)
        except Exception as e:
            unreal.log(f"[import] LOD skipped {t.destination_name}: {e}")
    b = sk.get_bounds().box_extent if ok else None
    unreal.log(f"[import] {'OK' if ok else 'FAILED'} {t.destination_name} skeleton={sk.skeleton.get_name() if ok else None} half-extent={b}")
lib.save_directory("/Game/Chars", only_if_is_dirty=True, recursive=True)
unreal.SystemLibrary.quit_editor()
