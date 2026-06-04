#include "material.h"
#include "labhelper.h"
#include "sampling.h"

using namespace labhelper;

namespace pathtracer {
WiSample sampleHemisphereCosine(const vec3 &wo, const vec3 &n) {
  mat3 tbn = tangentSpace(n);
  vec3 sample = cosineSampleHemisphere();
  WiSample r;
  r.wi = tbn * sample;
  if (dot(r.wi, n) > 0.0f)
    r.pdf = max(0.0f, dot(r.wi, n)) / M_PI;
  return r;
}

///////////////////////////////////////////////////////////////////////////
// A Lambertian (diffuse) material
///////////////////////////////////////////////////////////////////////////
vec3 Diffuse::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  if (dot(wi, n) <= 0.0f)
    return vec3(0.0f);
  if (!sameHemisphere(wi, wo, n))
    return vec3(0.0f);
  return (1.0f / M_PI) * color;
}

WiSample Diffuse::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r = sampleHemisphereCosine(wo, n);
  r.f = f(r.wi, wo, n);
  return r;
}

/*vec3 MicrofacetBRDF::f(const vec3& wi, const vec3& wo, const vec3& n) const
{
        return vec3(0.0f);
}*/

// -----------------------------------------------------------------------------
// FEATURE: Blinn-Phong Microfacet BRDF
// Evaluate glossy specular reflection using a physically-based microfacet
// model.

vec3 MicrofacetBRDF::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  if (dot(wi, n) <= 0.0f || dot(wo, n) <= 0.0f)
    return vec3(0.0f);

  vec3 wh = normalize(wi + wo);

  float ndotwh = max(dot(n, wh), 0.0f);
  float ndotwi = max(dot(n, wi), 0.0f);
  float ndotwo = max(dot(n, wo), 0.0f);
  float wodotwh = max(dot(wo, wh), EPSILON);

  // Blinn-Phong normal distribution function
  float D = ((shininess + 2.0f) / (2.0f * M_PI)) * pow(ndotwh, shininess);

  // Geometry attenuation term
  float G = min(1.0f, min((2.0f * ndotwh * ndotwo) / wodotwh,
                          (2.0f * ndotwh * ndotwi) / wodotwh));

  float denominator = 4.0f * ndotwo * ndotwi;

  if (denominator < EPSILON)
    return vec3(0.0f);

  float brdf = (D * G) / denominator;

  return vec3(brdf);
}
// -----------------------------------------------------------------------------

/*WiSample MicrofacetBRDF::sample_wi(const vec3& wo, const vec3& n) const
{
        WiSample r = sampleHemisphereCosine(wo, n);
        r.f = f(r.wi, wo, n);

        return r;
}*/

// -----------------------------------------------------------------------------
// FEATURE: Microfacet Importance Sampling
// Sample glossy reflection directions using a Blinn-Phong microfacet
// distribution to reduce Monte Carlo variance.

WiSample MicrofacetBRDF::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r;
  // Sample Blinn-Phong half vector
  float phi = 2.0f * M_PI * randf();
  float cosTheta = pow(randf(), 1.0f / (shininess + 1.0f));
  float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));

  vec3 wh_local = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

  // Transform to world space
  mat3 tbn = tangentSpace(n);
  vec3 wh = normalize(tbn * wh_local);
  // Reflect outgoing direction around half vector
  r.wi = reflect(-wo, wh);
  // Reject invalid hemisphere samples
  if (dot(r.wi, n) <= 0.0f) {
    r.pdf = 0.0f;
    r.f = vec3(0.0f);
    return r;
  }
  float ndotwh = max(dot(n, wh), 0.0f);
  float wodotwh = max(dot(wo, wh), EPSILON);

  // Blinn-Phong PDF
  r.pdf = ((shininess + 1.0f) * pow(ndotwh, shininess)) /
          (2.0f * M_PI * 4.0f * wodotwh);

  r.f = f(r.wi, wo, n);

  return r;
}

// -----------------------------------------------------------------------------

/*float BSDF::fresnel(const vec3& wi, const vec3& wo) const
{
        return 0.0f;
}*/

// -----------------------------------------------------------------------------
// FEATURE: Schlick Fresnel approximation
// Compute angle-dependent reflectance for physically-based glossy materials.

float BSDF::fresnel(const vec3 &wi, const vec3 &wo) const {
  vec3 wh = normalize(wi + wo);
  float cos_theta = max(0.0f, dot(wi, wh));
  return R0 + (1.0f - R0) * pow(1.0f - cos_theta, 5.0f);
}

// -----------------------------------------------------------------------------

/*vec3 DielectricBSDF::f(const vec3& wi, const vec3& wo, const vec3& n) const
{
        return vec3(0);
}*/

