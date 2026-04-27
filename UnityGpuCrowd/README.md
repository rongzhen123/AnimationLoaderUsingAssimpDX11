# Unity GPU Crowd Animation Project

This folder contains a Unity project scaffold implementing a GPU animation instancing system designed for very large crowds (target: up to ~10,000 animated units with LOD/culling).

## Unity version
- 2022.3 LTS (URP compatible with minor shader adjustments)

## What's included
- Animation texture/bone metadata database (`GpuAnimationDatabase`)
- Runtime instanced renderer manager (`GpuCrowdAnimator`)
- Compute-based frustum + distance culling (`GpuFrustumCull.compute`)
- GPU skinning instanced shader (`GpuCrowdInstanced.shader`)
- Editor-side baker scaffold (`GpuCrowdAnimationBaker`)
- Spawn helper (`GpuCrowdSpawner`)

## Setup (step-by-step)
1. Open this folder in Unity Hub as a project.
2. Create a material using shader: `Custom/GpuCrowdInstanced`.
3. Create a `GpuAnimationDatabase` asset from `Create > GPU Crowd > Animation Database`.
4. Add one crowd prefab mesh and populate its `bone indices/weights` in UV2/UV3 (or adapt shader input channels).
5. Use `Tools > GPU Crowd > Bake Selected Rig Animation` on your animated source object to generate the animation texture + metadata.
6. Create an empty GameObject in scene and add `GpuCrowdAnimator` and `GpuCrowdSpawner`.
7. Assign:
   - Mesh
   - Material
   - Animation Database
   - Frustum culling compute shader
8. Press Play and tune:
   - `instanceCount`
   - `maxDrawDistance`
   - `playbackSpeedMin/Max`

## Notes
- This is a complete code scaffold and runtime path, but authoring pipelines vary per project rig format. You may need to adapt the bake step to your skeleton/import settings.
- For production, add occlusion culling and LOD meshes/impostors for far distance.
