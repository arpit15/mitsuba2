#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

void parse_spectrum(PluginManager &pgmr, Properties &prop, py::dict &dict) {
  MTS_PY_IMPORT_TYPES()
  bool has_value = false, has_filename = false;
  std::string spec_name;

  for(auto item : dict) {
    std::string key = item.first.cast<std::string>();
    if(key.compare("name") == 0) {
      spec_name = item.second.cast<std::string>();
    } else if(key.compare("filename") == 0) {
      has_filename = true;
      std::string filename = item.second.cast<std::string>();
      std::vector<Float> wavelengths, values;
      spectrum_from_file(filename, wavelengths, values);
    } else if(key.compare("value") == 0) {
      has_value = true;
      if(isinstance<py::int_>(item.second)) {

      } else if(isinstance<py::str>(item.second)) {
        
      }
      else {
        auto obj = item.second.cast<ref<Object>>();
        prop.set_object(spec_name, obj);
        return;
      }
    }
  }
  // prop.set_object(spec_name, obj);
}

void create_properties(PluginManager &pgmr, Properties &prop, py::dict &dict) {
  MTS_PY_IMPORT_TYPES()
  bool within_emitter = false;
  for(auto item : dict) {
    std::string key = item.first.cast<std::string>();
    if(key.compare("type") == 0) {
      std::string plugin_name = item.second.cast<std::string>();
      if(key.compare("emitter") == 0)
        within_emitter = true;
      // capitalize the first letter
      plugin_name[0] = std::toupper(plugin_name[0]);
      prop.set_plugin_name(plugin_name);
    } else if(key.compare("spectrum") == 0) {
      // parse spectrum
      auto spec_dict = item.second.cast<py::dict>();
      parse_spectrum(pgmr, prop, spec_dict);

    } else if(key.compare("to_world") == 0) {
      prop.set_transform(key, item.second.cast<Properties::Transform4f>());
    } else if(isinstance<py::int_>(item.second)) {
      prop.set_long(key, item.second.cast<int64_t>());
    } else if(isinstance<Properties::Float>(item.second)) {
      prop.set_float(key, item.second.cast<Properties::Float>());
    } else if(isinstance<bool>(item.second)) {
      prop.set_bool(key, item.second.cast<bool>());
    } else if(isinstance<py::str>(item.second)) {
      prop.set_string(key, item.second.cast<std::string>());
    } else {
      // another plugin
      std::string parent_class_name = key;
      // capitalize the first letter
      parent_class_name[0] = std::toupper(parent_class_name[0]);
      auto nested_dict = item.second.cast<py::dict>();
      
      Properties nested_prop;
      create_properties(pgmr, nested_prop, nested_dict);
      const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum >());
      auto obj = pgmr.create_object(nested_prop, class_);
      prop.set_object(key, obj);
    }
  }
}

MTS_PY_EXPORT(PluginManager) {
  MTS_PY_IMPORT_TYPES()
  py::class_<PluginManager, std::unique_ptr<PluginManager, py::nodelete>>(m, "PluginManager")
    .def_static_method(PluginManager, instance, py::return_value_policy::reference)
    .def("create", [](PluginManager &pgmr, const py::dict dict) {
      Properties prop;
      std::string parent_class_name;
      for(auto item : dict) {
        parent_class_name = item.first.cast<std::string>();
        // capitalize the first letter
        parent_class_name[0] = std::toupper(parent_class_name[0]);
        auto nested_dict = item.second.cast<py::dict>();
        create_properties(pgmr, prop, nested_dict);
      }
      const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum>());
      return pgmr.create_object(prop, class_);
    }, py::return_value_policy::reference);
}
