# Weapon animation configuration (five-field layout)

Every weapon now needs **five** animation assets. Locomotion, sprint, crouch, jump, fall and landing
are global character animations and are never configured per weapon.

## Fields

| Field | Type | Used for |
|---|---|---|
| `ThirdPersonAnims.Idle` | `UAnimSequence` | Armed upper-body idle pose (hip and aim, all states) |
| `ThirdPersonAnims.AimOffset` | `UBlendSpace` | Continuous aim-direction correction |
| `ThirdPersonMontages.Equip` | `UAnimMontage` | Equip, first draw and later draws alike |
| `ThirdPersonMontages.Reload` | `UAnimMontage` | Empty **and** tactical reload |
| `ThirdPersonMontages.Fire` | `UAnimMontage` | Fire |

All three montages must author their slot track as **`UpperBody`**; the body graph layers them over
the global locomotion pose. Any field may stay empty — the code falls back safely (no montage, no
aim offset, unarmed idle).

## Non-animation fields (unchanged)

`WeaponType`, `WeaponMesh`, `MeshAttachSocket`, `LeftHandSocket` (FABRIK target), `CombatParams`
(ammo, damage, fire rate), `FireEffect`, `TracerEffect`.

## Adding a weapon

1. Duplicate `/Game/RUINMATES/Blueprint/Data/DA_WeaponDATA` to `DA_<Weapon>`.
2. Set the five animation fields (reuse a same-class weapon's idle/aim offset/equip/fire; usually
   only reload differs).
3. Set mesh, sockets, ammo/damage params and the Niagara effects.
4. Create/duplicate the weapon blueprint and point it at the new data asset.
5. No ABP edit is required.

## What changed in Phase 1H-A

- Removed `AimIdleAnim`, `CrouchIdleAnim`, `SprintAnim`, `AimOffset_Hip`, `AimOffset_Aim`,
  `Strafe_Standing`, `Strafe_Crouching`, `Strafe_Walking`, `FirstEquipMontage`,
  `EmptyReloadMontage`, `TacticalReloadMontage`, the random equip/fire montage arrays and the whole
  `WeaponMontages` (weapon-mesh) set.
- Weapon skeletal meshes no longer play montages; they keep their mesh, skeleton, sockets and muzzle
  calculation. Mechanical parts stay static.
- One equip montage, one reload montage (no reload-type lookup), one fire montage.
- Locomotion uses the character's shared blend spaces; crouch is procedural (pelvis offset).
- Glock18 data was migrated field-by-field **before** the legacy fields were deleted, then saved.

## Duplicate-play protection

`PlayEquippedMontages` refuses to restart an equip, reload or fire montage that is already running.
The host starts a reload once in `StartReloadOnServer`; clients start it once from
`OnRep_WeaponStatus`; equip/fire follow the same notify-based path.
