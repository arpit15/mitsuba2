#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++2a-extensions"

using Caster = py::object(*)(const mitsuba::Object *);
extern Caster cast_object;

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(plugin)
NAMESPACE_BEGIN(detail)

// Helper macro
#define SET_PROP(Type, Setter)                                        \
	if (strcmp(typeid(T).name(), typeid(Type).name()) == 0)           \
		props.Setter(name, value);
		return;
#undef
template <typename T>
void set(Properties &props, const std::string &name, const T &value) {
    SET_PROP(bool, set_bool)
    SET_PROP(int, set_int)
    SET_PROP(long, set_long)
    SET_PROP(float, set_float)
    SET_PROP(Vector3f, set_vector3f)
    SET_PROP(Point3f, set_point3f)
    SET_PROP(Transform4f, set_transform)

    // TODO: not sure about those two
    SET_PROP(ref<AnimatedTransform>, set_animated_transform)
    SET_PROP(ref<Object>, set_object)

    SET_PROP(std::string, set_string)
}


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
	  has_value = true;
	  if(isinstance<py::float_>(item.second) || isinstance<py::int_>(item.second)) {
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
	// get type
	auto it = dict.begin();
	Assert( strcmp(it.first.cast<py::str>(), "type"), "First dict key should be str 'type'");
	std::string parent_class_name = it.second.cast<py::str>();
	// iterate over next props
	it++;
	// check for 
	for(auto item : it) {
		std::string key = item.first.cast<std::string>();
		// check if the val is dict
		try {
			auto nested_dict = item.second.cast<std::dict>();
			bool nested_within_emitter = false, nested_within_spectrum = false;
			std::string parent_class_name = key;
			if(parent_class_name.compare("emitter") == 0)
			  	nested_within_emitter = true;
			if(parent_class_name.compare("rfilter") == 0)
				parent_class_name = "reconstructionFilter";
			// capitalize the first letter
			parent_class_name[0] = std::toupper(parent_class_name[0]);
			Properties nested_prop;
			create_properties(pgmr, nested_prop, nested_dict, nested_within_emitter, nested_within_spectrum);
			const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum >());
			auto obj = pgmr.create_object(nested_prop, class_);
			prop.set_object(key, obj);
		} catch (...) {
			throw std::runtime_error("only dict")
		} 

		if(key.compare("rgb") == 0) {
			// parse spectrum
			auto rgb_dict = item.second.cast<py::dict>();
			parse_rgb(pgmr, prop, rgb_dict, within_emitter, within_spectrum);

		} else if(key.compare("spectrum") == 0) {
			// parse spectrum
			auto spec_dict = item.second.cast<py::dict>();
			parse_spectrum(pgmr, prop, spec_dict, within_emitter);

		} else {
			auto val = item.second.ptr();
			set(key, val);
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
		Assert(dict.size() == 1);
		auto it = dict.begin();
		std::string parent_class_name = it.first;
		bool within_emitter = false, within_spectrum = false;
		
		if(parent_class_name.compare("emitter") == 0)
		  within_emitter = true;
		if(parent_class_name.compare("rfilter") == 0)
		  parent_class_name = "reconstructionFilter";
		// capitalize the first letter
		parent_class_name[0] = std::toupper(parent_class_name[0]);
		auto nested_dict = it.second.cast<py::dict>();
		
		mitsuba::plugin::detail::create_properties(pgmr, prop, nested_dict, within_emitter, within_spectrum);
	  	
	  	const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum>());
	  	return cast_object((ref<Object>)(pgmr.create_object(prop, class_)));
	}, py::return_value_policy::reference);
}
