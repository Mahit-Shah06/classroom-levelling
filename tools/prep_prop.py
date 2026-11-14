# Blender (headless): shrink a raw Rodin prop for the game build (<= 12k triangles, 1024 px textures) and render a preview.
#   blender -b --python tools/prep_prop.py -- <in.glb> <out.fbx> [preview.png]
import bpy, sys, math
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=a[0])
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
import os
ob.name = ob.data.name = os.path.splitext(os.path.basename(a[1]))[0]  # the game loads the mesh by this name
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
for o in list(bpy.data.objects):
    if o != ob:
        bpy.data.objects.remove(o)

def weld(o):
    """Rodin meshes are split along UV seams; decimating them unwelded tears the surface apart."""
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=1e-5)
    bpy.ops.object.mode_set(mode="OBJECT")
    if o.data.has_custom_normals:
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    bpy.ops.object.shade_smooth_by_angle(angle=1.05)

weld(ob)
tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
budget = 16000 if ob.name.startswith("city_") else 12000  # 70 skyline towers are always on screen
if tris > budget:
    m = ob.modifiers.new("dec", "DECIMATE")
    m.ratio = budget / tris
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.object.shade_smooth_by_angle(angle=1.05)
# Base colour only: after decimation the baked normal/metal maps no longer match the surface and shade as black shards in UE.
for mat in ob.data.materials:
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    for name, val in (("Metallic", 0.0), ("Roughness", 0.75), ("Normal", None)):
        for l in list(bsdf.inputs[name].links):
            nt.links.remove(l)
        if val is not None:
            bsdf.inputs[name].default_value = val
    for n in list(nt.nodes):
        if n.type in ("NORMAL_MAP", "SEPARATE_COLOR") or (n.type == "TEX_IMAGE" and not n.outputs["Color"].links and not n.outputs["Alpha"].links):
            nt.nodes.remove(n)
for img in list(bpy.data.images):
    if img.users == 0:
        bpy.data.images.remove(img)
for img in bpy.data.images:
    if max(img.size) > 1024:
        img.scale(1024, 1024)
    # UE names the texture after the embedded file; without a real file name it imports a 32x32 placeholder.
    img.name = ob.name + "_d"
    img.filepath_raw = "//" + ob.name + "_d.png"
    img.file_format = "PNG"
    img.pack()
if len(a) > 2:
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    c = sum(bb, Vector()) / 8
    r = max((v - c).length for v in bb)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    bpy.context.scene.collection.objects.link(cam)
    cam.location = c + Vector((1.6, -2.2, 1.1)).normalized() * r * 3.2
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc = bpy.context.scene
    sc.camera = cam
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.color_type = "TEXTURE"
    sc.render.resolution_x = sc.render.resolution_y = 512
    sc.render.filepath = a[2]
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
# FBX + UE's FBX importer: the glTF route (Interchange) gave black materials and scrambled UVs in UE 5.7.
bpy.ops.object.select_all(action="DESELECT")
ob.select_set(True)
bpy.ops.export_scene.fbx(filepath=a[1], use_selection=True, bake_anim=False, path_mode="COPY", embed_textures=True,
                         mesh_smooth_type="OFF", object_types={"MESH"})
print("[prop] OK", a[1])
