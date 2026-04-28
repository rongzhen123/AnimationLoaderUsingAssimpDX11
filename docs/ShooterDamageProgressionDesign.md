# Shooter Weapon/Enemy Damage & Progression System Design

This document defines a practical, tunable gameplay framework for a wave-based shooter with:

- Enemy spawn system.
- In-game update system.
- Enemy death score drop.
- Player score collection and weapon upgrading.
- Outer (meta/session-level) update system.

The design emphasizes:

1. **Gameplay enjoyment** (clear power moments, low frustration spikes).
2. **Numerical fairness** (predictable damage, manageable variance).
3. **Progression potential** (short-term upgrades + long-term mastery).

---

## 1) Core Data Model

Use data-first structs/tables (JSON/CSV/Scriptable-like assets) so design can tune values without code rebuilds.

## 1.1 Damage & Combat Definitions

```cpp
struct DamageProfile {
    float baseDamage;             // raw weapon/enemy damage
    float critChance;             // 0..1
    float critMultiplier;         // e.g. 1.5
    float armorPenetration;       // flat or % depending on game
    float statusBuildUp;          // for burn/poison/stun accumulation
};

struct DefenseProfile {
    float armor;                  // effective damage reduction parameter
    float resistance[4];          // e.g. Physical, Fire, Ice, Electric
    float dodgeChance;            // 0..1 (avoid fully if true)
    float damageTakenMultiplier;  // final scalar
};
```

### Recommended damage formula (stable and fair)

Use a **smooth armor formula** instead of hard subtraction:

```text
mitigatedDamage = rawDamage * (100 / (100 + armor))
```

Then:

```text
finalDamage = mitigatedDamage * typeResistance * damageTakenMultiplier
```

Why this is good:
- Never drops damage to zero unexpectedly.
- Armor gains have diminishing returns.
- Easy to explain and tune.

## 1.2 Weapon Data

```cpp
struct WeaponLevelData {
    int level;
    float damageMultiplier;
    float fireRateMultiplier;
    float reloadMultiplier;
    float spreadMultiplier;
    int scoreCostToNext;          // score needed to unlock next level
};

struct WeaponData {
    string weaponId;
    DamageProfile damage;
    float fireRate;
    int magazineSize;
    float reloadTime;
    float projectileSpeed;
    float spread;
    vector<WeaponLevelData> levelCurve;
};
```

## 1.3 Enemy Data

```cpp
struct EnemyArchetype {
    string enemyId;
    int tier;                     // 1=trash, 2=elite, 3=boss
    float baseHP;
    float baseMoveSpeed;
    DamageProfile attack;
    DefenseProfile defense;
    int scoreDropBase;
    float scoreDropVariance;      // e.g. 0.1 -> +/-10%
    float spawnWeight;            // used by weighted spawn pool
};
```

## 1.4 Progression State

```cpp
struct PlayerProgressionState {
    int scoreUncollectedWorld;    // dropped in world
    int scoreBanked;              // collected score
    int weaponLevel;
    int metaCurrency;             // optional long-term progression
};

struct MatchDifficultyState {
    int waveIndex;
    float intensity;              // director pressure metric
    float expectedTTK;            // target time to kill standard enemy
    float expectedSurvival;       // target player survivability window
};
```

---

## 2) Enemy Spawn System (Wave + Director Hybrid)

Use **wave budget** + **live intensity feedback** to avoid boring or unfair spikes.

### 2.1 Spawn budget per wave

```text
waveBudget = baseBudget + waveIndex * budgetGrowth
```

Each enemy has a cost:

```text
enemyCost = tierCost[tier] * HPFactor * DPSFactor
```

Spawn until budget exhausted, using weighted random from allowed pool.

### 2.2 Intensity regulation

Track a rolling pressure score:

```text
intensity = f(activeEnemyCount, incomingDPS, nearMissEvents, playerHPTrend)
```

Rules:
- If intensity too high: reduce elite chance, add spawn delay.
- If intensity too low: reduce delay, add flank spawns, increase speed slightly.

This creates a “breathing” combat rhythm.

### 2.3 Spawn safety fairness rules

- No spawn within `safeRadius` of player line-of-sight in last `safeTime` seconds.
- Cap simultaneous high-tier enemies.
- Boss spawns only when arena is not already overloaded.

---

## 3) In-Game Update System

Use a fixed-step simulation loop for deterministic and stable tuning.

```cpp
void GameUpdate(float dt) {
    InputSystem::Update(dt);
    SpawnSystem::Update(dt);
    AISystem::Update(dt);
    ProjectileSystem::Update(dt);
    CombatSystem::Update(dt);      // damage resolution
    DeathSystem::Update(dt);       // kill detection + drops
    PickupSystem::Update(dt);      // player collects score
    ProgressionSystem::Update(dt); // weapon upgrades
    UISystem::Update(dt);
}
```

### 3.1 CombatSystem::ResolveHit example

```cpp
float ResolveDamage(const HitContext& hit) {
    if (Random01() < hit.target.defense.dodgeChance) return 0.0f;

    float raw = hit.attacker.damage.baseDamage;

    if (Random01() < hit.attacker.damage.critChance)
        raw *= hit.attacker.damage.critMultiplier;

    float effectiveArmor = max(0.0f, hit.target.defense.armor - hit.attacker.damage.armorPenetration);
    float mitigated = raw * (100.0f / (100.0f + effectiveArmor));

    float typed = mitigated * hit.target.defense.resistance[hit.damageType];
    return typed * hit.target.defense.damageTakenMultiplier;
}
```

---

## 4) Enemy Death -> Score Drop System

When enemy HP <= 0:

