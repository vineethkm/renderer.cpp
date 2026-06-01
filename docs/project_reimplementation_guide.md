# Path Tracing Renderer: Full Project Guide and Reimplementation Notes

## 1) What this project is
This is a CPU path tracer with a real-time interactive preview window.

- Rendering core: CPU path tracing in `src/Pathtracer.cpp`
- Ray-scene acceleration/intersection: Embree 4 in `src/embree.cpp`
- Scene assets: OBJ + MTL + textures + HDR environment maps in `scenes/`
- UI and display: SDL2 + OpenGL + ImGui in `src/main.cpp`
- Model/asset loading helpers: `labhelper/`

The OpenGL side is only for presenting the progressively converged image and debug overlays; the lighting solution itself is computed on CPU.

## 2) Build and runtime model
### Build (`Makefile`)
- Compiles C++17 with OpenMP (`-fopenmp`)
- Pulls SDL2 flags from `pkg-config`
- Links `-lembree4`, `-lGLEW`, `-lGL`, `-lpthread`, etc.
- Builds all `.cpp` under `src/` and `labhelper/`, plus core ImGui backend files.

### Runtime loop (`src/main.cpp`)
Per frame:
1. Handle input/UI events.
2. If window or subsampling changed: resize internal render buffer.
3. Call `pathtracer::tracePaths(...)` once (adds one new sample iteration).
4. Upload float RGB render buffer to GL texture.
5. Draw fullscreen quad with that texture.
6. Draw ImGui controls + optional light overlays.

This gives a progressive render: each frame increases sample count and reduces noise.

## 3) High-level architecture
## Entry and orchestration
- `src/main.cpp`
  - Scene setup (`loadScenes`, `changeScene`)
  - Camera movement/orbit input
  - Path tracer settings and material/light UI controls
  - Calls into path tracer and Embree wrappers

## Path tracer core
- `src/Pathtracer.h/.cpp`
  - Settings/state (`rendered_image`, lights, environment)
  - Progressive accumulation
  - Primary ray generation
  - Recursive radiance estimator `Li(...)`

## Intersection and BVH
- `src/embree.h/.cpp`
  - Converts loaded triangle meshes into Embree geometries
  - Builds scene BVH (`rtcCommitScene`)
  - Provides:
    - `intersect(Ray&)` closest hit
    - `occluded(Ray&)` shadow test
    - `getIntersection(...)` shading payload extraction

## Materials and sampling
- `src/material.h/.cpp`
  - BRDF/BTDF/BSDF abstractions
  - Implemented: Lambertian diffuse
  - Declared but mostly stubbed: microfacet, dielectric, metal blends
- `src/sampling.h/.cpp`
  - Thread-local random generation
  - Concentric disk sampling
  - Cosine hemisphere sampling

## Image and assets
- `src/HDRImage.h/.cpp`: loads `.hdr` using stb and samples by UV
- `labhelper/Model.h/.cpp`: OBJ/MTL load, mesh split by material, texture loading, GPU buffer upload

## 4) Scene and data flow
1. `main.cpp` loads OBJ models via `labhelper::loadModelFromOBJ`.
2. `changeScene(...)` resets Embree scene and adds all scene models.
3. `pathtracer::buildBVH()` commits Embree acceleration structure.
4. Each frame, tracer generates camera rays for each pixel.
5. Rays intersect Embree scene; if hit, `Li` computes outgoing radiance.
6. Radiance is averaged into accumulation buffer (`rendered_image.data`).
7. Buffer is displayed via OpenGL textured fullscreen quad.

## 5) Algorithms used (detailed)
## 5.1 Progressive Monte Carlo integration
In `tracePaths` each frame computes one additional sample pass over all pixels.

Accumulation formula per pixel:
- Let `n = rendered_image.number_of_samples`
- Old mean = `M_n`
- New sample = `x_{n+1}`
- Updated mean:

`M_{n+1} = M_n * (n/(n+1)) + x_{n+1} * (1/(n+1))`

This is numerically stable online averaging and avoids storing all past samples.

## 5.2 Primary ray generation (pinhole camera)
For each pixel:
- Convert pixel coordinates to normalized screen coordinates [0,1]
- Apply jitter inside pixel (`jitterX`, `jitterY`) for anti-aliasing
- Convert to NDC [-1,1]
- Unproject through inverse `(P*V)`
- Ray origin = camera position
- Ray direction = normalized(worldPoint - cameraPos)

