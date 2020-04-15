import enoki as ek
import mitsuba
mitsuba.set_variant('gpu_autodiff_rgb')

from mitsuba.core import Thread, Color3f
from mitsuba.core.xml import load_file, load_string
from mitsuba.python.util import traverse
from mitsuba.python.autodiff import render, write_bitmap, Adam
import time

def write_gradient_image(grad, name):
    """Convert signed floats to blue/red gradient exr image"""
    from mitsuba.core import Bitmap

    convert_to_rgb = True

    if convert_to_rgb:
        # Compute RGB channels for .exr image (no grad = black)
        grad_R = grad.copy()
        grad_R[grad_R < 0] = 0.0
        grad_B = grad.copy()
        grad_B[grad_B > 0] = 0.0
        grad_B *= -1.0
        grad_G = grad.copy() * 0.0

        grad_np = np.concatenate((grad_R, grad_G, grad_B), axis=2)
    else:
        grad_np = np.concatenate((grad, grad, grad), axis=2)

    print('Writing', name + ".exr")
    Bitmap(grad_np).write(name + ".exr")

def render_gradient(scene, passes, diff_params):
    fsize = scene.sensors()[0].film().size()

    img  = np.zeros((fsize[1], fsize[0], 3), dtype=np.float32)
    grad = np.zeros((fsize[1], fsize[0], 1), dtype=np.float32)
    for i in range(passes):
        img_i = render(scene)
        ek.forward(diff_params, i == passes - 1)

        grad_i = ek.gradient(img_i).numpy().reshape(fsize[1], fsize[0], -1)[:, :, [0]]
        img_i = img_i.numpy().reshape(fsize[1], fsize[0], -1)

        # Remove NaNs
        grad_i[grad_i != grad_i] = 0
        img_i[img_i != img_i] = 0

        grad += grad_i
        img += img_i

    return img / passes, grad / passes

def make_scene(param):
        return load_string("""
            <scene version="2.0.0">
                <integrator type="pathreparam">
                                 <integer name="max_depth" value="2"/>
                </integrator>
                <sensor type="perspective">
                    <string name="fov_axis" value="smaller"/>
                    <float name="near_clip" value="0.1"/>
                    <float name="far_clip" value="2800"/>
                    <float name="focus_distance" value="1000"/>
                    <transform name="to_world">
                        <lookat origin="0, 0, 10" target="0, 0, 0" up="0, 1, 0"/>
                    </transform>
                    <float name="fov" value="10"/>
                    <sampler type="independent">
                        <integer name="sample_count" value="4"/>
                    </sampler>
                    <film type="hdrfilm">
                        <integer name="width" value="64"/>
                        <integer name="height" value="64"/>
                        <rfilter type="box"/>
                    </film>
                </sensor>

                <shape type="obj" id="light_shape">
                    <transform name="to_world">
                        <rotate x="1" angle="180"/>
                        <translate x="10.0" y="0.0" z="15.0"/>
                        <translate x="{}" y="{}" z="{}"/>
                    </transform>
                    <string name="filename" value="resources/data/obj/xy_plane.obj"/>
                    <emitter type="smootharea" id="smooth_area_light">
                        <spectrum name="radiance" value="100"/>
                    </emitter>
                </shape>

                <shape type="obj" id="object">
                    <string name="filename" value="resources/data/obj/smooth_empty_cube.obj"/>
                    <transform name="to_world">
                        <translate z="1.0"/>
                    </transform>
                </shape>

                <shape type="obj" id="planemesh">
                    <string name="filename" value="resources/data/obj/xy_plane.obj"/>
                    <transform name="to_world">
                        <scale value="2.0"/>
                    </transform>
                </shape>
            </scene>
        """.format(param, param, param))


scene = make_scene(0.0)
diff_param = Float(0.0)
ek.set_requires_gradient(diff_param)

# Update vertices so that they depend on diff_param
params = traverse(scene)
t = Transform4f.translate(Vector3f(1.0) * diff_param)
vertex_positions = params['light_shape.vertex_positions']
vertex_positions_t = t.transform_point(vertex_positions)
params['light_shape.vertex_positions'] = vertex_positions_t

# Update the scene
params.update()

diff_passes = 10
img, grad = render_gradient(scene, diff_passes, diff_param)
write_gradient_image(grad, "light_pos")