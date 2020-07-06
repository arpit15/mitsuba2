#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-spot:

Spot light source (:monosp:`spot`)
------------------------------------

.. pluginparameters::

 * - radiance
   - |spectrum|
   - Specifies the maximum radiant radiance at the center in units of power per unit steradian. (Default: 1).
     This cannot be spatially varying (e.g. have bitmap as type).
 * - cutoff_angle
   - |float|
   - Cutoff angle, beyond which the spot light is completely black (Default: 20 degrees)
 * - beam_width
   - |float|
   - Subtended angle of the central beam portion (Default: :math:`cutoff_angle \times 3/4`)
 * - texture
   - |texture|
   - An optional texture to be projected along the spot light. This must be spatially varying (e.g. have bitmap as type).
 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)

This plugin provides a spot light with a linear falloff. In its local coordinate system, the spot light is
positioned at the origin and points along the positive Z direction. It can be conveniently reoriented
using the lookat tag, e.g.:

.. code-block:: xml
    :name: spot-light

    <emitter type="spot">
        <transform name="to_world">
            <!-- Orient the light so that points from (1, 1, 1) towards (1, 2, 1) -->
            <lookat origin="1, 1, 1" target="1, 2, 1"/>
        </transform>
    </emitter>

The radiance linearly ramps up from cutoff_angle to beam_width (both specified in degrees),
after which it remains at the maximum value. A projection texture may optionally be supplied.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/emitter_spot_no_texture.jpg
   :caption: Two spot lights with different colors and no texture specified.
.. subfigure:: ../../resources/data/docs/images/render/emitter_spot_texture.jpg
   :caption: A spot light with a texture specified.
.. subfigend::
   :label: fig-spot-light

 */

template <typename Float, typename Spectrum>
class SpatialVaryingArea final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_medium, m_shape, m_world_transform)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    SpatialVaryingArea(const Properties &props) : Base(props) {
        m_flags = +EmitterFlags::Surface;
        m_radiance = props.texture<Texture>("radiance", Texture::D65(1.f));
        m_texture = props.texture<Texture>("texture", Texture::D65(1.f));

        if (m_radiance->is_spatially_varying())
            Throw("The parameter 'radiance' cannot be spatially varying (e.g. bitmap type)!");

        if (props.has_property("texture")) {
            if (!m_texture->is_spatially_varying())
                Throw("The parameter 'texture' must be spatially varying (e.g. bitmap type)!");
            m_flags |= +EmitterFlags::SpatiallyVarying;
        }

        m_quad_factor = props.float_("quad_factor", 0.001f);
        m_blur_size = props.float_("blur_size", 0.1f);
	
