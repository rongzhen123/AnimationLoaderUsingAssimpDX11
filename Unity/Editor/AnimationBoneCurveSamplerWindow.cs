using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[Serializable]
public class BoneRootRelativeTransformCurveAsset : ScriptableObject
{
    public string sourceClipName;
    public string sourceBonePath;
    public float sampleRate = 20f;

    public AnimationCurve posX = new AnimationCurve();
    public AnimationCurve posY = new AnimationCurve();
    public AnimationCurve posZ = new AnimationCurve();

    public AnimationCurve rotX = new AnimationCurve();
    public AnimationCurve rotY = new AnimationCurve();
    public AnimationCurve rotZ = new AnimationCurve();
    public AnimationCurve rotW = new AnimationCurve();

    public AnimationCurve scaleX = new AnimationCurve();
    public AnimationCurve scaleY = new AnimationCurve();
    public AnimationCurve scaleZ = new AnimationCurve();

    public Matrix4x4 EvaluateMatrix(float time)
    {
        var pos = new Vector3(posX.Evaluate(time), posY.Evaluate(time), posZ.Evaluate(time));
        var rot = new Quaternion(rotX.Evaluate(time), rotY.Evaluate(time), rotZ.Evaluate(time), rotW.Evaluate(time));
        var quatLengthSq = (rot.x * rot.x) + (rot.y * rot.y) + (rot.z * rot.z) + (rot.w * rot.w);
        if (quatLengthSq > 0f)
        {
            rot = Quaternion.Normalize(rot);
        }
        else
        {
            rot = Quaternion.identity;
        }

        var scale = new Vector3(scaleX.Evaluate(time), scaleY.Evaluate(time), scaleZ.Evaluate(time));
        return Matrix4x4.TRS(pos, rot, scale);
    }
}

public class AnimationBoneCurveSamplerWindow : EditorWindow
{
    private const float SampleRate = 20f;

    private GameObject targetRoot;
    private Animator animator;

    private AnimationClip[] clips = Array.Empty<AnimationClip>();
    private string[] clipNames = Array.Empty<string>();
    private int selectedClipIndex;

    private readonly List<Transform> bones = new List<Transform>();
    private readonly List<string> bonePaths = new List<string>();
    private int selectedBoneIndex;

    private BoneRootRelativeTransformCurveAsset playbackAsset;
    private GameObject playbackTarget;
    private Transform playbackInitTransform;
    private float playbackTime;

    [MenuItem("Tools/Animation/Bone Curve Sampler")]
    public static void Open()
    {
        GetWindow<AnimationBoneCurveSamplerWindow>("Bone Curve Sampler");
    }

    private void OnEnable()
    {
        TryBindFromSelection();
    }

    private void OnSelectionChange()
    {
        Repaint();
    }

    private void OnGUI()
    {
        EditorGUILayout.LabelField("Source", EditorStyles.boldLabel);

        using (new EditorGUILayout.HorizontalScope())
        {
            targetRoot = (GameObject)EditorGUILayout.ObjectField("GameObject", targetRoot, typeof(GameObject), true);
            if (GUILayout.Button("Use Selected", GUILayout.Width(120f)))
            {
                TryBindFromSelection();
            }
        }

        if (targetRoot == null)
        {
            EditorGUILayout.HelpBox("Select a GameObject with an Animator.", MessageType.Info);
            return;
        }

        if (animator == null)
        {
            if (!TryBindAnimator(targetRoot))
            {
                EditorGUILayout.HelpBox("Selected GameObject does not have an Animator component.", MessageType.Warning);
                return;
            }
        }

        if (clips.Length == 0)
        {
            EditorGUILayout.HelpBox("Animator has no clips in its RuntimeAnimatorController.", MessageType.Warning);
            if (GUILayout.Button("Refresh"))
            {
                RefreshClips();
            }

            return;
        }

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Clip", EditorStyles.boldLabel);
        selectedClipIndex = EditorGUILayout.Popup("Animation Clip", selectedClipIndex, clipNames);

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Bone", EditorStyles.boldLabel);
        selectedBoneIndex = EditorGUILayout.Popup("Bone Transform", selectedBoneIndex, bonePaths.ToArray());

        EditorGUILayout.Space();
        EditorGUILayout.HelpBox("Sampling transform relative to selected root at 20 FPS.", MessageType.None);

        using (new EditorGUI.DisabledScope(bones.Count == 0 || clips.Length == 0))
        {
            if (GUILayout.Button("Sample Bone To Curve Asset"))
            {
                SampleAndSave();
            }
        }

        EditorGUILayout.Space(16f);
        EditorGUILayout.LabelField("Apply Saved Curve Asset", EditorStyles.boldLabel);
        playbackAsset = (BoneRootRelativeTransformCurveAsset)EditorGUILayout.ObjectField(
            "Curve Asset",
            playbackAsset,
            typeof(BoneRootRelativeTransformCurveAsset),
            false);
        playbackTarget = (GameObject)EditorGUILayout.ObjectField("Target GameObject", playbackTarget, typeof(GameObject), true);
        playbackInitTransform = (Transform)EditorGUILayout.ObjectField(
            "Init Transform",
            playbackInitTransform,
            typeof(Transform),
            true);
        playbackTime = Mathf.Max(0f, EditorGUILayout.FloatField("Sample Time (s)", playbackTime));

        using (new EditorGUI.DisabledScope(playbackAsset == null || playbackTarget == null))
        {
            if (GUILayout.Button("Apply Matrix To Target"))
            {
                ApplySavedMatrixToTarget();
            }
        }
    }

