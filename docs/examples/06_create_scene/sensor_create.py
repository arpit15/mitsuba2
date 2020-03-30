import mitsuba
mitsuba.set_variant('scalar_rgb')
from mitsuba.core import *
from mitsuba.render import *
from ipdb import set_trace

pmgr = PluginManager.instance()
# tex = Texture.D65(10)
# print(type(tex))
# set_trace()

# integrator = pmgr.create({
#     "integrator": {
#         "type": "path",
#         "emitter_samples": 4,
#         "bsdf_samples" : 0
#     }
# })
# print(integrator)

# trao  = Transform4f.look_at(
#             Point3f(0, 0, -10),
#             Point3f(0, 0, 0),
#             Vector3f(0, 1, 0)
#         )

# camera = pmgr.create({
#     "sensor": {
#         "type" : "perspective",
#         "to_world" : trao,
#         "film" : {
#             "type" : "hdrfilm",
#             "width" : 1920,
#             "height" : 1080
#         }
#     }
# })
# print(camera)

# mesh = pmgr.create({
#     "shape" : {
#         "type" : "ply",
#         "filename" : "resources/data/ply/teapot.ply"
#     }
# })
# print(mesh)

point_emitter = pmgr.create({
    "emitter" : {
        "type" : "point",
        "to_world" : Transform4f.translate([2,-6,4]),
        "spectrum" : {
            "name" : "intensity",
            "value" : Texture.D65(10)
        }
    }
})
print(point_emitter)

# check area emitters

####

# set_trace()