using System.Runtime.InteropServices;
using Unity.Mathematics;

namespace GpuCrowd
{
    [StructLayout(LayoutKind.Sequential)]
    public struct InstanceData
    {
        public float4x4 objectToWorld;
        public uint clipIndex;
        public float normalizedTime;
        public float playbackSpeed;
        public float blend;
        public uint nextClip;
        public float3 pad;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VisibleInstance
    {
        public uint instanceIndex;
    }
}