        m_cutoff_angle = props.float_("cutoff_angle", 20.0f);
        m_beam_width = props.float_("beam_width", m_cutoff_angle * 3.0f / 4.0f);
        m_cutoff_angle = deg_to_rad(m_cutoff_angle);
        m_beam_width = deg_to_rad(m_beam_width);
        m_inv_transition_width = 1.0f / (m_cutoff_angle - m_beam_width);
        m_cos_cutoff_angle = cos(m_cutoff_angle);
        m_cos_beam_width = cos(m_beam_width);
        Assert(m_cutoff_angle >= m_beam_width);
        m_uv_factor = tan(m_cutoff_angle);
    }

    void set_shape(Shape *shape) override {
        if (m_shape)
            Throw("An area emitter can be only be attached to a single shape.");

        Base::set_shape(shape);
        m_area_times_pi = m_shape->surface_area() * math::Pi<ScalarFloat>;
    }

    Float smooth_profile(Float x) const {
        Float res(0);
        res = select(x >= m_blur_size && x <= Float(1) - m_blur_size, Float(1), res);
        res = select(x < m_blur_size && x > Float(0), x / m_blur_size, res);
        res = select(x > Float(1) - m_blur_size && x < Float(1),
                     (1 - x) / m_blur_size, res);
        return res;
    }
  
    // inline Float falloff_curve(const Vector3f &d, Wavelength wavelengths, Mask active) const {
    //     SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
    //     si.wavelengths = wavelengths;
    //     Float result(1);

    //     Vector3f local_dir = normalize(d);
    //     const Float cos_theta = local_dir.z();

    //     // if (m_texture->is_spatially_varying()) {
    //     //     si.uv = Point2f(0.5f + 0.5f * local_dir.x() / (local_dir.z() * m_uv_factor),
    //     //                     0.5f + 0.5f * local_dir.y() / (local_dir.z() * m_uv_factor));
    //     //     result *= m_texture->eval(si, active);
    //     // }

    //     auto beam_res = select(cos_theta >= m_cos_beam_width, result,
    //                                 result * ((m_cutoff_angle - acos(cos_theta)) * m_inv_transition_width));

    //     return select(cos_theta <= m_cos_cutoff_angle, Float(0.0f), beam_res);
    // }

    inline Float falloff_curve(const Float cos_theta) const {
        Float result(1);
        auto beam_res = select(cos_theta > m_cos_beam_width, result,
                            result * ((m_cutoff_angle - acos(cos_theta)) * m_inv_transition_width));

        return select(cos_theta < m_cos_cutoff_angle, Float(0.0f), beam_res);

     //    Float angle = acos(cos_theta) * math::InvPi<ScalarFloat> - 0.5f;
     //    //Float value = m_quad_factor*(1.f-pow(angle, 2.f));
    	// Float value = math::InvSqrtPi<ScalarFloat> * (1.f/m_quad_factor)*exp(- pow(angle/m_quad_factor, 2.f));
    	// return value;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &spatial_sample,
                                          const Point2f &dir_sample,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        PositionSample3f ps = m_shape->sample_position(time, spatial_sample, active);

        // 2. Sample directional component
        Vector3f local_dir = warp::square_to_uniform_cone(dir_sample, (Float)m_cos_cutoff_angle);
        Float pdf_dir = warp::square_to_uniform_cone_pdf(local_dir, (Float)m_cos_cutoff_angle);

        // 3. Sample spectrum
        SurfaceInteraction3f si(ps, zero<Wavelength>(0.f));
        auto [wavelengths, spec_weight] = m_radiance->sample(
            si, math::sample_shifted<Wavelength>(wavelength_sample), active);

        spec_weight *= falloff_curve(local_dir.z());
	spec_weight *= smooth_profile(ps.uv.x()) * smooth_profile(ps.uv.y());

	
        return std::make_pair(
            Ray3f(ps.p, Frame3f(ps.n).to_world(local_dir), time, wavelengths),
            unpolarized<Spectrum>(spec_weight) * m_area_times_pi / pdf_dir
        );
    }

    std::pair<DirectionSample3f, Spectrum> sample_direction(const Interaction3f &it,
                                                            const Point2f &sample,
                                                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        Assert(m_shape, "Can't sample from an area emitter without an associated Shape.");

        DirectionSample3f ds = m_shape->sample_direction(it, sample, active);
        active &= dot(ds.d, ds.n) < 0.f && neq(ds.pdf, 0.f);

        SurfaceInteraction3f si(ds, it.wavelengths);
        Spectrum spec = m_radiance->eval(si, active) / ds.pdf;

        // auto trafo = m_world_transform->eval(it.time, active);
        // Vector3f local_d = trafo.inverse() * -ds.d;
        // spec *= falloff_curve(local_d, it.wavelengths, active);

        auto cos_theta = -dot(ds.d, ds.n);
        spec *= falloff_curve(cos_theta);
	spec *= smooth_profile(ds.uv.x()) * smooth_profile(ds.uv.y());
	
        ds.object = this;
        return { ds, unpolarized<Spectrum>(spec) & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return select(dot(ds.d, ds.n) < 0.f,
                      m_shape->pdf_direction(it, ds, active), 0.f);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override { 
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        // auto trafo = m_world_transform->eval(si.time, active);
        // Vector3f local_d = trafo.inverse() * si.wi;

        auto cos_theta = dot(si.wi, si.n);

        return select(
            Frame3f::cos_theta(si.wi) > 0.f,
            unpolarized<Spectrum>(m_radiance->eval(si, active))
                * falloff_curve(cos_theta),
                // * falloff_curve(local_d, si.wavelengths, active),
            0.f
        );
    }

    ScalarBoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("radiance", m_radiance.get());
        callback->put_object("texture", m_texture.get());
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (string::contains(keys, "parent"))
            m_area_times_pi = m_shape->surface_area() * math::Pi<ScalarFloat>;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SpatialVaryingArea[" << std::endl
            << "  world_transform = " << string::indent(m_world_transform) << "," << std::endl
            << "  quad_factor = " << m_quad_factor << "," << std::endl
            << "  blur_size = " << m_blur_size << "," << std::endl
            << "  radiance = " << m_radiance << "," << std::endl
            << "  cutoff_angle = " << m_cutoff_angle << "," << std::endl
            << "  beam_width = " << m_beam_width << "," << std::endl
            << "  texture = " << (m_texture ? string::indent(m_texture) : "")
            << "  medium = " << (m_medium ? string::indent(m_medium) : "")
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "  <no shape attached!>";
        oss <<  std::endl << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFloat m_area_times_pi = 0.f;
    ref<Texture> m_radiance;
    ref<Texture> m_texture;
    ScalarFloat m_beam_width, m_cutoff_angle, m_uv_factor;
    ScalarFloat m_cos_beam_width, m_cos_cutoff_angle, m_inv_transition_width;
  ScalarFloat m_quad_factor, m_blur_size;
};


MTS_IMPLEMENT_CLASS_VARIANT(SpatialVaryingArea, Emitter)
MTS_EXPORT_PLUGIN(SpatialVaryingArea, "Spatial Varying Area emitter")
NAMESPACE_END(mitsuba)