// -----------------------------------------------------------------------------
// FEATURE: Dielectric BSDF Material Blending
// Combine diffuse and reflective components using Fresnel-based weighting.

vec3 DielectricBSDF::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  float F = fresnel(wi, wo);

  vec3 reflection = F * reflective_material->f(wi, wo, n);

  vec3 transmission = (1.0f - F) * transmissive_material->f(wi, wo, n);

  return reflection + transmission;
}

// -----------------------------------------------------------------------------

WiSample DielectricBSDF::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r;
  if (randf() < 0.5f) {
    r = reflective_material->sample_wi(wo, n);
    r.pdf *= 0.5f;
    float F = fresnel(r.wi, wo);
    r.f *= F;
  } else {
    r = transmissive_material->sample_wi(wo, n);
    r.pdf *= 0.5f;
    float F = fresnel(r.wi, wo);
    r.f *= (1.0f - F);
  }
  return r;
}

/*vec3 MetalBSDF::f(const vec3& wi, const vec3& wo, const vec3& n) const
{
        return vec3(0);
}*/

// -----------------------------------------------------------------------------
// FEATURE: Metallic Reflection Model
// Simulate colored specular reflections for metallic materials.

vec3 MetalBSDF::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  float F = fresnel(wi, wo);
  return color * F * reflective_material->f(wi, wo, n);
}
// -----------------------------------------------------------------------------

WiSample MetalBSDF::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r = reflective_material->sample_wi(wo, n);
  float F = fresnel(r.wi, wo);
  r.f *= F * color;
  return r;
}

/*vec3 BSDFLinearBlend::f(const vec3& wi, const vec3& wo, const vec3& n) const
{
        return vec3(0.0);
}*/

// -----------------------------------------------------------------------------
// FEATURE: Layered Material Blending
// Blend multiple BSDF material responses into a combined surface model.

vec3 BSDFLinearBlend::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  return w * bsdf0->f(wi, wo, n) + (1.0f - w) * bsdf1->f(wi, wo, n);
}
// -----------------------------------------------------------------------------

/*WiSample BSDFLinearBlend::sample_wi(const vec3& wo, const vec3& n) const
{
        return WiSample{};
}*/

/*WiSample BSDFLinearBlend::sample_wi(const vec3& wo, const vec3& n) const
{
        if (randf() < w)
        {
                return bsdf0->sample_wi(wo, n);
        }
        else
        {
                return bsdf1->sample_wi(wo, n);
        }
}*/

// FEATURE: BSDF Importance Sampling Blend
// Randomly sample blended material components using weighted probabilistic
// selection.

WiSample BSDFLinearBlend::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r;
  if (randf() < w) {
    r = bsdf0->sample_wi(wo, n);
  } else {
    r = bsdf1->sample_wi(wo, n);
  }
  // Both sub-BSDFs use cosine hemisphere sampling, so the mixture PDF equals
  // the unscaled component PDF (w*p + (1-w)*p = p). Do not scale by w.

  r.f = f(r.wi, wo, n);
  return r;
}

///////////////////////////////////////////////////////////////////////////
// A perfect specular refraction.
///////////////////////////////////////////////////////////////////////////
vec3 GlassBTDF::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  if (sameHemisphere(wi, wo, n)) {
    return vec3(0);
  } else {
    return vec3(1);
  }
}

WiSample GlassBTDF::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r;

  // n is already flipped toward wo by getIntersection, so dot(wo, n) > 0 always.
  // Use the entering flag (set before the flip) to select the correct IOR ratio.
  const float eta = entering ? (1.0f / ior) : ior;
  const vec3 N = n;

  float w = dot(wo, N) * eta;
  float k = 1.0f + (w - eta) * (w + eta);
  if (k < 0.0f) {
    // Total internal reflection
    r.wi = reflect(-wo, N);
  } else {
    k = sqrt(k);
    r.wi = normalize(-eta * wo + (w - k) * N);
  }
  r.pdf = abs(dot(r.wi, n));
  r.f = vec3(1.0f);

  return r;
}

vec3 BTDFLinearBlend::f(const vec3 &wi, const vec3 &wo, const vec3 &n) const {
  return w * btdf0->f(wi, wo, n) + (1.0f - w) * btdf1->f(wi, wo, n);
}

// Glass and diffuse have different sampling distributions, so the mixture PDF
// must be scaled by w or (1-w), and f overridden with the full blend value.
WiSample BTDFLinearBlend::sample_wi(const vec3 &wo, const vec3 &n) const {
  WiSample r;
  if (randf() < w) {
    r = btdf0->sample_wi(wo, n);
    r.pdf *= w;
  } else {
    r = btdf1->sample_wi(wo, n);
    r.pdf *= (1.0f - w);
  }
  r.f = f(r.wi, wo, n);
  return r;
}
} // namespace pathtracer
