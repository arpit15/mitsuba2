#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++2a-extensions"

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(plugin)
NAMESPACE_BEGIN(detail)
using Float = float;

/// Throws if non-whitespace characters are found after the given index.
static void check_whitespace_only(const std::string &s, size_t offset) {
    for (size_t i = offset; i < s.size(); ++i) {
        if (!std::isspace(s[i]))
            Throw("Invalid trailing characters in floating point number \"%s\"", s);
    }
}

static Float stof(const std::string &s) {
    size_t offset = 0;
    Float result = std::stof(s, &offset);
    check_whitespace_only(s, offset);
    return result;
}

void parse_rgb(PluginManager &pgmr, Properties &prop, py::dict &dict, bool within_emitter = false, bool within_spectrum = false) {
  MTS_PY_IMPORT_TYPES()
  Properties::Color3f col;
  std::string rgb_name;
  for(auto item : dict) {
    std::string key = item.first.cast<std::string>();
    if(key.compare("name") == 0) {
      rgb_name = item.second.cast<std::string>();
    } else if(isinstance<Properties::Color3f>(item.second)) {
      py::print("color key:", key, item.second);
       col = item.second.cast<Properties::Color3f>();
    } 
  }

  if(!within_spectrum) {
    bool is_ior = rgb_name == "eta" || rgb_name == "k" || rgb_name == "int_ior" ||
                  rgb_name == "ext_ior";
    Properties nested_prop(within_emitter? "srgb_d65" : "srgb");
    nested_prop.set_color("color", col);

    if(!within_emitter && is_ior)
      nested_prop.set_bool("unbounded", true);

    ref<Object> obj = pgmr.create_object(nested_prop, 
        Class::for_name("Texture", mitsuba::detail::get_variant<Float, Spectrum >()));
    prop.set_object(rgb_name, obj);
  } else {
    prop.set_color("color", col);
  }
}

void parse_spectrum(PluginManager &pgmr, Properties &prop, py::dict &dict, bool within_emitter = false) {
  MTS_PY_IMPORT_TYPES()
  bool has_value = false, has_filename = false, is_constant = false;
  std::string spec_name;
  // for spectrum not constant
  std::vector<Properties::Float> wavelengths, values;
  const Class *tex_cls = Class::for_name("Texture", mitsuba::detail::get_variant<Float, Spectrum >());
  for(auto item : dict) {
    std::string key = item.first.cast<std::string>();
    py::print("key:", key, item.second);
    if(key.compare("name") == 0) {
      spec_name = item.second.cast<std::string>();
    } else if(key.compare("filename") == 0) {
      if(has_value)
        Throw("'spectrum' requires one of \"value\" or \"filename\" attributes");
      has_filename = true;
      std::string filename = item.second.cast<std::string>();
      std::vector<Float> wavelengths, values;
      spectrum_from_file(filename, wavelengths, values);
    } else if(key.compare("value") == 0) {
      py::print("parsing value:", item.second);
      has_value = true;
      if(isinstance<py::float_>(item.second) || isinstance<py::int_>(item.second)) {
        py::print("got constant value");
        is_constant = true;
        Properties nested_prop("uniform");
        ScalarFloat val = item.second.cast<float>();
        if(within_emitter && is_spectral_v<Spectrum>) {
          nested_prop.set_plugin_name("d65");
          nested_prop.set_float("scale", val);
        } else {
          nested_prop.set_float("value", val);
        }
        ref<Object> obj = pgmr.create_object(nested_prop, tex_cls);
        auto expanded = obj->expand();
        if(!expanded.empty())
          obj = expanded[0];
        prop.set_object(spec_name, obj);
        return;
      } else if(isinstance<py::str>(item.second)) {
        is_constant = false;
        if(has_filename)
          Throw("'spectrum' requires one of \"value\" or \"filename\" attributes");
      
        if (has_value) {
          py::print("Creating for pair values");
          std::vector<std::string> tokens = string::tokenize(item.second.cast<std::string>());

          for (const std::string &token : tokens) {
            std::vector<std::string> pair = string::tokenize(token, ":");
            if (pair.size() != 2)
                Throw("invalid spectrum (expected wavelength:value pairs)");

            Properties::Float wavelength, value;
            try {
                wavelength = mitsuba::plugin::detail::stof(pair[0]);
                value = mitsuba::plugin::detail::stof(pair[1]);
            } catch (...) {
                Throw("could not parse wavelength:value pair: \"%s\"", token);
            }

            wavelengths.push_back(wavelength);
            values.push_back(value);
          }
        }
      }
    }
  }

  // scale and create
  Properties::Float unit_conversion = 1.f;
  if(within_emitter || !is_spectral_v<Spectrum>)
    unit_conversion = MTS_CIE_Y_NORMALIZATION;

  /* Detect whether wavelengths are regularly sampled and potentially
     apply the conversion factor. */
  bool is_regular = true;
  Properties::Float interval = 0.f;
  for (size_t n = 0; n < wavelengths.size(); ++n) {
    values[n] *= unit_conversion;

    if (n <= 0)
        continue;

    Properties::Float distance = (wavelengths[n] - wavelengths[n - 1]);
    if (distance < 0.f)
        Throw("wavelengths must be specified in increasing order");
    if (n == 1)
        interval = distance;
    else if (std::abs(distance - interval) > math::Epsilon<Float>)
        is_regular = false;
  }

  Properties nested_prop;
  if(is_regular) {
    nested_prop.set_plugin_name("regular");
    nested_prop.set_long("size", wavelengths.size());
    nested_prop.set_float("lambda_min", wavelengths.front());
    nested_prop.set_float("lambda_max", wavelengths.back());
    nested_prop.set_pointer("values", values.data());
  } else {
    nested_prop.set_plugin_name("irregular");
    nested_prop.set_long("size", wavelengths.size());
    nested_prop.set_pointer("wavelengths", wavelengths.data());
    nested_prop.set_pointer("values", values.data());
  }

  ref<Object> obj = pgmr.create_object(nested_prop, tex_cls);

  // In non-spectral mode, pre-integrate against the CIE matching curves
  if (is_spectral_v<Spectrum>) {

    /// Spectral IOR values are unbounded and require special handling
    bool is_ior = spec_name == "eta" || spec_name == "k" || spec_name == "int_ior" ||
                  spec_name == "ext_ior";

    Properties::Color3f color = spectrum_to_rgb(wavelengths, values, !(within_emitter || is_ior));

    Properties props3;
    if (is_monochromatic_v<Spectrum>) {
        props3 = Properties("uniform");
        props3.set_float("value", luminance(color));
    } else {
        props3 = Properties(within_emitter ? "srgb_d65" : "srgb");
        props3.set_color("color", color);

        if (!within_emitter && is_ior)
            props3.set_bool("unbounded", true);
    }

    obj = PluginManager::instance()->create_object(
        props3, tex_cls);
  }

  prop.set_object(spec_name, obj);
}

