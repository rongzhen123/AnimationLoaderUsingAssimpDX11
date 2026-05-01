# UGUI Sprite Damage Number System (No-Layout Optimized)

This guide provides a production-ready floating damage number system for Unity using UGUI sprites with object pooling and manual digit placement (no `HorizontalLayoutGroup`).

## Goals
- Avoid runtime allocations and GC spikes.
- Keep draw calls low via a single sprite atlas/material.
- Support normal/crit/heal style variants.
- Scale to high hit rates on mobile and desktop.

## Script 1: `DamageNumberStyle.cs`

```csharp
using UnityEngine;

[System.Serializable]
public class DamageNumberStyle
{
    [Header("Visual")]
    public Color color = Color.white;
    public float baseScale = 1f;
    public float jitterX = 18f;
    public float jitterY = 8f;
    public float digitSpacing = -2f;

    [Header("Timing")]
    public float lifetime = 0.8f;
    public float risePixels = 70f;

    [Header("Curves")]
    public AnimationCurve riseCurve = AnimationCurve.EaseInOut(0, 0, 1, 1);
    public AnimationCurve alphaCurve = AnimationCurve.EaseInOut(0, 1, 1, 0);
    public AnimationCurve scaleCurve = new AnimationCurve(
        new Keyframe(0f, 0.75f),
        new Keyframe(0.15f, 1.2f),
        new Keyframe(0.35f, 1f),
        new Keyframe(1f, 1f)
    );
}
```

## Script 2: `DamageNumberUI.cs` (manual digit placement)

```csharp
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class DamageNumberUI : MonoBehaviour
{
    [SerializeField] private RectTransform root;
    [SerializeField] private RectTransform digitsRoot;
    [SerializeField] private CanvasGroup canvasGroup;
    [SerializeField] private Image digitPrefab;

    private readonly List<Image> _digits = new();

    private Camera _camera;
    private Transform _target;
    private Vector3 _worldOffset;
    private Vector2 _jitter;
    private DamageNumberStyle _style;
    private float _timer;

    private Sprite[] _digitSprites; // 0..9
    private Sprite _minusSprite;
    private System.Action<DamageNumberUI> _onComplete;

    public void SetupSprites(Sprite[] digitSprites, Sprite minusSprite)
    {
        _digitSprites = digitSprites;
        _minusSprite = minusSprite;
    }

    public void Play(Camera cam, Transform target, Vector3 worldOffset, int amount, DamageNumberStyle style, System.Action<DamageNumberUI> onComplete)
    {
        _camera = cam;
        _target = target;
        _worldOffset = worldOffset;
        _style = style;
        _onComplete = onComplete;
        _timer = 0f;

        _jitter = new Vector2(
            Random.Range(-_style.jitterX, _style.jitterX),
            Random.Range(-_style.jitterY, _style.jitterY)
        );

        gameObject.SetActive(true);
        canvasGroup.alpha = 1f;
        root.localScale = Vector3.one * _style.baseScale;

        ApplyNumber(amount);

        StopAllCoroutines();
        StartCoroutine(Animate());
    }

    public void StopAndHide()
    {
        StopAllCoroutines();
        gameObject.SetActive(false);
    }

    private void ApplyNumber(int amount)
    {
        string str = amount.ToString();
        EnsureDigits(str.Length);

        float totalWidth = 0f;
        int activeCount = str.Length;

        for (int i = 0; i < activeCount; i++)
        {
            var img = _digits[i];
            char c = str[i];

            img.gameObject.SetActive(true);
            img.raycastTarget = false;
            img.color = _style.color;

            if (c == '-')
                img.sprite = _minusSprite;
            else
                img.sprite = _digitSprites[c - '0'];

            img.SetNativeSize();
            totalWidth += img.rectTransform.sizeDelta.x;
            if (i < activeCount - 1) totalWidth += _style.digitSpacing;
        }

        for (int i = activeCount; i < _digits.Count; i++)
            _digits[i].gameObject.SetActive(false);

        float cursorX = -totalWidth * 0.5f;
        for (int i = 0; i < activeCount; i++)
        {
            RectTransform rt = _digits[i].rectTransform;
            float w = rt.sizeDelta.x;
            rt.anchoredPosition = new Vector2(cursorX + (w * 0.5f), 0f);
            cursorX += w + _style.digitSpacing;
        }
    }

    private void EnsureDigits(int needed)
    {
        while (_digits.Count < needed)
        {
            var img = Instantiate(digitPrefab, digitsRoot);
            _digits.Add(img);
        }
    }

    private IEnumerator Animate()
    {
        while (_timer < _style.lifetime)
        {
            _timer += Time.deltaTime;
            float t = Mathf.Clamp01(_timer / _style.lifetime);

            if (_target != null)
            {
                Vector3 world = _target.position + _worldOffset;
                Vector2 screen = RectTransformUtility.WorldToScreenPoint(_camera, world);
                float rise = _style.riseCurve.Evaluate(t) * _style.risePixels;
                root.position = screen + _jitter + new Vector2(0f, rise);
            }

            canvasGroup.alpha = _style.alphaCurve.Evaluate(t);
            float s = _style.scaleCurve.Evaluate(t) * _style.baseScale;
            root.localScale = Vector3.one * s;

            yield return null;
        }

        _onComplete?.Invoke(this);
    }
}
```