This is standard stochastic pinhole ray casting.

## 5.3 Jittered sampling / stochastic AA
`tracePaths` perturbs each pixel sample with random offsets before ray generation.

Effect:
- Breaks structured aliasing
- Turns aliasing into noise
- Noise decreases with more samples

## 5.4 Direct lighting with hard shadows
In `Li`:
- Compute direction to point light `wi`
- Spawn shadow ray from hit point offset by `EPSILON` along geometric normal
- Set shadow `tfar` to distance to light minus epsilon
- If `occluded(shadowRay)` is false, add contribution

Contribution used:
- Inverse-square attenuation
- Lambertian BRDF (`albedo / pi`)
- Cosine term `max(dot(wi,n), 0)`

This is next-event estimation for a single point light, with binary visibility.

## 5.5 Recursive mirror-style reflections
Also in `Li`:
- Compute reflected direction `reflect(current_ray.d, n)`
- Trace recursively with decremented depth
- If miss, sample environment map along reflected direction

Reflectivity is approximated as:
- average of RGB albedo
- multiplied by global `settings.reflection_strength`

This is a heuristic reflectivity model (not energy-conserving PBR).

## 5.6 Indirect diffuse bounce
- Sample one cosine-weighted hemisphere direction using `Diffuse::sample_wi`
- Shoot indirect ray
- Recursively evaluate radiance from that direction
- Multiply by BRDF and cosine

This approximates one-sample Monte Carlo diffuse GI per bounce.

Important: code currently does not divide by PDF explicitly in throughput form. Because sampling is cosine-weighted and BRDF is Lambertian, a simplified expression can still work visually, but the estimator is not written in a clean general MIS/PDF form.

## 5.7 Environment map lighting
When rays miss geometry, `Lenvironment(wi)` is used:
- Convert world direction to spherical coordinates:
  - `theta = acos(wi.y)`
  - `phi = atan2(wi.z, wi.x)` remapped to [0,2pi)
- UV mapping:
  - `u = phi/(2pi)`
  - `v = 1 - theta/pi`
- Sample HDR texture and multiply by environment intensity.

This is lat-long environment lookup.

## 5.8 Embree acceleration workflow
For each mesh:
1. Create triangle geometry (`rtcNewGeometry`)
2. Allocate vertex and index buffers (`rtcSetNewGeometryBuffer`)
3. Write transformed vertices and triangle indices
4. Commit geometry
5. Attach to scene, save `geomID -> mesh/model` maps
6. Release geometry handle

After all geometries: `rtcCommitScene` builds/updates BVH.

Intersection payload reconstruction (`getIntersection`):
- Material by mesh material index
- Interpolate shading normal using barycentrics
- Interpolate UV using barycentrics
- Face forward normals against outgoing direction

## 5.9 Parallelism
`#pragma omp parallel for` over image rows.
- Shared accumulation buffer writes are safe because each thread writes unique pixel indices.
- RNG uses thread-indexed generators (`generators[omp_get_thread_num()]`).

Caveat: RNG generators are never explicitly seeded in `sampling.cpp`, so repeatability/random quality may be weak depending on default seeds and thread scheduling.

## 6) Current capabilities vs stubs
## Implemented and active
- Progressive path tracing
- Point light direct illumination with shadow rays
- Environment lighting
- Recursive reflections
- Indirect diffuse bounce (single sampled direction per bounce)
- Multi-sample per pixel (SPP)
- Jittered AA
- Scene switching and interactive camera

## Declared but not really implemented
In `material.cpp` these return zero/placeholder behavior:
- `MicrofacetBRDF::f`
- `BSDF::fresnel`
- `DielectricBSDF::f`
- `MetalBSDF::f`
- `BSDFLinearBlend` methods

So physically based material system is architected, but mostly not wired into `Li` yet.

## 7) Practical reimplementation blueprint (from scratch)
Recommended order:
1. Window + fullscreen texture display + progressive float buffer.
2. Camera rays and environment map sampling.
3. Embree scene setup + closest-hit queries.
4. Lambertian shading + point light + shadow rays.
5. Progressive accumulation.
6. Cosine-sampled indirect diffuse bounce.
7. Reflection bounce and recursion depth control.
8. UI controls for quality/perf.
9. Material system upgrade (microfacet + Fresnel + transmission).

