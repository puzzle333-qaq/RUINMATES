# Phase 1G — reload montage fix and interaction props

Baseline: `5a12a71`. No ABP rebuild, no weapon-system refactor, no economy change.

## Task 01 — reload montage

**Diagnosis (asset audit, read-only commandlet):**

| Finding | Value |
|---|---|
| Character mesh / anim class | `SKM_Quinn_Simple` / `ABP_RuinmatesCombat_C` (compatible skeleton `SK_Mannequin`) |
| `AM_Pistol_Equip` slot | `UpperBody` |
| `AM_Pistol_Reload`, `AM_Pistol_ReloadEmpty`, `AM_Pistol_Fire` slots | `DefaultSlot` (mismatch) |
| Weapon-space reload montages | none assigned (body montage is the only one) |
| Host path | `StartReloadOnServer` played the body montage **and** `TryReloadWeapon` played it again |

The layered slot in the combat graph is `UpperBody` (the working equip montage targets it, and
AGENTS.md defines `UpperBody` as the reload/fire slot). A montage authored on `DefaultSlot` plays
without a matching slot track, so the body never shows it. The duplicate host call restarted the
montage on the frame it started.

**Fixes:**

1. Reload and fire montage assets now author their slot track as `UpperBody` (asset edit, saved in
   place; animation curves untouched).
2. `PlayEquippedMontages` never restarts a reload montage that is already playing (same rule the
   fire montage already used).
3. `TryReloadWeapon` no longer plays the body montage a second time on the host; the single start
   comes from `StartReloadOnServer` (host) or `OnRep_WeaponStatus` (clients). Sprint cancel and
   `WeaponPose->SetReloading(true)` are preserved.
4. Regression test `RuinMates.Combat.ReloadMontageSetup`: both reload montages and the fire montage
   must exist, have length > 0, have a skeleton and contain an `UpperBody` slot track.

Ammo math is unchanged (`FinishReloadOnServer`), and the pose/IK release path still runs at reload
end (`WeaponPose->SetReloading(false)`).

## Task 02 — loot container mesh

- `ARuinmatesLootContainer` gained an optional `VisualMesh` (`UStaticMeshComponent`, Blueprint-visible,
  editable per container blueprint). It is attached to the existing `SearchBounds` root, query-only,
  blocks `Visibility` for aiming, does not generate overlaps and does not affect navigation.
- The search box remains the interaction distance authority, so a visual mesh can never widen the
  trace range; no decorative actor is used for picking.
- Test map containers all use the project's `SM_ChamferCube` scaled to the 80 cm search box.
- Search, reveal timer, loot tables, pickup and replication code were not touched.

## Task 03 — shop

- Created `BP_RuinmatesMerchant` (parent `ARuinmatesMerchant`) and gave its `Body` component the
  project counter mesh; the merchant still exposes `Body` for per-blueprint replacement.
- The test map spawns the blueprint, plus a counter/backdrop block behind it. The merchant `Body`
  remains the only interaction target; the backdrop has no interactable component.
- Interaction prompt and the V4 market flow are unchanged.

## Task 04 — test map

`RuinmatesBuildTestMap` now:

- assigns the crate mesh to every spawned loot container,
- creates `BP_RuinmatesMerchant` on first run (reuses it afterwards) and spawns it,
- keeps all regions, merchant stock and loot configuration, and never touches other maps.

## Task 05 — animation migration readiness (inspection only)

`UWeaponData` already covers the requested per-weapon data:

- `FPlayerAnim`: `IdleAnim`, `AimIdleAnim`, `CrouchIdleAnim`, `SprintAnim`, `AimOffset_Hip`,
  `AimOffset_Aim`, locomotion blend spaces.
- `FMontageData`: `EquipMontage`, `FirstEquipMontage`, `FireMontage`, `TacticalReloadMontage`,
  `EmptyReloadMontage`, plus variant arrays.
- `LeftHandSocket` for FABRIK; `WeaponMesh`/`MeshAttachSocket` for mounting.

Gaps for the later migration: no dedicated heal/interaction montage fields yet, and no per-weapon
"upper body slot name" field (the slot is authored on each montage). Both are additive changes.

## Verification

- `RUINMATESEditor Win64 Development` → Succeeded.
- `Automation RunTests RuinMates` → **42/42 Success**.

## Unverified (needs PIE)

- Standing / moving / aiming reload playback, empty vs tactical reload, host and client both seeing
  the body montage, ammo after reload, pose/IK recovery after reload.
- Crate visibility and aim/interaction at each container; shop visibility and prompt; market and loot
  still working; host/client seeing the same props.

## Handed over

- Fire montage also moved from `DefaultSlot` to `UpperBody` while fixing the reload; if fire should
  stay on a different slot this is a one-line asset change.
- The shop blueprint has no vendor art of its own; it uses prototyping meshes as allowed.
- Interaction prompt/range unchanged; no new NPC AI or dialogue.
