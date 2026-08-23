"""Convert the supplied FBX to an engine-friendly Y-up triangulated OBJ."""

from pathlib import Path
import bmesh
import bpy
from math import radians
from mathutils import Matrix


root = Path(__file__).resolve().parents[1]
source_fbx = Path(r"C:\Users\ADMIN\AppData\Local\Temp\Lancia Delta (1).fbx")
texture_png = root / "assets" / "lancia_delta_volterra.png"
texture_bmp = root / "assets" / "lancia_delta_volterra.bmp"
output_obj = root / "assets" / "lancia_delta_volterra.obj"

if not source_fbx.is_file():
    raise FileNotFoundError(source_fbx)
if not texture_png.is_file():
    raise FileNotFoundError(texture_png)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source_fbx))

meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
z_up_rotation = Matrix.Rotation(radians(90), 4, "X")
for obj in meshes:
    # FBX использует Y-up. Превращаем мировые координаты вершин в Blender Z-up
    # явно: bpy.ops.transform_apply здесь не применял поворот к данным меша.
    world_to_z_up = z_up_rotation @ obj.matrix_world
    for vertex in obj.data.vertices:
        vertex.co = world_to_z_up @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(obj.data)
    bm.free()

# The texture is converted to BMP before this script runs. Bind it as a single
# material across the imported parts so the generated MTL has one predictable
# texture reference.
texture = bpy.data.images.load(str(texture_bmp), check_existing=False)
material = bpy.data.materials.new("Volterra_Racing")
material.use_nodes = True
nodes = material.node_tree.nodes
links = material.node_tree.links
for node in nodes:
    nodes.remove(node)
out = nodes.new("ShaderNodeOutputMaterial")
bsdf = nodes.new("ShaderNodeBsdfPrincipled")
image_node = nodes.new("ShaderNodeTexImage")
image_node.image = texture
links.new(image_node.outputs["Color"], bsdf.inputs["Base Color"])
links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

for obj in meshes:
    obj.data.materials.clear()
    obj.data.materials.append(material)
    obj.select_set(True)

bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.wm.obj_export(filepath=str(output_obj), export_materials=True)

triangles = sum(len(obj.data.polygons) for obj in meshes)
vertices = sum(len(obj.data.vertices) for obj in meshes)
print(f"Exported {output_obj}: {vertices} vertices, {triangles} triangles")
