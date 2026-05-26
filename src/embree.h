#pragma once
#include <embree4/rtcore.h>
#include "Model.h"
#include <glm/glm.hpp>
#include <map>

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////
// This struct describes an intersection, as extracted from the Embree
// ray.
///////////////////////////////////////////////////////////////////////////
struct Intersection
{
	// Point where the ray intersected with geometry
	glm::vec3 position;

	// Normal of the intersected triangle (face normal)
	glm::vec3 geometry_normal;

	// Interpolated normal between the three vertex normals of the triangle
	glm::vec3 shading_normal;

	// "outgoing" vector. Pointing from the intersected point to the origin of the ray.
	glm::vec3 wo;

	// Interpolated UV coordinates between the 3 vertices of the triangle
	glm::vec2 uv;

	// Material information of the hit triangle
	const labhelper::Material* material;
};

///////////////////////////////////////////////////////////////////////////
// Our own Ray struct. We keep ray and hit data together for convenience,
// mirroring the old Embree 2 layout. Internally we convert to/from
// RTCRayHit (for intersect) and RTCRay (for occluded) in embree.cpp.
///////////////////////////////////////////////////////////////////////////
struct Ray
{
	Ray(const glm::vec3& origin = glm::vec3(0.0f),
	    const glm::vec3& direction = glm::vec3(0.0f),
	    float near = 0.0f,
	    float far = FLT_MAX)
	    : o(origin), d(direction), tnear(near), tfar(far)
	{
		geomID = RTC_INVALID_GEOMETRY_ID;
		primID = RTC_INVALID_GEOMETRY_ID;
		instID = RTC_INVALID_GEOMETRY_ID;
	}

	////////////////////////////
	// Ray data

	// `o`: origin position of the ray
	glm::vec3 o;

	// `d`: direction of the ray
	glm::vec3 d;

	// `tnear`, `tfar`: starting and ending distance for intersection search
	float tnear = 0.0f;
	float tfar  = FLT_MAX;

	float time  = 0.0f;
	uint32_t mask = 0xFFFFFFFF;

	////////////////////////////
	// Hit data (populated by intersect(), do not set manually)

	// Geometry normal (Ng) of the hit surface — un-normalized
	glm::vec3 n;

	// Barycentric coordinates of the hit point within the triangle
	float u = 0.0f, v = 0.0f;

	uint32_t geomID = RTC_INVALID_GEOMETRY_ID;
	uint32_t primID = RTC_INVALID_GEOMETRY_ID;
	uint32_t instID = RTC_INVALID_GEOMETRY_ID;
};

///////////////////////////////////////////////////////////////////////////
// Scene functions
///////////////////////////////////////////////////////////////////////////

// Add a model to the embree scene
void addModel(const labhelper::Model* model, const glm::mat4& model_matrix);

// Build an acceleration structure for the scene
void buildBVH();

// Reinitialize the scene
void reinitScene();

///////////////////////////////////////////////////////////////////////////
// Ray intersection functions
///////////////////////////////////////////////////////////////////////////

// Test a ray against the scene and find the closest intersection.
// Populates the hit fields of `r` on success.
bool intersect(Ray& r);

// Returns intersection details for a ray that has already been passed
// through intersect() successfully.
Intersection getIntersection(const Ray& r);

// Test whether a ray is occluded anywhere by the scene.
// Does NOT find the closest hit — just returns true/false.
bool occluded(Ray& r);

} // namespace pathtracer
