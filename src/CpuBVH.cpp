#include "CpuBVH.hpp"
#include <algorithm>
#include <cfloat>
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;

namespace cpu {

// Axis Aligned Bounding Boxes
struct AABB {
  glm::vec3 lo{FLT_MAX, FLT_MAX, FLT_MAX};
  glm::vec3 hi{-FLT_MAX, -FLT_MAX, -FLT_MAX};

  void expand(const glm::vec3 &p) {
    lo = glm::min(lo, p);
    hi = glm::max(hi, p);
  }

  // Does it intersect the AABB?
  bool intersect(const pathtracer::Ray &r, float &tmin_out) const {
    glm::vec3 inv_d = 1.0f / r.d;
    glm::vec3 t0 = (lo - r.o) * inv_d;
    glm::vec3 t1 = (hi - r.o) * inv_d;
    glm::vec3 tmin3 = glm::min(t0, t1);
    glm::vec3 tmax3 = glm::max(t0, t1);
    float tnear = std::max({tmin3.x, tmin3.y, tmin3.z, r.tnear});
    float tfar = std::min({tmax3.x, tmax3.y, tmax3.z, r.tfar});
    tmin_out = tnear;
    return tnear <= tfar;
  }
};

struct Triangle {
  glm::vec3 v0, v1, v2;
  uint32_t geomID; // to link to model, mesh and material
  uint32_t primID; // to index into g_triangles
};

// BVHNode
struct BVHNode {
  AABB bounds;
  uint32_t left = 0;
  uint32_t right = 0;
  uint32_t first = 0;
  uint32_t count = 0;
};

vector<Triangle> g_triangles;
vector<BVHNode> g_nodes;
unordered_map<uint32_t, const labhelper::Model *> map_geom_ID_to_model;
unordered_map<uint32_t, const labhelper::Mesh *> map_geom_ID_to_mesh;
uint32_t g_next_geom_id = 0;

// number of triangles in a BVH Node to be considered a leafnode
const int LEAF_SIZE = 4;

uint32_t buildRecursive(vector<Triangle> &tris, uint32_t start,
                        uint32_t count) {
  const uint32_t nodeIdx = (uint32_t)(g_nodes.size());
  g_nodes.push_back({});

  AABB bounds;
  for (uint32_t i = start; i < start + count; i++) {
    bounds.expand(tris[i].v0);
    bounds.expand(tris[i].v1);
    bounds.expand(tris[i].v2);
  }

  if (count <= (uint32_t)LEAF_SIZE) {
    g_nodes[nodeIdx].bounds = bounds;
    g_nodes[nodeIdx].first = start;
    g_nodes[nodeIdx].count = count;
    return nodeIdx;
  }

  // Split along the longest axis at the spatial midpoint
  glm::vec3 extent = bounds.hi - bounds.lo;
  int axis = 0;
  if (extent.y > extent.x)
    axis = 1;
  if (extent.z > extent[axis])
    axis = 2;
  float mid = (bounds.lo[axis] + bounds.hi[axis]) * 0.5f;

  // triangles with centroid < mid to the left
  uint32_t left_count = 0;
  for (uint32_t i = 0; i < count; i++) {
    float c = (tris[start + i].v0[axis] + tris[start + i].v1[axis] +
               tris[start + i].v2[axis]) /
              3.0f;
    if (c < mid) {
      std::swap(tris[start + i], tris[start + left_count]);
      left_count++;
    }
  }

  // If everything ended up on one side, just cut in half by index
  if (left_count == 0 || left_count == count)
    left_count = count / 2;

  uint32_t left_child = buildRecursive(tris, start, left_count);
  uint32_t right_child =
      buildRecursive(tris, start + left_count, count - left_count);

  g_nodes[nodeIdx].bounds = bounds;
  g_nodes[nodeIdx].left = left_child;
  g_nodes[nodeIdx].right = right_child;
  g_nodes[nodeIdx].count = 0;

  return nodeIdx;
}

// Moller-Trumbore triagnel intersection
bool intersectTriangle(pathtracer::Ray &r, const Triangle &tri, float &u_out,
                       float &v_out) {
  const glm::vec3 edge1 = tri.v1 - tri.v0;
  const glm::vec3 edge2 = tri.v2 - tri.v0;
  const glm::vec3 h = cross(r.d, edge2);
  const float a = dot(edge1, h);
  if (a > -1e-7f && a < 1e-7f)
    return false; // ray parallel to triangle

  const float f = 1.0f / a;
  const glm::vec3 s = r.o - tri.v0;
  const float u = f * dot(s, h);
  if (u < 0.0f || u > 1.0f)
    return false;

  const glm::vec3 q = cross(s, edge1);
  const float v = f * dot(r.d, q);
  if (v < 0.0f || u + v > 1.0f)
    return false;

  const float t = f * dot(edge2, q);
  if (t <= r.tnear || t >= r.tfar)
    return false;

  r.tfar = t;
  u_out = u;
  v_out = v;
  return true;
}

bool traverseBVH(pathtracer::Ray &r, bool any_hit) {
  if (g_nodes.empty())
    return false;

  uint32_t stack[128];
  int top = 0;
  stack[top++] = 0; // root

  bool hit = false;

  while (top > 0) {
    const BVHNode &node = g_nodes[stack[--top]];

    float tmin;
    if (!node.bounds.intersect(r, tmin))
      continue;

    if (node.count > 0) {
      // Leaf: test every triangle
      for (uint32_t i = node.first; i < node.first + node.count; i++) {
        float u, v;
        if (intersectTriangle(r, g_triangles[i], u, v)) {
          const Triangle &tri = g_triangles[i];
          r.u = u;
          r.v = v;
          r.geomID = tri.geomID;
          r.primID = tri.primID;
          // Store unnormalized face normal (matches Embree's Ng convention)
          r.n = cross(tri.v1 - tri.v0, tri.v2 - tri.v0);
          hit = true;
          if (any_hit)
            return true;
        }
      }
    } else {
      // Inner node: push children, nearer child last (processed first)
      float t_left, t_right;
      bool hl = g_nodes[node.left].bounds.intersect(r, t_left);
      bool hr = g_nodes[node.right].bounds.intersect(r, t_right);

      if (hl && hr) {
        if (t_left <= t_right) {
          stack[top++] = node.right; // far  → processed last
          stack[top++] = node.left;  // near → processed first
        } else {
          stack[top++] = node.left;
          stack[top++] = node.right;
        }
      } else if (hl) {
        stack[top++] = node.left;
      } else if (hr) {
        stack[top++] = node.right;
      }
    }
  }

  return hit;
}

void addModel(const labhelper::Model *model, const glm::mat4 &model_matrix) {
  cout << "CPU BVH: adding " << model->m_name << "..." << flush;

  for (const auto &mesh : model->m_meshes) {
    uint32_t geomID = g_next_geom_id++;
    map_geom_ID_to_model[geomID] = model;
    map_geom_ID_to_mesh[geomID] = &mesh;

    const uint32_t num_tris = mesh.m_number_of_vertices / 3;
    for (uint32_t i = 0; i < num_tris; i++) {
      const uint32_t base = mesh.m_start_index + i * 3;
      Triangle tri;
      tri.v0 = glm::vec3(model_matrix *
                         glm::vec4(model->m_positions[base + 0], 1.0f));
      tri.v1 = glm::vec3(model_matrix *
                         glm::vec4(model->m_positions[base + 1], 1.0f));
      tri.v2 = glm::vec3(model_matrix *
                         glm::vec4(model->m_positions[base + 2], 1.0f));
      tri.geomID = geomID;
      tri.primID = i;
      g_triangles.push_back(tri);
    }
  }

  cout << "done.\n";
}

void buildBVH() {
  cout << "CPU BVH: building " << g_triangles.size() << " triangles..." << endl
       << flush;
  g_nodes.clear();
  if (!g_triangles.empty()) {
    g_nodes.reserve(2 * g_triangles.size()); // reserve this size
    buildRecursive(g_triangles, 0, (uint32_t)(g_triangles.size()));
  }
  cout << "done. (" << g_nodes.size() << " nodes)\n";
}

void reinitScene() {
  g_triangles.clear();
  g_nodes.clear();
  map_geom_ID_to_model.clear();
  map_geom_ID_to_mesh.clear();
  g_next_geom_id = 0;
}

bool intersect(pathtracer::Ray &r) { return traverseBVH(r, false); }

pathtracer::Intersection getIntersection(const pathtracer::Ray &r) {
  const labhelper::Model *model = map_geom_ID_to_model[r.geomID];
  const labhelper::Mesh *mesh = map_geom_ID_to_mesh[r.geomID];

  pathtracer::Intersection i;
  i.material = &(model->m_materials[mesh->m_material_idx]);

  // Vertex index base for this triangle (mirrors embree.cpp's getIntersection)
  const uint32_t tri_base = (mesh->m_start_index / 3 + r.primID) * 3;

  glm::vec3 n0 = model->m_normals[tri_base + 0];
  glm::vec3 n1 = model->m_normals[tri_base + 1];
  glm::vec3 n2 = model->m_normals[tri_base + 2];
  float w = 1.0f - (r.u + r.v);
  i.shading_normal = normalize(w * n0 + r.u * n1 + r.v * n2);

  i.geometry_normal = normalize(r.n);
  i.position = r.o + r.tfar * r.d;
  i.wo = normalize(-r.d);

  i.entering = dot(r.d, i.geometry_normal) < 0.0f;

  if (dot(i.shading_normal, i.wo) < 0.0f)
    i.shading_normal = -i.shading_normal;
  if (dot(i.geometry_normal, i.wo) < 0.0f)
    i.geometry_normal = -i.geometry_normal;

  glm::vec2 uv0 = model->m_texture_coordinates[tri_base + 0];
  glm::vec2 uv1 = model->m_texture_coordinates[tri_base + 1];
  glm::vec2 uv2 = model->m_texture_coordinates[tri_base + 2];
  i.uv = w * uv0 + r.u * uv1 + r.v * uv2;

  return i;
}

bool occluded(pathtracer::Ray &r) { return traverseBVH(r, true); }

} // namespace cpu