void create_properties(PluginManager &pgmr, Properties &prop, py::dict &dict, bool within_emitter = false, bool within_spectrum = false) {
  MTS_PY_IMPORT_TYPES()
  for(auto item : dict) {
    std::string key = item.first.cast<std::string>();
    if(key.compare("type") == 0) {
      std::string plugin_name = item.second.cast<std::string>();
      // capitalize the first letter
      plugin_name[0] = std::toupper(plugin_name[0]);
      prop.set_plugin_name(plugin_name);
    } else if(key.compare("rgb") == 0) {
      // parse spectrum
      auto rgb_dict = item.second.cast<py::dict>();
      parse_rgb(pgmr, prop, rgb_dict, within_emitter, within_spectrum);

    } else if(key.compare("spectrum") == 0) {
      // parse spectrum
      auto spec_dict = item.second.cast<py::dict>();
      parse_spectrum(pgmr, prop, spec_dict, within_emitter);

    } else if(key.compare("to_world") == 0) {
      // py::print("transform key:", key, item.second);
      prop.set_transform(key, item.second.cast<Properties::Transform4f>());
    } else if(isinstance<py::bool_>(item.second)) {
      // py::print("key:", key, item.second);
      prop.set_bool(key, item.second.cast<bool>());
    } else if(isinstance<py::int_>(item.second)) {
      // py::print("int key:", key, item.second);
      prop.set_long(key, item.second.cast<int64_t>());
    } else if(isinstance<py::float_>(item.second)) {
      // py::print("float key:", key, item.second);
      prop.set_float(key, item.second.cast<Properties::Float>());
    } else if(isinstance<py::str>(item.second)) {
      // py::print("string key:", key, item.second);
      prop.set_string(key, item.second.cast<std::string>());
    } else if(isinstance<Properties::Point3f>(item.second)) {
      // py::print("point3f key:", key, item.second);
      prop.set_point3f(key, item.second.cast<Properties::Point3f>());
    } else if(isinstance<Properties::Vector3f>(item.second)) {
      // py::print("vector3f key:", key, item.second);
      prop.set_vector3f(key, item.second.cast<Properties::Vector3f>());
    } else {
      // another plugin
      bool nested_within_emitter = false, nested_within_spectrum = false;
       
      std::string parent_class_name = key;
      if(parent_class_name.compare("emitter") == 0)
          nested_within_emitter = true;
      if(parent_class_name.compare("rfilter") == 0)
          parent_class_name = "reconstructionFilter";
      // capitalize the first letter
      parent_class_name[0] = std::toupper(parent_class_name[0]);
      auto nested_dict = item.second.cast<py::dict>();
      
      Properties nested_prop;
      create_properties(pgmr, nested_prop, nested_dict, nested_within_emitter, nested_within_spectrum);
      const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum >());
      auto obj = pgmr.create_object(nested_prop, class_);
      prop.set_object(key, obj);
    }
  }
}

NAMESPACE_END(detail)
NAMESPACE_END(plugin)
NAMESPACE_END(mitsuba)

MTS_PY_EXPORT(PluginManager) {
  MTS_PY_IMPORT_TYPES()
  py::class_<PluginManager, std::unique_ptr<PluginManager, py::nodelete>>(m, "PluginManager")
    .def_static_method(PluginManager, instance, py::return_value_policy::reference)
    .def("create", [](PluginManager &pgmr, const py::dict dict) {
      Properties prop;
      std::string parent_class_name;
      bool within_emitter = false, within_spectrum = false;
      for(auto item : dict) {
        parent_class_name = item.first.cast<std::string>();
        if(parent_class_name.compare("emitter") == 0)
          within_emitter = true;
        if(parent_class_name.compare("rfilter") == 0)
          parent_class_name = "reconstructionFilter";
        // capitalize the first letter
        parent_class_name[0] = std::toupper(parent_class_name[0]);
        auto nested_dict = item.second.cast<py::dict>();
        mitsuba::plugin::detail::create_properties(pgmr, prop, nested_dict, within_emitter, within_spectrum);
      }
      const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum>());
      return pgmr.create_object(prop, class_);
    }, py::return_value_policy::reference);
}
