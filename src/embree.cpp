#include "embree.h"
#include <iostream>
#include <map>

using namespace std;
using namespace glm;

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////
// Global variables
///////////////////////////////////////////////////////////////////////////
RTCDevice embree_device = nullptr;
RTCScene  embree_scene  = nullptr;

///////////////////////////////////////////////////////////////////////////
// Build an acceleration structure for the scene
///////////////////////////////////////////////////////////////////////////
void buildBVH()
{
	cout << "Embree building BVH..." << flush;
	rtcCommitScene(embree_scene);   // was: rtcCommit()
	cout << "done.\n";
}

///////////////////////////////////////////////////////////////////////////
// Called when there is an embree error
///////////////////////////////////////////////////////////////////////////
void embreeErrorHandler(void* userval, const RTCError code, const char* str)
{
	cout << "Embree ERROR: " << str << endl;
	exit(1);
}

///////////////////////////////////////////////////////////////////////////
// Used to map an Embree geometry ID to our scene Meshes and Materials
///////////////////////////////////////////////////////////////////////////
map<uint32_t, const labhelper::Model*> map_geom_ID_to_model;
map<uint32_t, const labhelper::Mesh*>  map_geom_ID_to_mesh;

void initEmbree()
{
	///////////////////////////////////////////////////////////////////////
	// Lazy initialize embree on first use
	///////////////////////////////////////////////////////////////////////
	static bool embree_is_initialized = false;
	if(!embree_is_initialized)
	{
		cout << "Initializing embree..." << flush;
		embree_is_initialized = true;
		embree_device = rtcNewDevice(nullptr);
		// was: rtcDeviceSetErrorFunction2()
		rtcSetDeviceErrorFunction(embree_device, embreeErrorHandler, nullptr);
		cout << "done.\n";
	}
}

void reinitScene()
{
	initEmbree();

	if(embree_scene)
	{
		rtcReleaseScene(embree_scene);  // was: rtcDeleteScene()
	}

	// In Embree 4, rtcNewScene() takes only the device.
	// The old RTC_SCENE_STATIC and RTC_INTERSECT1 flags no longer exist.
	embree_scene = rtcNewScene(embree_device);
}

///////////////////////////////////////////////////////////////////////////
// Add a model to the embree scene
///////////////////////////////////////////////////////////////////////////
void addModel(const labhelper::Model* model, const mat4& model_matrix)
{
	///////////////////////////////////////////////////////////////////////
	// Lazy initialize embree on first use
	///////////////////////////////////////////////////////////////////////
	if(!embree_scene)
	{
		reinitScene();
	}

	///////////////////////////////////////////////////////////////////////
	// Transform and add each mesh in the model as a geometry in embree,
	// and create mappings so that we can connect an embree geom_ID to a
	// Material.
	//
	// Embree 4 geometry workflow:
	//   1. rtcNewGeometry()          — create geometry object
	//   2. rtcSetNewGeometryBuffer() — allocate + get pointer to vertex/index buffer
	//   3. Fill the buffers
	//   4. rtcCommitGeometry()       — finalize the geometry
	//   5. rtcAttachGeometry()       — attach to scene (returns geomID)
	//   6. rtcReleaseGeometry()      — scene owns it now; drop our handle
	///////////////////////////////////////////////////////////////////////
	cout << "Adding " << model->m_name << " to embree scene..." << flush;

	for(auto& mesh : model->m_meshes)
	{
		const uint32_t num_vertices  = mesh.m_number_of_vertices;
		const uint32_t num_triangles = num_vertices / 3;

		// 1. Create a triangle geometry object
		RTCGeometry geom = rtcNewGeometry(embree_device, RTC_GEOMETRY_TYPE_TRIANGLE);

		// 2a. Allocate vertex buffer — Embree 4 uses tightly-packed float3 (no vec4 padding)
		//     Format: RTC_FORMAT_FLOAT3, stride = 3 floats per vertex
		float* embree_vertices = (float*)rtcSetNewGeometryBuffer(
		    geom,
		    RTC_BUFFER_TYPE_VERTEX,
		    0,                       // slot
		    RTC_FORMAT_FLOAT3,
		    3 * sizeof(float),       // byte stride per vertex
		    num_vertices             // vertex count
		);

		// 3a. Transform and write vertices (x, y, z — no padding float)
		for(uint32_t i = 0; i < num_vertices; i++)
		{
			vec4 transformed = model_matrix * vec4(model->m_positions[mesh.m_start_index + i], 1.0f);
			embree_vertices[i * 3 + 0] = transformed.x;
			embree_vertices[i * 3 + 1] = transformed.y;
			embree_vertices[i * 3 + 2] = transformed.z;
		}

		// 2b. Allocate index buffer — uint3 per triangle
		//     Format: RTC_FORMAT_UINT3, stride = 3 uints per triangle
		unsigned int* embree_tri_idxs = (unsigned int*)rtcSetNewGeometryBuffer(
		    geom,
		    RTC_BUFFER_TYPE_INDEX,
		    0,                          // slot
		    RTC_FORMAT_UINT3,
		    3 * sizeof(unsigned int),   // byte stride per triangle
		    num_triangles               // triangle count
		);

		// 3b. Fill triangle indices (sequential, since vertices are already per-triangle)
		for(uint32_t i = 0; i < num_vertices; i++)
		{
			embree_tri_idxs[i] = i;
		}

		// 4. Commit the geometry (validate + preprocess)
		rtcCommitGeometry(geom);

		// 5. Attach to scene — returns the geomID used for hit lookup
		uint32_t geom_ID = rtcAttachGeometry(embree_scene, geom);
		map_geom_ID_to_mesh[geom_ID]  = &mesh;
		map_geom_ID_to_model[geom_ID] = model;

		// 6. Release our handle; the scene now owns the geometry
		rtcReleaseGeometry(geom);
	}

	cout << "done.\n";
}

