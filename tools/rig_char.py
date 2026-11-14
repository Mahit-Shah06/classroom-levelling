# Blender (headless): skin a generated A/T-pose character to the UE mannequin skeleton so every mannequin animation plays on it.
#   blender -b --python tools/rig_char.py -- <mannequin.fbx> <character.glb> <out.fbx> [preview.png]
# Method: scale the character to the mannequin, pose the mannequin's arms and legs onto the character's limbs,
# copy skin weights from the posed mannequin body (nearest surface), then make that pose the bind pose.
import bpy, sys, math
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
MANNY, GLB, OUT = argv[0], argv[1], argv[2]
PREVIEW = argv[3] if len(argv) > 3 else None
MAX_TRIS = 20000

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=MANNY)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=GLB)
new = [o for o in bpy.data.objects if o not in before]
meshes = [o for o in new if o.type == "MESH"]


def select(objs, active):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active


select(meshes, meshes[0])
if len(meshes) > 1:
    bpy.ops.object.join()
ch = bpy.context.view_layer.objects.active
ch.name = "Body"
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in new:
    if o != ch and o.name in bpy.data.objects:
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

weld(ch)
tris = sum(len(p.vertices) - 2 for p in ch.data.polygons)
if tris > MAX_TRIS:
    m = ch.modifiers.new("dec", "DECIMATE")
    m.ratio = MAX_TRIS / tris
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.object.shade_smooth_by_angle(angle=1.05)


def bone(n):
    return arm.matrix_world @ arm.data.bones[n].head_local


def world_verts(o):
    return [o.matrix_world @ v.co for v in o.data.vertices]


# --- fit the character to the mannequin: same height, feet on the ground, torso centred ---
vs = world_verts(ch)
zmin = min(v.z for v in vs)
h = max(v.z for v in vs) - zmin
target_h = max(v.z for v in world_verts(body))
s = target_h / h
ch.matrix_world = Matrix.Scale(s, 4) @ Matrix.Translation((0, 0, -zmin))
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
vs = world_verts(ch)
torso = [v for v in vs if 0.55 * target_h < v.z < 0.75 * target_h and abs(v.x - sum(w.x for w in vs) / len(vs)) < 0.2]
cx = sum(v.x for v in torso) / len(torso)
cy = sum(v.y for v in torso) / len(torso)
mt = [v for v in world_verts(body) if 0.55 * target_h < v.z < 0.75 * target_h and abs(v.x) < 0.2]
ch.matrix_world = Matrix.Translation((-cx, sum(v.y for v in mt) / len(mt) - cy, 0))
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
vs = world_verts(ch)


def centroid(pts):
    return sum(pts, Vector()) / len(pts)


def aim(bone_name, end_name, target):
    """Rotate pose bone about its head so that the head->end direction points at target."""
    pivot = bone(bone_name)
    pb_end = arm.matrix_world @ arm.pose.bones[end_name].head
    a, b = (pb_end - pivot).normalized(), (target - pivot).normalized()
    ang = a.angle(b)
    if ang < math.radians(2):
        return 0.0
    rot = Matrix.Rotation(ang, 4, a.cross(b).normalized())
    w = Matrix.Translation(pivot) @ rot @ Matrix.Translation(-pivot)
    pb = arm.pose.bones[bone_name]
    pb.matrix = arm.matrix_world.inverted() @ w @ arm.matrix_world @ pb.matrix
    bpy.context.view_layer.update()
    return math.degrees(ang)


for side, sx in (("l", 1), ("r", -1)):
    sv = [v for v in vs if v.x * sx > 0]
    # hand = the outermost points on that side (works for both T and A pose)
    reach = sorted(sv, key=lambda v: -(v.x * sx))
    hand = centroid(reach[:max(20, len(reach) // 60)])
    a1 = aim("upperarm_" + side, "middle_01_" + side, hand)
    # foot = the lowest points on that side
    low = [v for v in sv if v.z < 0.05 * target_h]
    foot = centroid(low)
    a2 = aim("thigh_" + side, "ball_" + side, Vector((foot.x, bone("ball_" + side).y, foot.z)))
    print(f"[rig] {side}: arm {a1:.0f} deg, leg {a2:.0f} deg")

# --- weights: bake the posed mannequin body, copy its skin weights by nearest surface ---
select([body], body)
for m in list(body.modifiers):
    if m.type == "ARMATURE":
        bpy.ops.object.modifier_apply(modifier=m.name)
select([ch, body], body)
bpy.ops.object.data_transfer(data_type="VGROUP_WEIGHTS", vert_mapping="POLYINTERP_NEAREST",
                             layers_select_src="ALL", layers_select_dst="NAME", mix_mode="REPLACE")
select([ch], ch)
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)

# --- the fitted pose becomes the bind pose ---
select([arm], arm)
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
bpy.data.objects.remove(body)
ch.parent = arm
ch.matrix_parent_inverse = arm.matrix_world.inverted()
ch.modifiers.new("Armature", "ARMATURE").object = arm

if PREVIEW:
    # Swing the limbs so bad weights are obvious in the picture.
    for n, ax, deg in (("upperarm_l", "X", 50), ("upperarm_r", "X", -40), ("thigh_l", "X", 35), ("calf_r", "X", 40), ("lowerarm_r", "Z", 50)):
        pb = arm.pose.bones[n]
        pb.rotation_mode = "XYZ"
        setattr(pb.rotation_euler, ax.lower(), math.radians(deg))
    sc = bpy.context.scene
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    cam.location = (2.6, -3.4, 1.3)
    cam.rotation_euler = (math.radians(84), 0, math.radians(37))
    sc.camera = cam
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.render.resolution_x, sc.render.resolution_y = 700, 900
    sc.render.filepath = PREVIEW
    bpy.ops.render.render(write_still=True)
    for pb in arm.pose.bones:
        pb.rotation_euler = (0, 0, 0)

# UE reads the armature object as the root bone ("root"); the FBX importer's wrapper empty would become an extra bone.
# Its bone data is already in centimetres, so drop the 0.01 wrapper and declare the scene unit as cm.
wrapper = arm.parent
arm.parent = None
arm.matrix_world = Matrix.Identity(4)
if wrapper:
    bpy.data.objects.remove(wrapper)
bpy.context.scene.unit_settings.scale_length = 0.01
bpy.context.view_layer.update()
# Base colour only: after decimation the baked normal/metal maps no longer match the surface and shade as dark shards in UE.
for mat in ch.data.materials:
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    for name, val in (("Metallic", 0.0), ("Roughness", 0.8), ("Normal", None)):
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
for img in bpy.data.images:  # 39 characters must fit a 1 GB jam build
    if max(img.size) > 1024:
        img.scale(1024, 1024)
        img.pack()
select([arm, ch], arm)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, add_leaf_bones=False, bake_anim=False,
                         path_mode="COPY", embed_textures=True, mesh_smooth_type="OFF", object_types={"ARMATURE", "MESH"})
print(f"[rig] OK {OUT} tris={sum(len(p.vertices) - 2 for p in ch.data.polygons)}")