    private void TryBindFromSelection()
    {
        if (Selection.activeGameObject != null)
        {
            targetRoot = Selection.activeGameObject;
        }

        if (targetRoot == null)
        {
            animator = null;
            clips = Array.Empty<AnimationClip>();
            clipNames = Array.Empty<string>();
            bones.Clear();
            bonePaths.Clear();
            selectedClipIndex = 0;
            selectedBoneIndex = 0;
            return;
        }

        if (!TryBindAnimator(targetRoot))
        {
            return;
        }

        RefreshClips();
        RefreshBones();
    }

    private bool TryBindAnimator(GameObject root)
    {
        animator = root.GetComponent<Animator>();
        return animator != null;
    }

    private void RefreshClips()
    {
        clips = Array.Empty<AnimationClip>();
        clipNames = Array.Empty<string>();
        selectedClipIndex = 0;

        if (animator == null || animator.runtimeAnimatorController == null)
        {
            return;
        }

        var unique = new Dictionary<string, AnimationClip>();
        foreach (var clip in animator.runtimeAnimatorController.animationClips)
        {
            if (clip == null)
            {
                continue;
            }

            if (!unique.ContainsKey(clip.name))
            {
                unique.Add(clip.name, clip);
            }
        }

        clips = new AnimationClip[unique.Count];
        clipNames = new string[unique.Count];

        var i = 0;
        foreach (var pair in unique)
        {
            clipNames[i] = pair.Key;
            clips[i] = pair.Value;
            i++;
        }
    }

    private void RefreshBones()
    {
        bones.Clear();
        bonePaths.Clear();
        selectedBoneIndex = 0;

        if (targetRoot == null)
        {
            return;
        }

        var all = targetRoot.GetComponentsInChildren<Transform>(true);
        foreach (var t in all)
        {
            bones.Add(t);
            bonePaths.Add(GetRelativePath(targetRoot.transform, t));
        }
    }

