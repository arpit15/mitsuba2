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
        # "rgb" : {
        #     "name" : "intensity",
        #     "value" : [0.5, 0.2, 0.5] #10 #Texture.D65(10)
        # }
        "spectrum" : {
            "name" : "intensity",
            "value" : Texture.D65(10)
        }
    }
})
print(point_emitter)

# check area emitters
# primitive_shape = pmgr.create({
#   "shape" : {
#     "type" : "sphere",
#     "center" : Point3f(0, 0, -10),
#     "radius" : 10.0,
#     "flip_normals" : False
#   }
#   })
# print(primitive_shape)
####

# bsdf = pmgr.create({
#   "bsdf" : {
#     "type"
#   }
#   })
# print(bsdf)
# ##3
# sampler = pmgr.create({
#   "sampler" : {
#     "type" : "independent",
#     "sample_count" : 4
#   }
# })
# print(sampler)
# ###
# film = pmgr.create({
#   "film" : {
#     "type" : "hdrfilm",
#     "high_quality_edges" : True,
#     "rfilter" : {
#       "type" : "box"
#     }
#   }
# })
# print(film)

# set_trace()