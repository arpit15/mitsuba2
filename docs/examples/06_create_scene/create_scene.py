import mitsuba
import enoki as ek

# Set the desired mitsuba variant
# mitsuba.set_variant('packet_spectral')
mitsuba.set_variant('scalar_rgb')

from mitsuba.core import (Thread, Properties, Class, 
                          PluginManager, xml, Transform4f, 
                          AnimatedTransform, Struct,
                          Point3f)
from mitsuba.render import (Scene, SamplingIntegrator,
                            Sensor, Mesh, Emitter,
                            Texture)


pgmr = PluginManager.instance()

props = Properties()
props.set_id("hello")

# integrator
props_integrator = Properties("direct")
props_integrator['type'] = "direct"
props_integrator['emitter_samples'] = 4
props_integrator['bsdf_samples'] = 0
obj = SamplingIntegrator(props_integrator)  # fake
obj1 = pgmr.create_object(props_integrator, obj.class_())
props['integrator'] = obj1

# camera
props_cam = Properties("perspective")
props_cam['near_clip'] = float(1)
props_cam['far_clip'] = float(1000)

# transform
cam_trans = Transform4f.look_at(origin=[0.0, -12.0, 1.5], target=[0.5, 0.0, 1.5], up=[0.0, 0.0, 1.0])
props_cam['to_world'] = AnimatedTransform(cam_trans)
cam = Sensor(props_cam)  # fake
obj1 = pgmr.create_object(props_cam, cam.class_())
props['sensors'] = obj1

# add mesh - doesn't have a cstor
props_shape = Properties("ply")
props_shape['filename'] = "resources/data/ply/teapot.ply"
obj = vertex_struct = Struct() \
        .append("x", Struct.Type.Float32) \
        .append("y", Struct.Type.Float32) \
        .append("z", Struct.Type.Float32)

index_struct = Struct() \
    .append("i0", Struct.Type.UInt32) \
    .append("i1", Struct.Type.UInt32) \
    .append("i2", Struct.Type.UInt32)
obj = Mesh("MyMesh", vertex_struct, 1, index_struct, 1)
    
obj1 = pgmr.create_object(props_shape, obj.class_().parent())
props['shapes'] = obj1

# point emitter
props_em1 = Properties("point")
props_em1['to_world'] = Transform4f.translate([2, -6, 4.5])
props_em1['intensity'] = Texture.D65(10)
em_obj = Emitter(props_em1)
obj1 = pgmr.create_object(props_em1, em_obj.class_())
# add to scene
props["emitters1"] = (obj1)

# create instances
scene = Scene(props)
# add to scene graph
obj1 = pgmr.create_object(props, scene.class_())
assert type(obj1) is Scene

print(pgmr.loaded_plugins())  

scene.integrator().render(scene, scene.sensors()[0])
film = scene.sensors()[0].film()

# Write out rendering as high dynamic range OpenEXR file
film.set_destination_file('/tmp/output.exr')
film.develop()
