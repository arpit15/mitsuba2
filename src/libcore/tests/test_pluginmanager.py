import pytest
import mitsuba

def test01_pluginmanager_instance(variant_scalar_rgb):
  from mitsuba.core import PluginManager
  inst = PluginManager.instance()

def test02_plugin_loaded(variant_scalar_rgb):
  from mitsuba.core import PluginManager
  from mitsuba.core.xml import load_string
  scale = (1, 1, 1)
  translate = (0, 0, 0)
  cy_obj = load_string("""<shape version="2.0.0" type="cylinder">
        <transform name="to_world">
            <scale x="{}" y="{}" z="{}"/>
            <translate x="{}" y="{}" z="{}"/>
        </transform>
    </shape>""".format(scale[0], scale[1], scale[2],
                       translate[0], translate[1], translate[2]))
  inst = PluginManager.instance()
  plugin_list = inst.loaded_plugins()
  assert(3 == len(plugin_list))

def test03_create_object(variant_scalar_rgb):
  from mitsuba.core import PluginManager, Properties
  from mitsuba.render import Scene

  pgmr = PluginManager.instance()
  props = Properties("scene")
  obj = Scene(Properties())
  obj1 = pgmr.create_object(props, obj.class_())
  assert type(obj1) is Scene