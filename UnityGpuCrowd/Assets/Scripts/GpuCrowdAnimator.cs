using System;
using System.Runtime.InteropServices;
using Unity.Mathematics;
using UnityEngine;
using UnityEngine.Rendering;

namespace GpuCrowd
{
    public sealed class GpuCrowdAnimator : MonoBehaviour
    {
        [Header("Render")]
        [SerializeField] private Mesh mesh;
        [SerializeField] private Material material;
        [SerializeField] private ShadowCastingMode shadowCastingMode = ShadowCastingMode.Off;
        [SerializeField] private bool receiveShadows;

        [Header("Animation")]
        [SerializeField] private GpuAnimationDatabase animationDatabase;
        [SerializeField] private int defaultClipIndex;
        [SerializeField] private float playbackSpeedMin = 0.9f;
        [SerializeField] private float playbackSpeedMax = 1.1f;

        [Header("Instances")]
        [SerializeField] private int instanceCount = 10000;
        [SerializeField] private Vector2 spawnRange = new(250f, 250f);

        [Header("Culling")]
        [SerializeField] private ComputeShader cullingShader;
        [SerializeField] private float maxDrawDistance = 220f;

        private GraphicsBuffer instanceBuffer;
        private GraphicsBuffer visibleIndexBuffer;
        private GraphicsBuffer argsBuffer;
        private GraphicsBuffer clipMetaBuffer;

        private Bounds drawBounds;
        private InstanceData[] instanceData;

        private static readonly int AnimTexId = Shader.PropertyToID("_AnimTex");
        private static readonly int BoneCountId = Shader.PropertyToID("_BoneCount");
        private static readonly int ClipMetaBufferId = Shader.PropertyToID("_ClipMetaBuffer");
        private static readonly int InstanceBufferId = Shader.PropertyToID("_InstanceBuffer");
        private static readonly int VisibleIndexBufferId = Shader.PropertyToID("_VisibleIndexBuffer");

        private static readonly int CsInstanceBuffer = Shader.PropertyToID("_InstanceBuffer");
        private static readonly int CsVisibleIndexBuffer = Shader.PropertyToID("_VisibleIndexBuffer");
        private static readonly int CsArgsBuffer = Shader.PropertyToID("_ArgsBuffer");
        private static readonly int CsInstanceCount = Shader.PropertyToID("_InstanceCount");
        private static readonly int CsViewProj = Shader.PropertyToID("_ViewProj");
        private static readonly int CsCameraPosition = Shader.PropertyToID("_CameraPosition");
        private static readonly int CsMaxDistance = Shader.PropertyToID("_MaxDistance");

        private int cullKernel;

        [StructLayout(LayoutKind.Sequential)]
        private struct ClipMeta
        {
            public int startFrame;
            public int frameCount;
            public float fps;
            public float pad;
        }

        private void OnEnable()
        {
            if (!Validate())
            {
                enabled = false;
                return;
            }

            BuildInstanceData();
            AllocateBuffers();
            UploadStaticData();
        }

        private void OnDisable()
        {
            ReleaseBuffers();
        }

        private bool Validate()
        {
            return mesh != null &&
                   material != null &&
                   animationDatabase != null &&
                   animationDatabase.animationTexture != null &&
                   cullingShader != null &&
                   animationDatabase.clips.Count > 0;
        }

        private void BuildInstanceData()
        {
            instanceData = new InstanceData[instanceCount];
            var random = new Unity.Mathematics.Random(0xBADC0FFEu);

            for (var i = 0; i < instanceCount; i++)
            {
                float x = random.NextFloat(-spawnRange.x, spawnRange.x);
                float z = random.NextFloat(-spawnRange.y, spawnRange.y);
                float y = 0f;

                float3 position = new(x, y, z);
                quaternion rotation = quaternion.RotateY(random.NextFloat(0f, math.PI * 2f));
                float4x4 matrix = float4x4.TRS(position, rotation, new float3(1f));

                instanceData[i] = new InstanceData
                {
                    objectToWorld = matrix,
                    clipIndex = (uint)math.clamp(defaultClipIndex, 0, animationDatabase.clips.Count - 1),
                    normalizedTime = random.NextFloat(0f, 1f),
                    playbackSpeed = random.NextFloat(playbackSpeedMin, playbackSpeedMax),
                    blend = 0f,
                    nextClip = (uint)math.clamp(defaultClipIndex, 0, animationDatabase.clips.Count - 1),
                    pad = float3.zero
                };
            }

            drawBounds = new Bounds(Vector3.zero, new Vector3(spawnRange.x * 2f + 20f, 40f, spawnRange.y * 2f + 20f));
        }

