import mitsuba
mitsuba.set_variant("scalar_rgb")
# mitsuba.set_variant("packet_rgb")

from mitsuba.core import PluginManager, Class, Object
from mitsuba.core.xml import load_string

from mitsuba.render import Film

# from mitsuba.core import Transform4f, AnimatedTransform
# t = Transform4f.rotate([1, 0, 0], 30)
# a = AnimatedTransform(t)
# print(type(t), type(a))


# scale = (1, 1, 1)
# translate = (0, 0, 0)
# cy_obj = load_string("""<shape version="2.0.0" type="cylinder">
#       <transform name="to_world">
#           <scale x="{}" y="{}" z="{}"/>
#           <translate x="{}" y="{}" z="{}"/>
#       </transform>
#   </shape>""".format(scale[0], scale[1], scale[2],
#                      translate[0], translate[1], translate[2]))

# integrator = load_string("""<integrator version="2.0.0" type='direct'>
#         <integer name="emitter_samples" value="4"/>
#         <integer name="bsdf_samples" value="0"/>
#     </integrator>""")

# pt_light = load_string("""<emitter version="2.0.0" type="point">
#         <point name="position" x="2" y="-6.0" z="4.5"/>
#         <rgb name="intensity" value="10.0"/>
#     </emitter>""")

# empty_scene = load_string("""<scene version="2.0.0">
#   </scene>""")

# teapot = load_string("""<shape version="2.0.0" type="ply">
#         <string name="filename"
#                 value="resources/data/ply/teapot.ply"/>

#         <transform name="to_world">
#             <rotate x="0.0" y="1.0" z="1.0" angle="15"/>
#             <rotate x="0.0" y="0.0" z="1.0" angle="-15"/>
#         </transform>
#     </shape>""")

film = load_string("""<film version="2.0.0" type="hdrfilm">
            <rfilter type="box"/>
            <integer name="width" value="1024"/>
            <integer name="height" value="768"/>
        </film>""")
inst = PluginManager.instance()
plugin_list = inst.loaded_plugins()
# # assert(3 == len(plugin_list))
print(plugin_list)

from ipdb import set_trace
set_trace()