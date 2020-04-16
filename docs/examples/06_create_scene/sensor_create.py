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
#     },
#     "shape1" : {
#         "type" : "ply",
#         "filename" : "resources/data/ply/teapot.ply"
#     }
# })
# print(mesh)

# point_emitter = pmgr.create({
#     "emitter" : {
#         "type" : "point",
#         "to_world" : Transform4f.translate([2,-6,4]),
#         # "rgb" : {
#         #     "name" : "intensity",
#         #     "value" : [0.5, 0.2, 0.5] #10 #Texture.D65(10)
#         # }
#         "spectrum" : {
#             "name" : "intensity",
#             "value" : 10.0
#         }
#     }
# })
# print(point_emitter)

# check area emitters
primitive_shape = pmgr.create({
  "shape" : {
    "type" : "sphere",
    # "center" : Point3f(0, 0, -10),
    "radius" : 10.0,
    "flip_normals" : False,
    "emitter" : {
      "type" : "area",
      # "spectrum" : {
      #   "radiance" : "file.spd"#10.0 #"400:0.1, 700:0.2"
      # }
      "rgb" : {
      "radiance" : Color3f(0.5, 0.2, 0.5) # [0.5, 0.2, 0.5]
      }
    }
  }
})

print(primitive_shape)
print(primitive_shape.is_emitter())


# Spectrum
# spec = pmgr.create({
#   "spectrum" : {
#     "type" : "d65"
#   }
# })
# print(spec)
######
####

# bsdf = pmgr.create({
#   "bsdf" : {
#     "type" : "d65"
#   }
# })

# bsdf = pmgr.create({
#   "bsdf" : {
#     "type" : "blackbody",
#     "temperature" : 5000
#   }
# })

# bsdf = pmgr.create({
#   "bsdf" : {
#     "type" : "srgb_d65", #"srgb"
#     "color" : Color3f(1,1,1)
#   }
# })

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