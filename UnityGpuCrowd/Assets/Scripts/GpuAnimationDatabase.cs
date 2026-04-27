using System;
using System.Collections.Generic;
using UnityEngine;

namespace GpuCrowd
{
    [CreateAssetMenu(menuName = "GPU Crowd/Animation Database", fileName = "GpuAnimationDatabase")]
    public sealed class GpuAnimationDatabase : ScriptableObject
    {
        [Serializable]
        public struct ClipInfo
        {
            public string name;
            public int startFrame;
            public int frameCount;
            public float fps;
        }

        [Header("Baked animation matrix texture")]
        public Texture2D animationTexture;
        public int boneCount;
        public int totalFrameCount;

        [Header("Clip metadata")]
        public List<ClipInfo> clips = new();

        public bool TryGetClip(int clipIndex, out ClipInfo clip)
        {
            if (clipIndex >= 0 && clipIndex < clips.Count)
            {
                clip = clips[clipIndex];
                return true;
            }

            clip = default;
            return false;
        }
    }
}
