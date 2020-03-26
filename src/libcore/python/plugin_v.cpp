#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(PluginManager) {
  MTS_PY_IMPORT_TYPES()
  py::class_<PluginManager, std::unique_ptr<PluginManager, py::nodelete>>(m, "PluginManager")
    .def_static_method(PluginManager, instance, py::return_value_policy::reference)
    // .def_method(PluginManager, ensure_plugin_loaded, "name"_a)
    // .def_method(PluginManager, loaded_plugins)
    // .def_method(PluginManager, register_python_plugin, "plugin_name"_a)
    .def("create_object", (ref<Object> (PluginManager::*)(const Properties &props, const Class *class_)) &PluginManager::create_object, "props"_a, "class_"_a, py::return_value_policy::reference);
}