        private void AllocateBuffers()
        {
            instanceBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, instanceCount, Marshal.SizeOf<InstanceData>());
            visibleIndexBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, instanceCount, Marshal.SizeOf<VisibleInstance>());
            argsBuffer = new GraphicsBuffer(GraphicsBuffer.Target.IndirectArguments, 1, 5 * sizeof(uint));

            ClipMeta[] clipMeta = new ClipMeta[animationDatabase.clips.Count];
            for (int i = 0; i < clipMeta.Length; i++)
            {
                var c = animationDatabase.clips[i];
                clipMeta[i] = new ClipMeta
                {
                    startFrame = c.startFrame,
                    frameCount = c.frameCount,
                    fps = math.max(c.fps, 1f),
                    pad = 0f
                };
            }

            clipMetaBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, clipMeta.Length, Marshal.SizeOf<ClipMeta>());
            clipMetaBuffer.SetData(clipMeta);

            var args = new uint[5];
            args[0] = (uint)mesh.GetIndexCount(0);
            args[1] = 0;
            args[2] = (uint)mesh.GetIndexStart(0);
            args[3] = (uint)mesh.GetBaseVertex(0);
            args[4] = 0;
            argsBuffer.SetData(args);

            cullKernel = cullingShader.FindKernel("CSMain");
        }

        private void UploadStaticData()
        {
            instanceBuffer.SetData(instanceData);

            material.SetTexture(AnimTexId, animationDatabase.animationTexture);
            material.SetInt(BoneCountId, animationDatabase.boneCount);
            material.SetBuffer(ClipMetaBufferId, clipMetaBuffer);
            material.SetBuffer(InstanceBufferId, instanceBuffer);
            material.SetBuffer(VisibleIndexBufferId, visibleIndexBuffer);
        }

        private void Update()
        {
            if (instanceData == null) return;

            float dt = Time.deltaTime;
            for (int i = 0; i < instanceData.Length; i++)
            {
                var d = instanceData[i];
                d.normalizedTime += dt * d.playbackSpeed;
                d.normalizedTime = d.normalizedTime - math.floor(d.normalizedTime);
                instanceData[i] = d;
            }

            instanceBuffer.SetData(instanceData);
            RunCulling();

            Graphics.DrawMeshInstancedIndirect(
                mesh,
                0,
                material,
                drawBounds,
                argsBuffer,
                0,
                null,
                shadowCastingMode,
                receiveShadows,
                gameObject.layer);
        }

        private void RunCulling()
        {
            var camera = Camera.main;
            if (camera == null) return;

            var vp = camera.projectionMatrix * camera.worldToCameraMatrix;

            cullingShader.SetBuffer(cullKernel, CsInstanceBuffer, instanceBuffer);
            cullingShader.SetBuffer(cullKernel, CsVisibleIndexBuffer, visibleIndexBuffer);
            cullingShader.SetBuffer(cullKernel, CsArgsBuffer, argsBuffer);
            cullingShader.SetInt(CsInstanceCount, instanceCount);
            cullingShader.SetMatrix(CsViewProj, vp);
            cullingShader.SetVector(CsCameraPosition, camera.transform.position);
            cullingShader.SetFloat(CsMaxDistance, maxDrawDistance);

            cullingShader.SetBuffer(cullKernel, CsArgsBuffer, argsBuffer);

            var resetArgs = new uint[5]
            {
                (uint)mesh.GetIndexCount(0),
                0,
                (uint)mesh.GetIndexStart(0),
                (uint)mesh.GetBaseVertex(0),
                0
            };
            argsBuffer.SetData(resetArgs);

            int threadGroups = Mathf.CeilToInt(instanceCount / 64f);
            cullingShader.Dispatch(cullKernel, threadGroups, 1, 1);
        }

        private void ReleaseBuffers()
        {
            instanceBuffer?.Release();
            visibleIndexBuffer?.Release();
            argsBuffer?.Release();
            clipMetaBuffer?.Release();

            instanceBuffer = null;
            visibleIndexBuffer = null;
            argsBuffer = null;
            clipMetaBuffer = null;
        }
    }
}
