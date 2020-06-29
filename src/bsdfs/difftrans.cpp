#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-difftrans:

Smooth diffuse material (:monosp:`diffuse`)
-------------------------------------------

.. pluginparameters::

 * - reflectance
   - |spectrum| or |texture|
   - Specifies the diffuse albedo of the material (Default: 0.5)

The diffuse translucent material
represents an ideally diffuse material with a user-specified amount of
reflectance. Any received illumination is scattered so that the surface
looks the same independently of the direction of observation.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_diffuse_plain.jpg
   :caption: Homogeneous reflectance
.. subfigure:: ../../resources/data/docs/images/render/bsdf_diffuse_textured.jpg
   :caption: Textured reflectance
.. subfigend::
   :label: fig-diffuse

Apart from a homogeneous reflectance value, the plugin can also accept
a nested or referenced texture map to be used as the source of reflectance
information, which is then mapped onto the shape based on its UV
parameterization. When no parameters are specified, the model uses the default
of 50% reflectance.

Note that this material is one-sided---that is, observed from the
back side, it will be completely black. If this is undesirable,
consider using the :ref:`twosided <bsdf-twosided>` BRDF adapter plugin.
The following XML snippet describes a diffuse material,
whose reflectance is specified as an sRGB color:

.. code-block:: xml
    :name: diffuse-srgb

    <bsdf type="diffuse">
        <rgb name="reflectance" value="0.2, 0.25, 0.7"/>
    </bsdf>

Alternatively, the reflectance can be textured:

.. code-block:: xml
    :name: diffuse-texture

    <bsdf type="diffuse">
        <texture type="bitmap" name="reflectance">
            <string name="filename" value="wood.jpg"/>
        </texture>
    </bsdf>

*/
template <typename Float, typename Spectrum>
class DiffuseTransmitter final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    DiffuseTransmitter(const Properties &props) : Base(props) {
        m_transmittance = props.texture<Texture>("transmittance", .5f);
        m_flags = BSDFFlags::DiffuseTransmission | BSDFFlags::FrontSide |
            BSDFFlags::BackSide;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* sample1 */,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = zero<BSDFSample3f>();

        if (unlikely(none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::DiffuseTransmission)))
            return { bs, 0.f };

        

        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::DiffuseTransmission;
        bs.sampled_component = 0;
        bs.sampled_roughness = math::Infinity<Float>;

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.wo.z() = select(cos_theta_i<=0.f, bs.wo.z(), -bs.wo.z());

        UnpolarizedSpectrum value = m_transmittance->eval(si, active);

        return { bs, select(active && bs.pdf > 0.f, unpolarized<Spectrum>(value), 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseTransmission))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= (cos_theta_i*cos_theta_o) < 0.f;

        UnpolarizedSpectrum value =
            m_transmittance->eval(si, active) * math::InvPi<Float> * abs(cos_theta_o);

        return select(active, unpolarized<Spectrum>(value), 0.f);
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (!ctx.is_enabled(BSDFFlags::DiffuseTransmission))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Vector3f wo_ = wo;
        wo_.z() = select(cos_theta_i<=0.f, wo.z(), -wo.z());

        Float pdf = abs(warp::square_to_cosine_hemisphere_pdf(wo_));

        return select(cos_theta_i*cos_theta_o < 0.f, pdf, 0.f);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("transmittance", m_transmittance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DiffuseTransmitter[" << std::endl
            << "  transmittance = " << string::indent(m_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_transmittance;
};

MTS_IMPLEMENT_CLASS_VARIANT(DiffuseTransmitter, BSDF)
MTS_EXPORT_PLUGIN(DiffuseTransmitter, "Diffuse translucent material")
NAMESPACE_END(mitsuba)
