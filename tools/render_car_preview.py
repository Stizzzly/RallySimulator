"""Render a quick silhouette check for car.obj with Blender."""
from pathlib import Path
import bpy
from math import radians
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(root / "car.obj"))
car = bpy.context.selected_objects[0]
car.rotation_euler.x = radians(90)  # game Y-up -> Blender Z-up

body = bpy.data.materials.new("Rally white")
body.diffuse_color = (0.82, 0.04, 0.03, 1)
car.data.materials.append(body)

for x in (-1.0, 1.0):
    for y in (-1.32, 1.35):
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=.38, depth=.36, location=(x, y, .30), rotation=(0, radians(90), 0))
        wheel = bpy.context.object
        tire = bpy.data.materials.new("Tire")
        tire.diffuse_color = (.025, .025, .025, 1)
        wheel.data.materials.append(tire)

bpy.ops.object.light_add(type='AREA', location=(4, 1, 6))
bpy.context.object.data.energy = 900
bpy.context.object.data.shape = 'DISK'
bpy.context.object.data.size = 5
bpy.ops.object.light_add(type='AREA', location=(-4, 4, 3))
bpy.context.object.data.energy = 500
bpy.context.object.data.size = 4

bpy.ops.object.camera_add(location=(5.4, 7.8, 3.5))
camera = bpy.context.object
bpy.context.scene.camera = camera
target = (0, 0.2, .8)
direction = Vector(target) - camera.location
camera.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()

scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.render.resolution_x = 900
scene.render.resolution_y = 650
scene.render.resolution_percentage = 100
scene.world = bpy.data.worlds.new("Preview world")
scene.world.color = (.06, .06, .06)
scene.render.filepath = str(root / "car_preview.png")
bpy.ops.render.render(write_still=True)