This order keeps the renderer testable at every stage.

## 8) Known technical debt / risks
- Energy conservation is not enforced across diffuse + reflection terms.
- Indirect estimator is not generalized using explicit `throughput *= f * cos / pdf`.
- Reflection reflectivity heuristic is based on base color average only.
- RNG seeding/thread strategy is simplistic.
- `HDRImage::sample` uses nearest lookup, no filtering.
- No tone mapping/gamma correction pass; output is copied directly.
- OpenMP generator array assumes max 24 threads.

## 9) Feature ideas and concrete implementation directions
## High impact rendering quality
1. Proper BSDF throughput path tracing
- Convert `Li` to iterative path loop.
- Track `throughput` and multiply by `f * abs(dot(n,wi)) / pdf` each bounce.
- Add Russian roulette after bounce 3-5.

2. GGX microfacet specular
- Implement GGX NDF, Smith masking-shadowing, Schlick Fresnel.
- Support roughness-metallic workflow from material fields/textures.

3. Refraction/transmission
- Use dielectric BSDF with Snell + Fresnel split.
- Handle total internal reflection.

4. Multiple importance sampling (MIS)
- Combine direct-light sampling and BSDF sampling for lower variance.
- Start with balance heuristic.

5. Better environment lighting
- Build importance sampling CDF over env map luminance * sin(theta).
- Sample env lights directly for faster convergence.

## Performance features
6. Adaptive sampling
- Per-pixel variance estimation; stop sampling converged pixels.

7. Tiled rendering + dynamic scheduling
- Better thread load balancing and cache locality.

8. Temporal accumulation for interactive camera motion
- Reprojection + history clamping (if moving toward real-time preview quality).

9. Denoiser integration
- Add Intel OIDN/OptiX denoiser post-process.
- Export albedo/normal AOVs for better denoising.

## Lighting and effects
10. Area lights done properly
- The project already has `disc_lights`; integrate actual sampling in `Li`.

11. Emissive geometry
- Treat emissive materials as light sources with direct sampling.

12. Depth of field + thin lens
- Sample lens disk and focal plane intersection.

13. Motion blur
- Use ray time and animated transforms.

14. Participating media
- Homogeneous fog first, then heterogeneous volumes.

## Usability and engineering
15. Scene description file
- Replace hard-coded scene setup with JSON/TOML scene config.

16. Deterministic mode
- Seeded RNG per pixel/sample for reproducible debugging.

17. Render output pipeline
- EXR output, tone mapping (ACES/Reinhard), gamma/sRGB conversion.

18. Validation tests
- Unit tests for sampling PDFs, BRDF invariants, and intersection correctness.

## 10) Suggested “from scratch” module boundaries
If rebuilding cleanly, split into these modules:
- `core/math` (ray, frame, transforms)
- `core/sampling` (rng, hemisphere, disk, light sampling)
- `scene` (camera, materials, geometry, textures, lights)
- `integrator` (path tracer, direct lighting, MIS)
- `accel` (Embree bridge)
- `io` (obj/mtl/hdr/exr)
- `app` (window/UI/input)

Keep integrator independent of UI/OpenGL so you can run offline batch renders easily.

## 11) Mapping current files to responsibilities
- `src/main.cpp`: app loop, UI, camera, scene switching, display upload
- `src/Pathtracer.*`: path tracing algorithm and accumulation
- `src/embree.*`: BVH and intersections
- `src/material.*`: scattering model interfaces and implementations
- `src/sampling.*`: random sampling primitives
- `src/HDRImage.*`: HDR loading/sampling
- `labhelper/Model.*`: OBJ/MTL/texture loading and mesh/material data
- `src/copyTexture.*`: fullscreen texture blit shader
- `src/simple.*`: flat-color debug drawing shader

## 12) Minimal correctness checklist during reimplementation
- Ray misses return environment radiance.
- Surface normals oriented consistently relative to outgoing direction.
- Shadow rays offset by epsilon and capped by light distance.
- Progressive average matches offline accumulation when camera is static.
- SPP increase reduces noise approximately as `1/sqrt(N)`.
- Increasing max bounces affects indirect illumination visibly.

---
This document reflects the current code behavior, including incomplete material models and heuristic reflection handling, so you can both reproduce it faithfully and know exactly where to improve.
