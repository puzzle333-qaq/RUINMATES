# Phase 1F-C combat repair — partial acceptance, 2026-11-19

Baseline: `8b47cdc`. The work is **not fully accepted as playable**: visual and network checks below remain open.

## Actual asset audit

`Phase1FC_AssetAudit.json` records pre-change serialized Blueprint defaults, not inferred C++ values. Read-only tools: `Scripts/audit_combat.py`, `RuinmatesCombatSetup -Inspect`.

- Active character: `/Game/RUINMATES/Blueprint/Character/BP_ThirdPersonCharacter`; mesh `SKM_Quinn_Simple`; original AnimClass `ABP_Unarmed_C`.
- Arm: 400 cm, zero socket/target offsets; camera child location `(180,50,60)`, FOV 90. ADS arm 170 with socket `(0,55,35)` and target `(15,20,55)`. The child forward offset put ADS camera ahead of the arm pivot. It is now zero; hip arm 300, socket `(0,50,60)`. C++ blends arm settings and FOV to ADS 75. Blueprint retains configurable ADS settings.
- Inventory has only `BP_Glock18`. Weapon data: `DA_WeaponDATA`, existing `SK_Handgun`, `GlockSocket`, `FABRIK`. Existing authored mount is preserved. Equip and weapon-mesh montages were empty. Fire/reload body montages existed. No new weapon system was added.
- `AO_Pistol` axes are normalized -1..1; all three samples are mesh-space rotation additive (type 2), on `SK_Mannequin`. The former AimIdle selected an additive AO sample as base pose; now it uses the non-additive pistol Idle.
- `BS_Strafing_Pistol` and `BS_Crouch_Pistol` are 1D **angle** assets, -180..180. Runtime distinguishes these from speed-only 1D assets and direction/speed 2D assets.
- Niagara: `NS_WeaponFire` Direction=Vector3f, Trigger=Bool; `NS_WeaponFire_Tracer` MuzzlePosition=Position, ImpactPositions=Vector array, Trigger=Bool. Actual configured data fields were null; now explicit existing system references are saved. Tracer spawns in world space; flash uses socket-local zero rotation. Visible output still needs acceptance.
- Native aim binding uses IA_Aim with right-mouse fallback. The audit includes serialized IMC_Default. A complete Blueprint event-graph/input conflict audit is still outstanding.

## Changes

- Independent `ABP_RuinmatesCombat`, parent `URuinmatesAnimInstance`, actual connected graph: eight-state Locomotion machine -> spine_01 upper-body branch blend (depth 3, mesh-space rotation) -> FullBody -> FABRIK. Upper branch: weapon pose -> RotationOffsetBlendSpace -> UpperBody Slot. Direct self-variable getter nodes; no legacy owner Property Access paths copied.
- Native animation snapshot uses BaseAimRotation for proxy pitch, normalized to selected offset axes. Existing fallback assets are Blueprint defaults; constructor asset finders removed.
- Left-hand FABRIK uses WeaponPoseComponent's hand_r-space target; reload state, montage activity and optional curve suppress IK. Montage suppression is currently conservative (includes fire); per-action finer control needs gameplay tuning.
- Weapon Montage copies use UpperBody. Equip copy comes from existing MM_Pistol_Equip. Tactical/empty reload use distinct copies of the same available source reload; distinct authored empty-reload movement is not present. Weapon-mesh montage fields remain empty because no compatible clip was validated.
- Server reconstructs fire rays from pawn camera/BaseAimRotation, ignores legacy client RPC view fields, retains muzzle obstruction trace. Developer-only `ruinmates.DebugCombatTrace 1` draws cyan camera/orange muzzle rays; default 0, compiled out of Shipping.
- Server now plays body fire/reload for remote requests; finished reload clears Pose state immediately. Friendly fire reuses Phase 1F-A GameMode 0.25 multiplier and StatsComponent; no duplicate damage implementation.

## Modified assets

- `Content/RUINMATES/Blueprint/Character/BP_ThirdPersonCharacter.uasset`
- New `Content/RUINMATES/Blueprint/Character/ABP_RuinmatesCombat.uasset`
- `Content/RUINMATES/Blueprint/Data/DA_WeaponDATA.uasset`
- New `Content/RUINMATES/Animation/Combat/AM_Pistol_Equip.uasset`
- New `Content/RUINMATES/Animation/Combat/AM_Pistol_Fire.uasset`
- New `Content/RUINMATES/Animation/Combat/AM_Pistol_Reload.uasset`
- New `Content/RUINMATES/Animation/Combat/AM_Pistol_ReloadEmpty.uasset`

Original ABP_Unarmed and original animation assets remain unchanged. LFS tracking retained. Existing user changes to RuinmatesTerminalTypes.cpp and .vsconfig were excluded.

## Evidence and actual acceptance

- Editor build: succeeded (`Saved/CombatCompile.log`).
- AGENTS command `Automation RunFilter RuinMates` did **not** execute tests on this engine: RuinMates is not a valid filter flag. Corrected to `Automation RunTests RuinMates; Quit`.
- Final result: 41 executed, 36 Success, 5 Success with warnings, 0 Fail, 0 NotRun, exit 0. Tracked report: `Docs/Phase1FC_Automation.json`. Warnings include transient-world destruction and null-mesh socket queries in existing tests.
- Independent process loaded, compiled and saved new ABP without Blueprint compiler errors (`Saved/CombatPresentation.log`). Restarted graphical editor rendered it successfully. Interactive graph opening/editing was not exercised.
- Actual rendered standalone PIE captures: `Saved/CombatPlay/01_hip.png` through `08_jump.png`; runtime evidence `Saved/CombatPlay/runtime.json`; log `Saved/CombatPlayCapture.log`. Active AnimClass is new ABP; mesh_hidden=false. Hip and ADS show body and pistol left of centered crosshair. Up/down controller pitch ±45 produced AO inputs ±0.5. Crouch state 4 and airborne state 6 observed. Ammo HUD changed 30/90 -> 29/90 after server shot.
- Initial two-player PIE launched successfully and both players were visible in the viewports, with held pistol pose. This is **not** acceptance of action replication or damage synchronization.
- Captures of fire/reload alone do **not** prove visible tracer/muzzle flash or complete montage playback; timing was too coarse. No claim of these passing.

## Open acceptance work

1. Interactive new ABP open/edit/compile/save/reload cycle; inspect graph layout and every transition. Validate animation dynamics rather than runtime variables alone.
2. Walk/run/sprint, moving aim/fire, standing and moving tactical/empty reload, landing, and sustained left-hand contact. Confirm upper body does not overwrite airborne motion and refine montage IK timing.
3. Observe muzzle flash and tracer over multiple frames on Host/Client; verify socket transform and endpoint against wall obstruction. Existing typed binding is checked, visible output is not.
4. Host/client mutual aim, fire/reload/equip observation; real friendly-fire health and HUD synchronization plus medical use. Automated authority/Stats checks passed; two-player gameplay damage was not exercised.
5. Different weapon switch acceptance needs a second validated existing weapon/data configuration. Current default inventory contains one weapon.
6. Audit all Blueprint camera/input events and asset notifies for legacy dependencies. No full Property Access/reference scan of the old backup was performed.