///////////////////////////////////////////////////////////////////////////
// Extract an intersection from our Ray (which was populated by intersect())
///////////////////////////////////////////////////////////////////////////
Intersection getIntersection(const Ray& r)
{
	const labhelper::Model* model = map_geom_ID_to_model[r.geomID];
	const labhelper::Mesh*  mesh  = map_geom_ID_to_mesh[r.geomID];

	Intersection i;
	i.material = &(model->m_materials[mesh->m_material_idx]);

	// Interpolate shading normal from the three triangle vertex normals
	vec3 n0 = model->m_normals[((mesh->m_start_index / 3) + r.primID) * 3 + 0];
	vec3 n1 = model->m_normals[((mesh->m_start_index / 3) + r.primID) * 3 + 1];
	vec3 n2 = model->m_normals[((mesh->m_start_index / 3) + r.primID) * 3 + 2];
	float w = 1.0f - (r.u + r.v);
	i.shading_normal = normalize(w * n0 + r.u * n1 + r.v * n2);

	// Geometry normal comes from the stored Ng (un-normalized face normal)
	i.geometry_normal = -normalize(r.n);

	i.position = r.o + r.tfar * r.d;
	i.wo       = normalize(-r.d);

	// Interpolate texture coordinates
	vec2 uv0 = model->m_texture_coordinates[((mesh->m_start_index / 3) + r.primID) * 3 + 0];
	vec2 uv1 = model->m_texture_coordinates[((mesh->m_start_index / 3) + r.primID) * 3 + 1];
	vec2 uv2 = model->m_texture_coordinates[((mesh->m_start_index / 3) + r.primID) * 3 + 2];
	i.uv = w * uv0 + r.u * uv1 + r.v * uv2;

	return i;
}

///////////////////////////////////////////////////////////////////////////
// Test a ray against the scene and find the closest intersection.
//
// Embree 4 changes vs Embree 2:
//   - Use RTCRayHit (ray + hit in one struct with named scalar fields)
//   - Call rtcIntersect1() instead of rtcIntersect()
//   - hit.geomID MUST be initialized to RTC_INVALID_GEOMETRY_ID before the call
//   - No RTCIntersectContext needed (pass nullptr for RTCIntersectArguments)
///////////////////////////////////////////////////////////////////////////
bool intersect(Ray& r)
{
	RTCRayHit rayhit;

	// Fill ray fields (Embree 4 uses scalar org_x/org_y/... instead of a vec3)
	rayhit.ray.org_x = r.o.x;
	rayhit.ray.org_y = r.o.y;
	rayhit.ray.org_z = r.o.z;
	rayhit.ray.dir_x = r.d.x;
	rayhit.ray.dir_y = r.d.y;
	rayhit.ray.dir_z = r.d.z;
	rayhit.ray.tnear = r.tnear;
	rayhit.ray.tfar  = r.tfar;
	rayhit.ray.time  = r.time;
	rayhit.ray.mask  = r.mask;
	rayhit.ray.flags = 0;

	// Must initialize geomID to invalid before the call
	rayhit.hit.geomID    = RTC_INVALID_GEOMETRY_ID;
	rayhit.hit.primID    = RTC_INVALID_GEOMETRY_ID;
	rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

	// Intersect — nullptr for optional RTCIntersectArguments
	rtcIntersect1(embree_scene, &rayhit);

	// Copy hit data back into our Ray struct
	if(rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	{
		r.tfar   = rayhit.ray.tfar;
		r.u      = rayhit.hit.u;
		r.v      = rayhit.hit.v;
		r.geomID = rayhit.hit.geomID;
		r.primID = rayhit.hit.primID;
		r.instID = rayhit.hit.instID[0];
		r.n      = vec3(rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
// Test whether a ray is occluded anywhere by the scene.
//
// Embree 4 changes vs Embree 2:
//   - Use RTCRay (no hit data) instead of RTCRay with geomID
//   - Call rtcOccluded1() instead of rtcOccluded()
//   - Occlusion is signalled by tfar being set to -infinity (not geomID)
//   - No RTCIntersectContext needed (pass nullptr for RTCOccludedArguments)
///////////////////////////////////////////////////////////////////////////
bool occluded(Ray& r)
{
	RTCRay ray;

	ray.org_x = r.o.x;
	ray.org_y = r.o.y;
	ray.org_z = r.o.z;
	ray.dir_x = r.d.x;
	ray.dir_y = r.d.y;
	ray.dir_z = r.d.z;
	ray.tnear = r.tnear;
	ray.tfar  = r.tfar;
	ray.time  = r.time;
	ray.mask  = r.mask;
	ray.flags = 0;

	// Occluded — nullptr for optional RTCOccludedArguments
	rtcOccluded1(embree_scene, &ray);

	// In Embree 4, tfar is set to -infinity when the ray is occluded
	return ray.tfar < 0.0f;
}

} // namespace pathtracer