    private void SampleAndSave()
    {
        if (selectedClipIndex < 0 || selectedClipIndex >= clips.Length)
        {
            EditorUtility.DisplayDialog("Sampling Failed", "Please choose a valid animation clip.", "OK");
            return;
        }

        if (selectedBoneIndex < 0 || selectedBoneIndex >= bones.Count)
        {
            EditorUtility.DisplayDialog("Sampling Failed", "Please choose a valid bone transform.", "OK");
            return;
        }

        var clip = clips[selectedClipIndex];
        var bone = bones[selectedBoneIndex];

        if (clip == null || bone == null)
        {
            EditorUtility.DisplayDialog("Sampling Failed", "Clip or bone is null.", "OK");
            return;
        }

        var file = EditorUtility.SaveFilePanelInProject(
            "Save Bone Curve Asset",
            $"{targetRoot.name}_{clip.name}_{bone.name}_RootRelativeCurves",
            "asset",
            "Choose where to save the sampled curve asset.");

        if (string.IsNullOrEmpty(file))
        {
            return;
        }

        var curveAsset = CreateInstance<BoneRootRelativeTransformCurveAsset>();
        curveAsset.sourceClipName = clip.name;
        curveAsset.sourceBonePath = bonePaths[selectedBoneIndex];
        curveAsset.sampleRate = SampleRate;

        var previousSample = AnimationMode.InAnimationMode();

        try
        {
            if (!previousSample)
            {
                AnimationMode.StartAnimationMode();
            }

            var totalSamples = Mathf.FloorToInt(clip.length * SampleRate) + 1;
            for (var i = 0; i < totalSamples; i++)
            {
                var time = i / SampleRate;
                AnimationMode.SampleAnimationClip(targetRoot, clip, time);

                var rootRelativeMatrix = targetRoot.transform.worldToLocalMatrix * bone.localToWorldMatrix;
                var rootRelativePos = rootRelativeMatrix.GetColumn(3);
                var rootRelativeRot = rootRelativeMatrix.rotation;
                var rootRelativeScale = ExtractScale(rootRelativeMatrix);

                AddKey(curveAsset.posX, time, rootRelativePos.x);
                AddKey(curveAsset.posY, time, rootRelativePos.y);
                AddKey(curveAsset.posZ, time, rootRelativePos.z);

                AddKey(curveAsset.rotX, time, rootRelativeRot.x);
                AddKey(curveAsset.rotY, time, rootRelativeRot.y);
                AddKey(curveAsset.rotZ, time, rootRelativeRot.z);
                AddKey(curveAsset.rotW, time, rootRelativeRot.w);

                AddKey(curveAsset.scaleX, time, rootRelativeScale.x);
                AddKey(curveAsset.scaleY, time, rootRelativeScale.y);
                AddKey(curveAsset.scaleZ, time, rootRelativeScale.z);
            }

            AssetDatabase.CreateAsset(curveAsset, file);
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();

            EditorUtility.DisplayDialog("Done", $"Saved sampled curves to:\n{file}", "OK");
        }
        finally
        {
            if (!previousSample)
            {
                AnimationMode.StopAnimationMode();
            }
        }
    }

    private static void AddKey(AnimationCurve curve, float time, float value)
    {
        curve.AddKey(new Keyframe(time, value));
    }

    private void ApplySavedMatrixToTarget()
    {
        if (playbackAsset == null || playbackTarget == null)
        {
            EditorUtility.DisplayDialog("Apply Failed", "Please assign both curve asset and target GameObject.", "OK");
            return;
        }

        var relativeMatrix = playbackAsset.EvaluateMatrix(playbackTime);
        var worldMatrix = targetRoot != null
            ? targetRoot.transform.localToWorldMatrix * relativeMatrix
            : relativeMatrix;
        if (playbackInitTransform != null)
        {
            var initPositionOffset = Matrix4x4.Translate(playbackInitTransform.localPosition);
            worldMatrix = initPositionOffset * worldMatrix;
        }

        Undo.RecordObject(playbackTarget.transform, "Apply Bone Matrix From Curve Asset");
        ApplyWorldMatrix(playbackTarget.transform, worldMatrix);
        EditorUtility.SetDirty(playbackTarget.transform);
    }

    private static void ApplyWorldMatrix(Transform target, Matrix4x4 worldMatrix)
    {
        var parentWorldToLocal = target.parent != null
            ? target.parent.worldToLocalMatrix
            : Matrix4x4.identity;

        var localMatrix = parentWorldToLocal * worldMatrix;

        target.localPosition = localMatrix.GetColumn(3);
        target.localRotation = localMatrix.rotation;
    }

    private static Vector3 ExtractScale(Matrix4x4 matrix)
    {
        var x = new Vector3(matrix.m00, matrix.m10, matrix.m20).magnitude;
        var y = new Vector3(matrix.m01, matrix.m11, matrix.m21).magnitude;
        var z = new Vector3(matrix.m02, matrix.m12, matrix.m22).magnitude;
        return new Vector3(x, y, z);
    }

    private static string GetRelativePath(Transform root, Transform target)
    {
        if (root == target)
        {
            return root.name;
        }

        var stack = new Stack<string>();
        var current = target;

        while (current != null && current != root)
        {
            stack.Push(current.name);
            current = current.parent;
        }

        if (current == null)
        {
            return target.name;
        }

        return string.Join("/", stack.ToArray());
    }
}
