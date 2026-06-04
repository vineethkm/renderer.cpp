#pragma once
#include "embree.h"
#include <Model.h>
#include <glm/glm.hpp>

namespace cpu {
// Add a model to the scene
void addModel(const labhelper::Model *model, const glm::mat4 &model_matrix);

// Build an acceleration structure for the scene
void buildBVH();

// Reinitialize the scene
void reinitScene();

///////////////////////////////////////////////////////////////////////////
// Ray intersection functions
///////////////////////////////////////////////////////////////////////////

// Test a ray against the scene and find the closest intersection.
// Populates the hit fields of `r` on success.
bool intersect(pathtracer::Ray &r);

// Returns intersection details for a ray that has already been passed
// through intersect() successfully.
pathtracer::Intersection getIntersection(const pathtracer::Ray &r);

// Test whether a ray is occluded anywhere by the scene.
// Does NOT find the closest hit — just returns true/false.
bool occluded(pathtracer::Ray &r);
} // namespace cpu