## Script 3: `DamageNumberManager.cs` (pool + API)

```csharp
using System.Collections.Generic;
using UnityEngine;

public class DamageNumberManager : MonoBehaviour
{
    public static DamageNumberManager Instance { get; private set; }

    public enum DamageKind { Normal, Crit, Heal }

    [Header("Refs")]
    [SerializeField] private Canvas targetCanvas;
    [SerializeField] private Camera worldCamera;
    [SerializeField] private DamageNumberUI damageNumberPrefab;

    [Header("Sprites")]
    [SerializeField] private Sprite[] digitSprites = new Sprite[10];
    [SerializeField] private Sprite minusSprite;

    [Header("Pool")]
    [SerializeField] private int prewarmCount = 40;
    [SerializeField] private int maxActive = 180;

    [Header("Styles")]
    [SerializeField] private DamageNumberStyle normalStyle;
    [SerializeField] private DamageNumberStyle critStyle;
    [SerializeField] private DamageNumberStyle healStyle;

    [Header("Position")]
    [SerializeField] private Vector3 defaultWorldOffset = new Vector3(0f, 1.6f, 0f);

    private readonly Queue<DamageNumberUI> _pool = new();
    private readonly List<DamageNumberUI> _active = new();

    private void Awake()
    {
        if (Instance != null && Instance != this)
        {
            Destroy(gameObject);
            return;
        }

        Instance = this;

        if (worldCamera == null)
            worldCamera = Camera.main;

        for (int i = 0; i < prewarmCount; i++)
        {
            var item = CreateItem();
            item.gameObject.SetActive(false);
            _pool.Enqueue(item);
        }
    }

    public void ShowDamage(Transform target, int amount, DamageKind kind = DamageKind.Normal)
    {
        if (target == null) return;

        DamageNumberStyle style = kind switch
        {
            DamageKind.Crit => critStyle,
            DamageKind.Heal => healStyle,
            _ => normalStyle
        };

        var item = GetOrRecycle();
        _active.Add(item);

        item.Play(worldCamera, target, defaultWorldOffset, amount, style, ReturnToPool);
    }

    private DamageNumberUI CreateItem()
    {
        var item = Instantiate(damageNumberPrefab, targetCanvas.transform);
        item.SetupSprites(digitSprites, minusSprite);
        return item;
    }

    private DamageNumberUI GetOrRecycle()
    {
        if (_pool.Count > 0)
            return _pool.Dequeue();

        if (_active.Count >= maxActive)
        {
            var recycled = _active[0];
            _active.RemoveAt(0);
            recycled.StopAndHide();
            return recycled;
        }

        return CreateItem();
    }

    private void ReturnToPool(DamageNumberUI item)
    {
        if (item == null) return;

        item.StopAndHide();
        _active.Remove(item);
        _pool.Enqueue(item);
    }
}
```

## Prefab Checklist

1. Create `Canvas` named `DamageCanvas`.
   - Render Mode: **Screen Space - Overlay** (or Screen Space - Camera with world camera assigned).
   - `CanvasScaler`: `Scale With Screen Size`, reference `1920x1080`, match `0.5`.

2. Create prefab `DamageNumberUI`.
   - Root: `RectTransform` + `CanvasGroup` + `DamageNumberUI` script.
   - Child `DigitsRoot`: plain `RectTransform` (no layout components).
   - Child `DigitPrefab`: `Image`, `Preserve Aspect = true`, `Raycast Target = false`.

3. Create `DamageNumberManager` object.
   - Assign `targetCanvas`, `worldCamera`, and `damageNumberPrefab`.
   - Assign digit sprites (`0..9`) and `minusSprite`.
   - Set prewarm/max values.
   - Configure style presets for normal/crit/heal.

## Performance Notes
- Keep all digit sprites in one atlas to maximize batching.
- Avoid changing materials per hit.
- Prewarm pool at scene load.
- If hit rates are extreme, aggregate DOT ticks (e.g., per 0.1s bucket).


## Merge Rapid DoT Hits (recommended)

For poison/burn ticks, showing one number per tick can spam the UI.
A simple approach is to aggregate hits per target in a short time bucket (for example, 0.10s) and then emit one merged number.

### Add-on script: `DotDamageAggregator.cs`

