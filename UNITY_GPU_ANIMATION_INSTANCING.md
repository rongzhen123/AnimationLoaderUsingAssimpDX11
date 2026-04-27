# Unity GPU Animation Instancing Design (10,000 Animated Characters)

This document describes a practical production design for rendering **~10,000 skinned characters** in Unity by replacing CPU skinning (`SkinnedMeshRenderer`) with **GPU animation instancing**.

---

## 1) Why `SkinnedMeshRenderer` fails at 10k

`SkinnedMeshRenderer` updates bone transforms and skinning per character on CPU (or per-renderer GPU path with significant overhead), causing:
- High main-thread + render-thread cost
- High draw-call pressure
- Poor batching because each character has unique animation state

At 10,000 agents, even simple rigs typically bottleneck CPU before GPU is saturated.

---

## 2) Target architecture

Use this pipeline:

1. **Offline bake animations to textures**
   - For each clip/frame/bone, store bone matrix rows in animation textures.
2. **Use static bind-pose mesh instead of `SkinnedMeshRenderer`**
   - Mesh stores `boneIndices` + `boneWeights` in vertex attributes.
3. **Render with GPU instancing**
   - One material per LOD/state family.
   - Per-instance data: world matrix, clip index, normalized time, playback speed, animation params.
4. **Vertex shader does skinning**
   - Sample animation texture, reconstruct bone matrices, blend up to 4 bones.
5. **Crowd simulation updates only lightweight instance data**
   - CPU updates transform + animation time only.
6. **Optional:** Use `DrawMeshInstancedIndirect` + compute culling for max scale.

---

## 3) Data layout (recommended)

### 3.1 Animation texture packing

For each animation clip:
- `frameCount` frames, `boneCount` bones.
- Per bone matrix: 3 rows (float4 each) = 12 floats (3x4 matrix).

Pack as texture coordinates:
- X axis = `boneIndex * 3 + row`
- Y axis = `frameIndex + clipOffset`

Texture format options:
- Quality-first: `RGBAHalf`
- Memory-first: quantize to 16-bit/10-bit custom packing (advanced)

### 3.2 Per-instance buffer

Store in `StructuredBuffer<InstanceData>`:

```hlsl
struct InstanceData
{
    float4x4 objectToWorld;
    uint clipIndex;
    float normalizedTime;
    float speed;
    float animLerp;      // optional for cross-fade
    uint nextClipIndex;  // optional
    float4 colorTint;    // optional variation
};
```

For 10k units, this is manageable and cache-friendly.

---

## 4) Shader design (core idea)

### 4.1 Vertex inputs
- Position/normal/tangent/uv
- Bone indices (`uint4`) and weights (`float4`)

### 4.2 Bone sampling
Pseudo-code:

```hlsl
float3x4 LoadBoneMatrix(uint clipIndex, uint frameIndex, uint boneIndex)
{
    uint x0 = boneIndex * 3;
    uint y  = frameIndex + _ClipStartFrame[clipIndex];

    float4 r0 = _AnimTex.Load(int3(x0 + 0, y, 0));
    float4 r1 = _AnimTex.Load(int3(x0 + 1, y, 0));
    float4 r2 = _AnimTex.Load(int3(x0 + 2, y, 0));

    return float3x4(r0, r1, r2);
}
```

### 4.3 Vertex skinning
For each of 4 influences:
1. Load matrix from current frame (and optionally next frame for interpolation)
2. Transform position/normal
3. Weighted blend

Then transform skinned position by instance `objectToWorld`.

---

## 5) CPU system design in Unity

### 5.1 Authoring/bake step (Editor)
Create an editor baker:
- Input: model with rig + clips
- Output:
  - Static mesh (with bone weights/indices)
  - Animation texture asset(s)
  - Clip metadata (start frame, frame count, fps)

### 5.2 Runtime crowd manager
Use one manager per archetype:
- `NativeArray<InstanceData>` for simulation-side state
- Upload to `ComputeBuffer`/`GraphicsBuffer`
- Submit draw calls per LOD with:
  - `Graphics.DrawMeshInstancedIndirect` (preferred at 10k)
  - or `Graphics.DrawMeshInstanced` for smaller counts

### 5.3 LOD strategy (critical)
Use at least 3 tiers:
- **LOD0 (near):** full GPU skinned mesh
- **LOD1 (mid):** fewer bones / half animation rate
- **LOD2 (far):** impostor or rigid billboard

Also update animation at reduced tick rate for distant units.

---

## 6) Cross-fade and variation

To avoid robotic motion:
- Keep per-instance random phase offset at spawn
- Support 2-clip blend in shader (`clipA` + `clipB`, `blendWeight`)
- Add small playback speed variation (e.g. 0.9~1.1)

For performance, limit blend windows and avoid perpetual dual-clip sampling for all LODs.

---

## 7) Culling pipeline

For 10,000 characters, CPU frustum culling can still cost too much.

Recommended:
1. Compute shader frustum + distance culling
2. Write visible instances to append buffer
3. Build indirect args buffer
4. Draw with `DrawMeshInstancedIndirect`

Optional extension:
- Hi-Z occlusion culling for dense city scenes

---

## 8) Memory and performance budgeting (rule of thumb)

Example (100 bones, 30 FPS, 20 clips, avg 2 sec/clip):
- Total frames ≈ `20 * 60 = 1200`
- Matrix texels/frame ≈ `100 bones * 3 = 300 texels`
- Total texels ≈ `360,000`
- With `RGBAHalf` (8 bytes/texel) ≈ `2.75 MB`

Even with multiple characters/archetypes, this is usually cheaper than CPU skinning cost at scale.

---

## 9) Minimal implementation checklist

- [ ] Build animation baker (clips -> matrix texture)
- [ ] Build metadata asset (`clipStart`, `frameCount`, `fps`)
- [ ] Create instanced skinning shader (URP/HDRP compatible path)
- [ ] Replace `SkinnedMeshRenderer` crowd with indirect instancing manager
- [ ] Add LOD + reduced update frequencies
- [ ] Add compute culling
- [ ] Profile with Unity Profiler + RenderDoc

---

## 10) Practical pitfalls

- Precision issues when compressing matrices too aggressively
- Wrong bind-pose handling (must include inverse bind pose correctly)
- Tangent/normal skinning mismatch causing lighting artifacts
- Excess material variants killing batching
- Re-uploading large buffers every frame unnecessarily

---

## 11) Suggested milestones

1. **Milestone A:** 1 clip, no blend, 1k instances stable
2. **Milestone B:** multi-clip + per-instance time, 5k stable
3. **Milestone C:** indirect + compute culling, 10k stable
4. **Milestone D:** LOD/impostor integration, shipping budget

---

## 12) Expected result

With correct implementation (indirect instancing + GPU skinning + LOD/culling), rendering **10,000 animated characters** is feasible on modern desktop GPUs, and often practical on high-end mobile with stricter LOD/clip constraints.
