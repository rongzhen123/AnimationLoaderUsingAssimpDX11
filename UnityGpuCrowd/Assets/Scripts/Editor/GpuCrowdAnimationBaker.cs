#if UNITY_EDITOR
using System.Collections.Generic;
using GpuCrowd;
using UnityEditor;
using UnityEngine;

public static class GpuCrowdAnimationBaker
{
    [MenuItem("Tools/GPU Crowd/Bake Selected Rig Animation")]
    public static void BakeSelected()
    {
        if (Selection.activeGameObject == null)
        {
            Debug.LogError("Select a GameObject that has an Animator component.");
            return;
        }

        var root = Selection.activeGameObject;
        var animator = root.GetComponentInChildren<Animator>();
        if (animator == null || animator.runtimeAnimatorController == null)
        {
            Debug.LogError("Selected object must contain an Animator with a RuntimeAnimatorController.");
            return;
        }

        var clips = animator.runtimeAnimatorController.animationClips;
        if (clips == null || clips.Length == 0)
        {
            Debug.LogError("No animation clips found on controller.");
            return;
        }

        var smr = root.GetComponentInChildren<SkinnedMeshRenderer>();
        if (smr == null)
        {
            Debug.LogError("No SkinnedMeshRenderer found.");
            return;
        }

        int boneCount = smr.bones.Length;
        if (boneCount == 0)
        {
            Debug.LogError("Skinned mesh has no bones.");
            return;
        }

        const float sampleRate = 30f;
        var clipInfos = new List<GpuAnimationDatabase.ClipInfo>();

        int totalFrames = 0;
        foreach (var clip in clips)
        {
            int frames = Mathf.Max(1, Mathf.CeilToInt(clip.length * sampleRate));
            clipInfos.Add(new GpuAnimationDatabase.ClipInfo
            {
                name = clip.name,
                startFrame = totalFrames,
                frameCount = frames,
                fps = sampleRate
            });
            totalFrames += frames;
        }

        int width = boneCount * 3;
        int height = totalFrames;

        var tex = new Texture2D(width, height, TextureFormat.RGBAHalf, false, true)
        {
            filterMode = FilterMode.Point,
            wrapMode = TextureWrapMode.Clamp,
            name = root.name + "_AnimTex"
        };

        // NOTE: This scaffold writes identity matrices. Replace with actual sampled pose matrices
        // by evaluating each clip/frame and writing skinned bone matrices in clip/frame rows.
        var pixels = new Color[width * height];
        for (int y = 0; y < height; y++)
        {
            for (int b = 0; b < boneCount; b++)
            {
                int x = b * 3;
                pixels[y * width + x + 0] = new Color(1, 0, 0, 0);
                pixels[y * width + x + 1] = new Color(0, 1, 0, 0);
                pixels[y * width + x + 2] = new Color(0, 0, 1, 0);
            }
        }

        tex.SetPixels(pixels);
        tex.Apply(false, true);

        string rootPath = "Assets/Resources";
        if (!AssetDatabase.IsValidFolder(rootPath))
        {
            AssetDatabase.CreateFolder("Assets", "Resources");
        }

        string texPath = $"{rootPath}/{tex.name}.asset";
        AssetDatabase.CreateAsset(tex, texPath);

        var db = ScriptableObject.CreateInstance<GpuAnimationDatabase>();
        db.animationTexture = AssetDatabase.LoadAssetAtPath<Texture2D>(texPath);
        db.boneCount = boneCount;
        db.totalFrameCount = totalFrames;
        db.clips = clipInfos;

        string dbPath = $"{rootPath}/{root.name}_GpuAnimDb.asset";
        AssetDatabase.CreateAsset(db, dbPath);

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();

        Debug.Log($"GPU crowd bake scaffold complete. Database: {dbPath}, texture: {texPath}");
    }
}
#endif