1. Mark dead state.
2. Trigger death VFX/SFX.
3. Spawn score pickup(s).
4. Add kill stats.

### 4.1 Score drop formula

```text
scoreDrop = round(scoreDropBase * waveScalar * random(1-variance, 1+variance))
```

Where:

```text
waveScalar = 1 + waveIndex * 0.03
```

Guideline:
- Trash enemies: frequent small drops.
- Elites: medium guaranteed drop + chance for bonus orb.
- Boss: large drop burst + guaranteed upgrade shard (optional).

### 4.2 Anti-feel-bad protections

- Pickups magnetize to player at close range.
- Pickups persist at least N seconds.
- Auto-collect leftovers at wave end.

---

## 5) Score Collection -> Weapon Upgrade System

Two models are viable:

1. **Instant level-up** once banked score reaches threshold.
2. **Shop between waves** (stronger strategy pacing).

For fairness and readability, start with instant level-up but gate frequency.

### 5.1 Upgrade cost curve

Use gently accelerating cost:

```text
cost(level) = baseCost * pow(level, 1.35)
```

Example:
- L1->L2: 100
- L2->L3: 255
- L3->L4: 441
- L4->L5: 650

### 5.2 Upgrade payout curve

Per level (example):

- +12% damage
- +6% fire rate
- -4% reload time
- slight spread improvement every 2 levels

Important: keep compounded DPS gain near ~15-20% per level early, taper late game.

### 5.3 ProgressionSystem update

```cpp
void ProgressionSystem::Update(float dt) {
    while (state.scoreBanked >= CostToNext(state.weaponLevel) && state.weaponLevel < maxWeaponLevel) {
        state.scoreBanked -= CostToNext(state.weaponLevel);
        state.weaponLevel++;
        ApplyWeaponLevel(state.weaponLevel);
        EventBus::Emit("WeaponUpgraded", state.weaponLevel);
    }
}
```

---

## 6) Outer (Meta) Update System

Outer system runs outside combat (run-end / profile / difficulty adaptation).

## 6.1 End-of-run processing

At run end:

- Convert part of score to meta currency.
- Save highest wave, time survived, total kills.
- Update player skill estimate.

```cpp
struct MetaProfile {
    int permanentCurrency;
    int unlockedWeaponsMask;
    float mmrLikeSkill;           // for dynamic baseline tuning
    int highestWaveReached;
};
```

## 6.2 Adaptive baseline difficulty

Adjust next run baseline modestly:

- If player clears target wave too easily: +5% enemy HP/DPS baseline next run.
- If repeatedly fails early: -5% baseline (floored).

Cap adjustments tightly to avoid obvious rubber-banding.

---

## 7) Balance Framework (Enjoyment + Fairness)

Use measurable target bands.

## 7.1 Key metrics

- **TTK Standard Enemy**: target 0.7s–1.2s early, 1.0s–1.8s mid.
- **Player time-to-death under average pressure**: 6s–10s.
- **Upgrade interval**: every 60–120 seconds.
- **Hit feedback frequency**: at least one meaningful feedback event every 2s.

## 7.2 Simulation checks

Create an offline balancer script to run 10k simulated fights:

- Weapon level vs enemy tier survival curves.
- Score income vs upgrade cost progression.
- Wave clear probability by skill bucket.

Tune by changing data tables, not code.

## 7.3 Practical guardrails

- Never increase enemy HP and damage in same wave by large amounts.
- One “stress axis” at a time (count, speed, or durability).
- Keep random variance small for core damage outcomes.

---

## 8) Suggested First-Pass Numbers

- Base player weapon DPS: `100`.
- Standard enemy HP (wave 1): `90`.
- Elite HP (wave 1): `260`.
- Enemy HP growth/wave: `+8%`.
- Enemy DPS growth/wave: `+5%`.
- Score gain growth/wave: `+3%`.
- Weapon level DPS growth: early `+17%`, late `+11%`.

This keeps player power spikes slightly ahead in short windows, then enemy pressure catches up.

---

## 9) Implementation Checklist

1. Define all combat/spawn/progression structs in data assets.
2. Implement deterministic update order.
3. Add death-drop pickup pipeline.
4. Add progression thresholds and upgrade events.
5. Add end-of-run meta processing.
6. Instrument metrics (TTK, deaths, upgrade intervals).
7. Build auto-balance simulation loop.

If all seven are present, you have a full system that is tunable, scalable, and fair.

---

## 10) Production Baseline Data Pack (Ready to Ship v1)

The following files now provide concrete balancing data for a production baseline:

- `docs/balance/weapons.v1.json`
- `docs/balance/enemies.v1.json`
- `docs/balance/spawn_director.v1.json`
- `docs/balance/progression_meta.v1.json`

### 10.1 Practical integration rules

1. Load all `*.v1.json` at startup into immutable runtime config objects.
2. Resolve per-wave stats with:

```text
enemyHP = baseHP * pow(1 + hp_growth_per_wave, waveIndex - 1)
enemyDamage = baseDamage * pow(1 + damage_growth_per_wave, waveIndex - 1)
scoreDrop = baseScore * pow(1 + score_growth_per_wave, waveIndex - 1)
```

3. Resolve weapon level multipliers directly from `level_curve[level]`.
4. Enforce KPI targets in automated nightly simulation (10k seeded matches).

### 10.2 Live-ops tuning policy

- Treat these values as **v1 production defaults**.
- Do small patches (2-6%) instead of large swings.
- Only change one pressure axis per patch (enemy HP, enemy DPS, spawn density, or player power).
- Version subsequent changes as `*.v2.json`, `*.v3.json`, etc. for rollback safety.