```csharp
using System.Collections.Generic;
using UnityEngine;

public class DotDamageAggregator : MonoBehaviour
{
    [SerializeField] private float mergeWindowSeconds = 0.10f;

    private readonly Dictionary<int, Bucket> _buckets = new();

    private struct Bucket
    {
        public Transform target;
        public int total;
        public float deadline;
        public DamageNumberManager.DamageKind kind;
    }

    public void AddTick(Transform target, int amount, DamageNumberManager.DamageKind kind)
    {
        if (target == null || amount == 0) return;

        int id = target.GetInstanceID();
        float now = Time.unscaledTime;

        if (_buckets.TryGetValue(id, out var b))
        {
            b.total += amount;
            b.deadline = now + mergeWindowSeconds;
            b.kind = kind;
            _buckets[id] = b;
            return;
        }

        _buckets[id] = new Bucket
        {
            target = target,
            total = amount,
            deadline = now + mergeWindowSeconds,
            kind = kind
        };
    }

    private void Update()
    {
        if (_buckets.Count == 0) return;

        float now = Time.unscaledTime;
        List<int> done = null;

        foreach (var kv in _buckets)
        {
            if (kv.Value.deadline > now) continue;

            if (kv.Value.target != null)
                DamageNumberManager.Instance.ShowDamage(kv.Value.target, kv.Value.total, kv.Value.kind);

            done ??= new List<int>(8);
            done.Add(kv.Key);
        }

        if (done == null) return;

        for (int i = 0; i < done.Count; i++)
            _buckets.Remove(done[i]);
    }
}
```

### Usage
- Call `AddTick(target, tickDamage, DamageNumberManager.DamageKind.Normal)` instead of `ShowDamage` for DoT ticks.
- Keep `mergeWindowSeconds` between **0.08s and 0.15s** for readable but responsive feedback.
- If your game pauses by `timeScale = 0`, `Time.unscaledTime` keeps aggregation deterministic for UI.


## Feature Extensions Requested

### 1) Crit effect: shake + star sprite prefix

Add an optional prefix sprite for critical hits (for example, a star icon) and apply a short shake during the first animation phase.

#### `DamageNumberStyle` additions
```csharp
public bool enableShake = false;
public float shakeAmplitude = 10f;
public float shakeFrequency = 40f;
public Sprite prefixSprite; // e.g. crit star
```

#### `DamageNumberUI` implementation notes
- When building the visual string, reserve slot 0 for `prefixSprite` when provided.
- During animation, add a temporary x/y offset:

```csharp
float shake = 0f;
if (_style.enableShake && t < 0.25f)
    shake = Mathf.Sin(t * _style.shakeFrequency) * _style.shakeAmplitude * (1f - (t / 0.25f));

root.position = screen + _jitter + new Vector2(shake, rise);
```

### 2) Abbreviation support (e.g. `12500 -> 12.5K`)

Use a formatter before digit rendering so very large values stay readable.

```csharp
private static string FormatAbbrev(float value)
{
    float abs = Mathf.Abs(value);

    if (abs >= 1_000_000_000f) return (value / 1_000_000_000f).ToString("0.#") + "B";
    if (abs >= 1_000_000f)     return (value / 1_000_000f).ToString("0.#") + "M";
    if (abs >= 1_000f)         return (value / 1_000f).ToString("0.#") + "K";

    return value.ToString("0");
}
```

If you use suffixes (`K/M/B`), add sprites for letters or render suffix via a tiny TMP child for hybrid mode.

### 3) Color by damage type (fire, ice, poison)

Prefer explicit type mapping in manager-level config.

```csharp
public enum ElementType { Physical, Fire, Ice, Poison, Lightning }

[System.Serializable]
public struct ElementColorMap
{
    public ElementType element;
    public Color color;
}

[SerializeField] private ElementColorMap[] elementColors;

private Color ResolveElementColor(ElementType type, Color fallback)
{
    for (int i = 0; i < elementColors.Length; i++)
        if (elementColors[i].element == type)
            return elementColors[i].color;

    return fallback;
}
```

Then clone style color for the spawn:

```csharp
var style = kind == DamageKind.Crit ? critStyle : normalStyle;
Color c = ResolveElementColor(elementType, style.color);
// pass color override into Play/ApplyNumber
```

### 4) Locale formatting (if needed)

For non-abbreviated numbers, use culture-aware formatting:

```csharp
using System.Globalization;

private static string FormatLocale(int value, CultureInfo culture)
{
    return value.ToString("N0", culture); // 12,500 or 12 500 depending on locale
}
```

Recommended policy:
- **Combat readability first**: use abbreviated values in intense gameplay.
- **Locale correctness in slower contexts**: logs, summaries, tooltips.
- Expose a setting: `UseAbbrevInCombat` + `CultureCode`.
