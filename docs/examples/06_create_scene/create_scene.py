import mitsuba
import enoki as ek

# Set the desired mitsuba variant
# mitsuba.set_variant('packet_spectral')
mitsuba.set_variant('scalar_rgb')

from mitsuba.core import (Thread, Properties, Class, 
                          PluginManager, xml, Transform4f, 
                          AnimatedTransform, Point3f, Struct,
                          Vector3f)
from mitsuba.render import (Scene, SamplingIntegrator,
                            Sensor, Film, Mesh, Emitter,
                            Shape, Texture)


from ipdb import set_trace

pgmr = PluginManager.instance()

props = Properties()
props.set_id("hello")

# integrator
props_integrator = Properties("direct")
props_integrator['type'] = "direct"
props_integrator['emitter_samples'] = 4
props_integrator['bsdf_samples'] = 0
# obj = SamplingIntegrator(props_integrator)
# obj1 = pgmr.create_object(props_integrator, obj.class_())
obj1 = pgmr.create_object(props_integrator, "integrator")
props['integrator'] = obj1
print("Created integrator")

# camera
props_cam = Properties("perspective")
# props_cam['type'] = 'perspective'
props_cam['near_clip'] = float(1)
props_cam['far_clip'] = float(1000)

# transform
cam_trans = Transform4f.look_at(origin=[0.0, -12.0, 1.5], target=[0.5, 0.0, 1.5], up=[0.0, 0.0, 1.0])
props_cam['to_world'] = AnimatedTransform(cam_trans)
# film - doesn't have a cstor
props_film = Properties("hdrfilm")
props_film['width'] = 1024
props_film['height'] = 768
# obj = Film(props_film)
film = pgmr.create_object(props_film, "film") #, Film.mro()[0])
props_cam['film'] = film

cam = Sensor(props_cam)
# obj1 = pgmr.create_object(props_cam, cam.class_())
obj1 = pgmr.create_object(props_cam, "sensor")
props['sensors'] = obj1

# add mesh - doesn't have a cstor
props_shape = Properties("ply")
props_shape['filename'] = "resources/data/ply/teapot.ply"
# mesh_trans = Transform4f.rotate(axis= Vector3f([0,1,1]), angle= float(15))*Transform4f.rotate(vec=[0,0,1], angle=-15)
# props_shape['to_world'] = mesh_trans
obj = Shape(props_shape)

print("Trying to load ply")
obj1 = pgmr.create_object(props_shape, "shape") #, obj.class_())
props['shapes'] = obj1
print("Loading ply successful")

# point emitter
props_em1 = Properties("point")
props_em1['position'] = Vector3f([2, -6, 4.5])
props_em1['intensity'] = Texture.D65(10)
em_obj = Emitter(props_em1)
obj1 = pgmr.create_object(props_em1, "emitter") #, em_obj.class_())
# add to scene
props["emitters1"] = (obj1)

# point emitter
props_em2 = Properties("point")
props_em2['intensity'] = Texture.D65(10)
props_em2['position'] = Vector3f([2, -6, 4.5])
obj = Emitter(props_em1)
obj2 = pgmr.create_object(props_em2, "emitter") #, em_obj.class_())
# add to scene
props["emitters2"] = obj2

# create instances
scene = Scene(props)
# add to scene graph
obj1 = pgmr.create_object(props, scene.class_())
assert type(obj1) is Scene

print(pgmr.loaded_plugins())  

set_trace()

scene.integrator().render(scene, scene.sensors()[0])
film = scene.sensors()[0].film()

# Write out rendering as high dynamic range OpenEXR file
film.set_destination_file('/tmp/output.exr')
film.develop()
