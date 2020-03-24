#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(PluginManager) {
  MTS_PY_IMPORT_TYPES()
  py::class_<PluginManager, std::unique_ptr<PluginManager, py::nodelete>>(m, "PluginManager")
    .def_static_method(PluginManager, instance, py::return_value_policy::reference)
    .def_method(PluginManager, ensure_plugin_loaded, "name"_a)
    .def_method(PluginManager, loaded_plugins)
    .def_method(PluginManager, register_python_plugin, "plugin_name"_a)
    .def("create_object", (ref<Object> (PluginManager::*)(const Properties &props, const Class *class_)) &PluginManager::create_object, "props"_a, "class_"_a, py::return_value_policy::reference)
    .def("create_object",
      [](PluginManager& pgmr, const Properties &props, const std::string node_name) {
        
        // figure out the class
        // const Class *plugin_class;

        // auto it = std::find(pgmr.d->m_python_plugins.begin(), pgmr.d->m_python_plugins.end(), props.plugin_name());
        // if (it != pgmr.d->m_python_plugins.end()) {
        //     plugin_class = Class::for_name(props.plugin_name(), mitsuba::detail::get_variant<Float, Spectrum>());
        // } else {
        //     const Plugin *plugin = pgmr.d->plugin(props.plugin_name());
        //     plugin_class = Class::for_name(plugin->plugin_name, mitsuba::detail::get_variant<Float, Spectrum>());
        // }

        // Assert(plugin_class != nullptr);
        // ref<Object> object = plugin_class->construct(props);
        // return object;

        // base class are supposed to be already loaded
        const Class *class_ = Class::for_name(node_name, mitsuba::detail::get_variant<Float, Spectrum>());
        Assert(class_ != nullptr);
        return pgmr.create_object(props, class_);
      },
    "props"_a, "node_name"_a, py::return_value_policy::reference);

}